// Trava de serie (migracao 012) contra o WORKER INTEIRO
// (src/index.js -> fetch), com SQLite real em memoria e sem rede: as sessoes sao
// semeadas como preparar-local.sh faz, e as verificacoes de token no Trakt, no
// Supabase e no Simkl sao respondidas por um fetch falso. Node 22+:
//   node --test servidor/recomendacoes/teste-trava.mjs
import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { readFileSync } from "node:fs";
import { DatabaseSync } from "node:sqlite";
import test from "node:test";
import worker from "./src/index.js";

const MIGRACOES = ["schema.sql", "migracao-001-avatar-nota.sql", "migracao-002-descobrivel.sql",
  "migracao-003-registro.sql", "migracao-004-triagem.sql", "migracao-005-amigos.sql",
  "migracao-006-social.sql", "migracao-007-resposta.sql", "migracao-008-identidade.sql", "migracao-009-enquete.sql",
  "migracao-012-trava.sql"];

function banco(t) {
  const sqlite = new DatabaseSync(":memory:");
  t.after(() => sqlite.close());
  for (const m of MIGRACOES) {
    // schema.sql ja cria colunas que 001/002 acrescentam: "duplicate column" e
    // esperado, exatamente como em preparar-local.sh. Comando a comando.
    for (const cmd of readFileSync(new URL(m, import.meta.url), "utf8").split(/;\s*\n/)) {
      const c = cmd.replace(/^\s*--.*$/gm, "").trim();
      if (!c) continue;
      try { sqlite.exec(c); } catch (e) { if (!/duplicate column/.test(String(e.message))) throw e; }
    }
  }
  const stmt = (sql, valores) => {
    const st = sqlite.prepare(sql);
    return {
      first: async () => st.get(...valores) ?? null,
      all: async () => ({ results: st.all(...valores) }),
      run: async () => { const r = st.run(...valores);
        return { meta: { changes: Number(r.changes), last_row_id: Number(r.lastInsertRowid) } }; },
      _exec: () => st.run(...valores),
    };
  };
  const DB = {
    prepare(sql) {
      const base = stmt(sql, []);
      return { ...base, bind: (...v) => stmt(sql, v) };
    },
    async batch(lista) {
      sqlite.exec("BEGIN");
      try { for (const s of lista) s._exec(); sqlite.exec("COMMIT"); }
      catch (e) { sqlite.exec("ROLLBACK"); throw e; }
      return [];
    },
  };
  const sessao = (via, token, id, nome) => sqlite.prepare(
    "INSERT INTO sessao (hash, id, nome, expira) VALUES (?, ?, ?, 9999999999)"
  ).run(createHash("sha256").update(`${via}:${token}`).digest("hex"), id, nome);
  return { sqlite, DB, sessao };
}

// Os emissores de mentira. Trakt: token -> slug. Supabase: token -> sub e a
// lista de perfis. Simkl: token -> id da conta.
function emissores(t, { trakt = {}, nuvio = {}, perfis = {}, simkl = {}, perfisFalha = false } = {}) {
  const original = globalThis.fetch;
  globalThis.fetch = async (url, op = {}) => {
    const u = String(url);
    const tok = String(op.headers?.authorization || "").replace("Bearer ", "");
    if (u.startsWith("https://api.trakt.tv/users/settings")) {
      const slug = trakt[tok];
      return slug ? Response.json({ user: { ids: { slug }, name: slug.toUpperCase() } }) : new Response("", { status: 401 });
    }
    if (u === "https://sb.test/auth/v1/user") {
      const sub = nuvio[tok];
      return sub ? Response.json({ id: sub, user_metadata: {} }) : new Response("", { status: 401 });
    }
    if (u === "https://sb.test/rest/v1/rpc/sync_pull_profiles") {
      if (perfisFalha) return new Response("", { status: 500 });
      return Response.json((perfis[tok] || [1]).map((i) => ({ profile_index: i })));
    }
    if (u === "https://api.simkl.com/users/settings") {
      const id = simkl[tok];
      return id ? Response.json({ user: { name: "Simkl " + id }, account: { id } }) : new Response("", { status: 401 });
    }
    throw new Error("rede inesperada no teste: " + u);
  };
  t.after(() => { globalThis.fetch = original; });
}

function cliente(env) {
  // api(via, token, metodo, rota, corpo?, perfil?)
  return async (via, token, metodo, rota, corpo, perfil) => {
    const headers = { authorization: `Bearer ${token}`, "x-nuvio-auth": via };
    if (perfil) headers["x-nuvio-perfil"] = String(perfil);
    const req = new Request("https://w.test" + rota, {
      method: metodo, headers, body: metodo === "POST" ? JSON.stringify(corpo ?? {}) : undefined,
    });
    const res = await worker.fetch(req, env);
    const txt = await res.text();
    let body = null;
    try { body = txt ? JSON.parse(txt) : null; } catch { body = txt; }
    return { status: res.status, body };
  };
}

