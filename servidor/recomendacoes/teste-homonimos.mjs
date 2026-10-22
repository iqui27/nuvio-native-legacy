// #202 "nomes repetidos" na busca de pessoas: a mesma pessoa publicada sob duas
// identidades (outro perfil da casa, ou Trakt + conta Nuvio) aparece UMA vez;
// pessoas diferentes com o mesmo apelido continuam todas. Worker inteiro com
// SQLite real em memoria, como teste-identidade.mjs. Node 22+:
//   node --test servidor/recomendacoes/teste-homonimos.mjs
import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { readFileSync } from "node:fs";
import { DatabaseSync } from "node:sqlite";
import test from "node:test";
import worker from "./src/index.js";

const MIGRACOES = ["schema.sql", "migracao-001-avatar-nota.sql", "migracao-002-descobrivel.sql",
  "migracao-003-registro.sql", "migracao-004-triagem.sql", "migracao-005-amigos.sql",
  "migracao-006-social.sql", "migracao-007-resposta.sql", "migracao-008-identidade.sql"];

function cenario(t) {
  const sqlite = new DatabaseSync(":memory:");
  t.after(() => sqlite.close());
  for (const m of MIGRACOES) {
    for (const cmd of readFileSync(new URL(m, import.meta.url), "utf8").split(/;\s*\n/)) {
      const c = cmd.replace(/^\s*--.*$/gm, "").trim();
      if (!c) continue;
      try { sqlite.exec(c); } catch (e) { if (!/duplicate column/.test(String(e.message))) throw e; }
    }
  }
  const stmt = (sql, v) => {
    const st = sqlite.prepare(sql);
    return {
      first: async () => st.get(...v) ?? null,
      all: async () => ({ results: st.all(...v) }),
      run: async () => { const r = st.run(...v);
        return { meta: { changes: Number(r.changes), last_row_id: Number(r.lastInsertRowid) } }; },
      _exec: () => st.run(...v),
    };
  };
  const DB = {
    prepare(sql) { return { ...stmt(sql, []), bind: (...v) => stmt(sql, v) }; },
    async batch(lista) {
      sqlite.exec("BEGIN");
      try { for (const s of lista) s._exec(); sqlite.exec("COMMIT"); }
      catch (e) { sqlite.exec("ROLLBACK"); throw e; }
      return [];
    },
  };
  const sessao = (via, token, id) => sqlite.prepare(
    "INSERT INTO sessao (hash, id, nome, expira) VALUES (?, ?, '', 9999999999)"
  ).run(createHash("sha256").update(`${via}:${token}`).digest("hex"), id);
  sessao("nuvio", "tok-a", "nuvio:aaa");     // conta com dois perfis
  sessao("nuvio", "tok-b", "nuvio:bbb");     // quem procura
  sessao("nuvio", "tok-c", "nuvio:ccc");     // familia: dois perfis, mesmo apelido
  sessao("nuvio", "tok-e", "nuvio:eee");
  sessao("trakt", "tt-e", "trakt:elisa");    // o Trakt da eee (ligado abaixo)
  sessao("trakt", "tt-x", "trakt:outra");    // outro Trakt, sem vinculo
  const original = globalThis.fetch;
  globalThis.fetch = async (url) => {
    if (String(url).endsWith("/rest/v1/rpc/sync_pull_profiles"))
      return Response.json([1, 2].map((i) => ({ profile_index: i })));
    throw new Error("rede inesperada no teste: " + url);
  };
  t.after(() => { globalThis.fetch = original; });
  const env = { DB, SUPABASE_URL: "https://sb.test", SUPABASE_ANON_KEY: "anon", TRAKT_CLIENT_ID: "tc" };
  const api = async (via, token, rota, corpo, perfil) => {
    const headers = { authorization: `Bearer ${token}`, "x-nuvio-auth": via };
    if (perfil) headers["x-nuvio-perfil"] = String(perfil);
    const res = await worker.fetch(new Request("https://w.test" + rota, {
      method: "POST", headers, body: JSON.stringify(corpo ?? {}) }), env);
    return { status: res.status, body: await res.json() };
  };
  return { sqlite, api };
}

const apelidos = (lista) => lista.map((x) => x.apelido).sort();

async function publicarTodos(api) {
  const perfil = { apelido: "marina", bio: "so terror", generos: ["terror"], recentes: 1 };
  for (const [via, tok, n] of [["nuvio", "tok-a"], ["nuvio", "tok-a", 1], ["nuvio", "tok-b"],
    ["nuvio", "tok-c"], ["nuvio", "tok-c", 2], ["nuvio", "tok-e"], ["trakt", "tt-e"], ["trakt", "tt-x"]])
    assert.equal((await api(via, tok, "/v1/eu", {}, n)).status, 200);
  // A COPIA DO BUG: o mesmo "Meu perfil" republicado no perfil 1 da conta aaa.
  await api("nuvio", "tok-a", "/v1/perfil", perfil);
  await api("nuvio", "tok-a", "/v1/perfil", perfil, 1);
  // Familia: dois perfis da conta ccc escolheram o mesmo apelido, cada um o seu.
  await api("nuvio", "tok-c", "/v1/perfil", { apelido: "marina", bio: "mae" });
  await api("nuvio", "tok-c", "/v1/perfil", { apelido: "marina", bio: "filha" }, 2);
  // A mesma pessoa no Trakt (verificado) e na conta Nuvio; e uma estranha no Trakt.
  await api("nuvio", "tok-e", "/v1/perfil", { apelido: "marina", bio: "eu" });
  await api("trakt", "tt-e", "/v1/perfil", { apelido: "marina", bio: "eu no trakt" });
  await api("trakt", "tt-x", "/v1/perfil", { apelido: "marina", bio: "eu" });
  await api("nuvio", "tok-b", "/v1/perfil", { apelido: "bia" });
}