function cenario(t, opcoes) {
  const b = banco(t);
  emissores(t, opcoes);
  const env = { DB: b.DB, SUPABASE_URL: "https://sb.test", SUPABASE_ANON_KEY: "anon",
                TRAKT_CLIENT_ID: "tc", ...(opcoes?.env || {}) };
  b.sessao("nuvio", "tok-a", "nuvio:aaa", "");
  b.sessao("nuvio", "tok-b", "nuvio:bbb", "Gustavo");
  b.sessao("nuvio", "tok-c", "nuvio:ccc", "Carolina");
  b.sessao("nuvio", "tok-e", "nuvio:eee", "Elisa");
  b.sessao("trakt", "tt-p", "trakt:pedrinho", "Pedrinho");
  return { ...b, env, api: cliente(env) };
}



const A = "nuvio:aaa", A2 = "nuvio:aaa:2", A3 = "nuvio:aaa:3";

test("criar: so perfis da mesma conta, 2 a 8, e quem cria entra", async (t) => {
  const { api } = cenario(t);
  let r = await api("nuvio", "tok-a", "POST", "/v1/trava", { serie: "tt0001", modo: 1, membros: ["nuvio:bbb"] });
  assert.equal(r.status, 400, "amigo de outra conta fica para a Onda 4");
  r = await api("nuvio", "tok-a", "POST", "/v1/trava", { serie: "tt0001", modo: 1, membros: [] });
  assert.equal(r.status, 400, "sozinho nao e trava");
  r = await api("nuvio", "tok-a", "POST", "/v1/trava", { serie: "tt0001:1:2", modo: 1, membros: [A2] });
  assert.equal(r.status, 400, "serie com :T:E e recusada");
  r = await api("trakt", "tt-p", "POST", "/v1/trava", { serie: "tt0001", modo: 1, membros: [A2] });
  assert.equal(r.status, 403, "sem conta Nuvio nao ha perfis");
  r = await api("nuvio", "tok-a", "POST", "/v1/trava",
    { serie: "tt0001", titulo: "Severance", poster: "javascript:x", modo: 1, pin: 1, membros: [A2, A3, A2] });
  assert.equal(r.status, 200);
  assert.match(r.body.id, /^[0-9a-f]{32}$/);
  assert.equal(r.body.trava.poster, "", "poster so https");
  assert.deepEqual(r.body.trava.membros.map((m) => m.pessoa), [A, A2, A3]);
  assert.ok(r.body.trava.membros.every((m) => m.estado === 1 && m.t === 0 && m.e === 0));
});

test("GET /v1/travas: so membros veem; ETag e travaRev em /v1/rec andam juntos", async (t) => {
  const { api, env } = cenario(t);
  const c = await api("nuvio", "tok-a", "POST", "/v1/trava", { serie: "tt0001", modo: 1, membros: [A2] });
  const id = c.body.id;
  const a2 = await api("nuvio", "tok-a", "GET", "/v1/travas", null, 2);
  assert.equal(a2.body.travas.length, 1);
  assert.equal(a2.body.travas[0].id, id);
  const a3 = await api("nuvio", "tok-a", "GET", "/v1/travas", null, 3);
  assert.equal(a3.body.travas.length, 0, "perfil 3 nao e membro: nao ve a posicao de ninguem");
  const b = await api("nuvio", "tok-b", "GET", "/v1/travas");
  assert.equal(b.body.travas.length, 0, "outra conta nao ve nada");

  // ETag: 304 sem mudanca, 200 depois de um passo.
  const req = (etag) => worker.fetch(new Request("https://w.test/v1/travas", {
    headers: { authorization: "Bearer tok-a", "x-nuvio-auth": "nuvio", ...(etag ? { "if-none-match": etag } : {}) } }), env);
  const r1 = await req();
  const et = r1.headers.get("etag");
  assert.equal((await req(et)).status, 304);
  const rec0 = await api("nuvio", "tok-a", "GET", "/v1/rec");
  await api("nuvio", "tok-a", "POST", "/v1/trava/passo", { id, t: 1, e: 1 });
  assert.equal((await req(et)).status, 200, "passo invalida o 304");
  const rec1 = await api("nuvio", "tok-a", "GET", "/v1/rec");
  assert.ok(rec1.body.travaRev > rec0.body.travaRev, "/v1/rec traz a revisao nova");
  const rec2 = await api("nuvio", "tok-a", "GET", "/v1/rec", null, 2);
  assert.equal(rec2.body.travaRev, rec1.body.travaRev, "o outro membro tambem ve a revisao subir");
});