test("busca: copia da mesma pessoa some, homonimos de verdade ficam", async (t) => {
  const { sqlite, api } = cenario(t);
  await publicarTodos(api);
  sqlite.prepare("INSERT INTO identidade (provedor, sujeito, pessoa, metodo, verificado, criado) " +
    "VALUES ('trakt', 'elisa', 'nuvio:eee', 'token', 1, 1)").run();
  const r = await api("nuvio", "tok-b", "/v1/perfis/buscar", { q: "marina" });
  assert.equal(r.status, 200);
  const bios = r.body.resultados.map((x) => x.bio).sort();
  // aaa (uma vez so), ccc mae, ccc filha, eee (uma vez so: o trakt ligado some),
  // e o Trakt sem vinculo, que e outra pessoa ate provar o contrario.
  assert.deepEqual(bios, ["eu", "eu", "filha", "mae", "so terror"]);
  assert.equal(new Set(r.body.resultados.map((x) => x.pub)).size, 5);
  // a copia que fica e a do perfil principal
  const pubA = sqlite.prepare("SELECT pub FROM perfil WHERE pessoa = 'nuvio:aaa'").get().pub;
  assert.ok(r.body.resultados.some((x) => x.pub === pubA));
});

test("identidade declarada (nao verificada) nao funde", async (t) => {
  const { sqlite, api } = cenario(t);
  await publicarTodos(api);
  sqlite.prepare("INSERT INTO identidade (provedor, sujeito, pessoa, metodo, verificado, criado) " +
    "VALUES ('trakt', 'elisa', 'nuvio:eee', 'declarado', 0, 1)").run();
  const r = await api("nuvio", "tok-b", "/v1/perfis/buscar", { q: "marina" });
  assert.equal(r.body.resultados.length, 6);
});

test("fica a copia com quem ja sou amigo", async (t) => {
  const { sqlite, api } = cenario(t);
  await publicarTodos(api);
  for (const [a, b] of [["nuvio:bbb", "nuvio:aaa:1"], ["nuvio:aaa:1", "nuvio:bbb"]])
    sqlite.prepare("INSERT INTO contato (a, b, criado) VALUES (?, ?, 1)").run(a, b);
  const r = await api("nuvio", "tok-b", "/v1/perfis/buscar", { q: "marina" });
  const terror = r.body.resultados.filter((x) => x.bio === "so terror");
  assert.equal(terror.length, 1);
  assert.equal(terror[0].relacao, "amigo");
});

test("comunidade tambem lista a pessoa uma vez so", async (t) => {
  const { api } = cenario(t);
  await publicarTodos(api);
  const r = await api("nuvio", "tok-b", "/v1/perfis/comunidade", {});
  assert.equal(r.status, 200);
  // sem vinculo Trakt neste teste: aaa x1, ccc x2, eee, trakt:elisa, trakt:outra
  assert.deepEqual(apelidos(r.body.pessoas), Array(6).fill("marina"));
  assert.equal(r.body.pessoas.filter((x) => x.bio === "so terror").length, 1);
});

test("gosto parecido: a copia nao conta como outra sugestao", async (t) => {
  const { sqlite, api } = cenario(t);
  await publicarTodos(api);
  const imdbs = ["tt0000001", "tt0000002", "tt0000003"];
  for (const n of [undefined, 1]) {
    await api("nuvio", "tok-a", "/v1/perfil/atividade", { nivel: 1 }, n);
    for (const imdb of imdbs) await api("nuvio", "tok-a", "/v1/atividade", { imdb, titulo: imdb }, n);
  }
  const r = await api("nuvio", "tok-b", "/v1/perfis/sugeridos", { imdbs });
  assert.equal(r.status, 200);
  // Desde 8d36a7f0 o gosto parecido tambem sugere quem e ativo ou tem os
  // mesmos generos (as outras "marina", pessoas distintas). O que importa aqui:
  // a COPIA da conta aaa nao vira uma segunda sugestao, e a que fica e a principal.
  const terror = r.body.sugeridos.filter((x) => x.bio === "so terror");
  assert.equal(terror.length, 1);
  assert.equal(terror[0].motivo.tipo, "titulos");
  assert.equal(terror[0].pub, sqlite.prepare("SELECT pub FROM perfil WHERE pessoa = 'nuvio:aaa'").get().pub);
});