test("passo: so avanca, desfazer volta, Juntos marca outro perfil da conta, nunca de outra conta", async (t) => {
  const { api } = cenario(t);
  const { body: { id } } = await api("nuvio", "tok-a", "POST", "/v1/trava", { serie: "tt0001", modo: 0, membros: [A2] });
  assert.equal((await api("nuvio", "tok-a", "POST", "/v1/trava/passo", { id, t: 1, e: 3 })).body.mudou, 1);
  assert.equal((await api("nuvio", "tok-a", "POST", "/v1/trava/passo", { id, t: 1, e: 2 })).body.mudou, 0);
  assert.equal((await api("nuvio", "tok-a", "POST", "/v1/trava/passo", { id, t: 1, e: 2, desfazer: 1 })).body.mudou, 1);
  // Juntos: a TV do perfil principal anda o perfil 2 tambem.
  assert.equal((await api("nuvio", "tok-a", "POST", "/v1/trava/passo", { id, t: 1, e: 2, pessoa: A2 })).status, 200);
  assert.equal((await api("nuvio", "tok-a", "POST", "/v1/trava/passo", { id, t: 1, e: 2, pessoa: A3 })).status, 404,
    "perfil da conta que nao e membro");
  assert.equal((await api("nuvio", "tok-a", "POST", "/v1/trava/passo", { id, t: 1, e: 2, pessoa: "nuvio:bbb" })).status, 403);
  assert.equal((await api("nuvio", "tok-a", "POST", "/v1/trava/passo", { id, t: 0, e: 2 })).status, 400);
  assert.equal((await api("nuvio", "tok-a", "POST", "/v1/trava/passo", { id, t: 0, e: 0 })).status, 400);
  // Desfazer pode voltar a "nao comecou".
  assert.equal((await api("nuvio", "tok-a", "POST", "/v1/trava/passo", { id, t: 0, e: 0, desfazer: 1, pessoa: A2 })).body.mudou, 1);
  await api("nuvio", "tok-a", "POST", "/v1/trava/passo", { id, t: 1, e: 2, pessoa: A2 });
  const l = await api("nuvio", "tok-a", "GET", "/v1/travas", null, 2);
  const pos = Object.fromEntries(l.body.travas[0].membros.map((m) => [m.pessoa, `${m.t}x${m.e}`]));
  assert.deepEqual(pos, { [A]: "1x2", [A2]: "1x2" });
  // Outra conta nao mexe numa trava que nao e dela.
  assert.equal((await api("nuvio", "tok-b", "POST", "/v1/trava/passo", { id, t: 5, e: 5 })).status, 404);
});

test("gestao: modo, pin, pausar, membros, sair e apagar", async (t) => {
  const { api } = cenario(t);
  const { body: { id } } = await api("nuvio", "tok-a", "POST", "/v1/trava", { serie: "tt0001", modo: 1, membros: [A2, A3] });
  assert.equal((await api("nuvio", "tok-a", "POST", "/v1/trava/modo", { id, modo: 0 })).body.trava.modo, 0);
  assert.equal((await api("nuvio", "tok-a", "POST", "/v1/trava/pin", { id, pin: 1 })).body.trava.pin, 1);
  assert.equal((await api("nuvio", "tok-a", "POST", "/v1/trava/pausar", { id, pausada: 1 })).body.trava.pausada, 1);
  let r = await api("nuvio", "tok-a", "POST", "/v1/trava/membros", { id, membros: [A3] });
  assert.deepEqual(r.body.trava.membros.map((m) => m.pessoa), [A, A3]);
  assert.equal((await api("nuvio", "tok-a", "GET", "/v1/travas", null, 2)).body.travas.length, 0, "quem saiu nao ve mais");
  r = await api("nuvio", "tok-a", "POST", "/v1/trava/membros", { id, membros: [A2, A3] });
  assert.equal(r.body.trava.membros.length, 3, "voltar a entrar reativa");
  // Sair: o perfil 3 sai; sobram dois.
  r = await api("nuvio", "tok-a", "POST", "/v1/trava/sair", { id }, 3);
  assert.equal(r.status, 200);
  assert.equal((await api("nuvio", "tok-a", "GET", "/v1/travas", null, 3)).body.travas.length, 0);
  // Sair com dois: a trava acaba para todos.
  r = await api("nuvio", "tok-a", "POST", "/v1/trava/sair", { id }, 2);
  assert.equal(r.body.apagada, 1);
  assert.equal((await api("nuvio", "tok-a", "GET", "/v1/travas")).body.travas.length, 0);
  // Apagar.
  const n = await api("nuvio", "tok-a", "POST", "/v1/trava", { serie: "tt0002", modo: 1, membros: [A2] });
  assert.equal((await api("nuvio", "tok-b", "POST", "/v1/trava/apagar", { id: n.body.id })).status, 404);
  assert.equal((await api("nuvio", "tok-a", "POST", "/v1/trava/apagar", { id: n.body.id }, 2)).body.apagada, 1);
});

test("limites: 30 travas ativas por pessoa", async (t) => {
  const { api, sqlite } = cenario(t);
  for (let i = 0; i < 30; i++)
    sqlite.prepare("INSERT INTO trava_membro (trava, pessoa, estado) VALUES (?, ?, 1)").run("x" + i, A);
  const r = await api("nuvio", "tok-a", "POST", "/v1/trava", { serie: "tt0001", modo: 1, membros: [A2] });
  assert.equal(r.status, 429);
});
