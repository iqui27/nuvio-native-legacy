// Identidade social unificada (F08, migracao 008) contra o WORKER INTEIRO
// (src/index.js -> fetch), com SQLite real em memoria e sem rede: as sessoes sao
// semeadas como preparar-local.sh faz, e as verificacoes de token no Trakt, no
// Supabase e no Simkl sao respondidas por um fetch falso. Node 22+:
//   node --test servidor/recomendacoes/teste-identidade.mjs
import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { readFileSync } from "node:fs";
import { DatabaseSync } from "node:sqlite";
import test from "node:test";
import worker from "./src/index.js";

const MIGRACOES = ["schema.sql", "migracao-001-avatar-nota.sql", "migracao-002-descobrivel.sql",
  "migracao-003-registro.sql", "migracao-004-triagem.sql", "migracao-005-amigos.sql",
  "migracao-006-social.sql", "migracao-007-resposta.sql", "migracao-008-identidade.sql"];

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
    if (u.startsWith("https://api.simkl.com/users/settings")) {
      // A Simkl exige client_id/app-name/app-version na query; sem eles, 412.
      const q = new URL(u).searchParams;
      if (!q.get("client_id") || !q.get("app-name") || !q.get("app-version")) return new Response("", { status: 412 });
      if (simkl.__http) return new Response("", { status: simkl.__http });
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

const q = (sqlite, sql, ...v) => sqlite.prepare(sql).all(...v);

test("/v1/eu anuncia o recurso e as identidades (deteccao do cliente)", async (t) => {
  const { api } = cenario(t);
  const r = await api("nuvio", "tok-a", "POST", "/v1/eu");
  assert.equal(r.status, 200);
  assert.deepEqual(r.body.recursos, ["identidade1"]);
  assert.deepEqual(r.body.identidades, []);
  assert.equal(r.body.autenticado, "nuvio");
});

test("vincular pelo Trakt com a prova Nuvio funde a pessoa trakt no perfil e preserva tudo", async (t) => {
  const { sqlite, api } = cenario(t, { nuvio: { "sb-a": "aaa" } });
  await api("nuvio", "tok-a", "POST", "/v1/eu", { nome: "Henrique" });
  await api("nuvio", "tok-b", "POST", "/v1/eu", { nome: "Gustavo" });
  await api("trakt", "tt-p", "POST", "/v1/eu");
  // A vida do Pedrinho no Trakt antes do vinculo: um amigo (bbb), uma rec
  // recebida, alcance 2, perfil publico, um evento.
  const cod = q(sqlite, "SELECT codigo FROM pessoa WHERE id = 'nuvio:bbb'")[0].codigo;
  assert.equal((await api("trakt", "tt-p", "POST", "/v1/contatos", { codigo: cod })).status, 200);
  assert.equal((await api("trakt", "tt-p", "POST", "/v1/alcance", { nivel: 2 })).body.alcance, 2);
  await api("trakt", "tt-p", "POST", "/v1/atividade", { ev: "fim", imdb: "tt0000001", titulo: "A" });
  await api("nuvio", "tok-b", "POST", "/v1/rec", { para: "trakt:pedrinho", imdb: "tt0000002" });
  await api("trakt", "tt-p", "POST", "/v1/descobrivel", { descobrivel: 1 });
  const pubAntes = q(sqlite, "SELECT pub FROM perfil WHERE pessoa = 'trakt:pedrinho'")[0].pub;

  const v = await api("trakt", "tt-p", "POST", "/v1/identidades/vincular", { provedor: "nuvio", token: "sb-a" });
  assert.equal(v.status, 200, JSON.stringify(v.body));
  assert.deepEqual([v.body.pessoa, v.body.fundiu], ["nuvio:aaa", 1]);

  assert.equal(q(sqlite, "SELECT COUNT(*) AS n FROM pessoa WHERE id = 'trakt:pedrinho'")[0].n, 0);
  assert.deepEqual(q(sqlite, "SELECT b FROM contato WHERE a = 'nuvio:aaa'").map((x) => x.b), ["nuvio:bbb"]);
  assert.deepEqual(q(sqlite, "SELECT b FROM contato WHERE a = 'nuvio:bbb'").map((x) => x.b), ["nuvio:aaa"]);
  assert.equal(q(sqlite, "SELECT para FROM rec")[0].para, "nuvio:aaa");
  assert.equal(q(sqlite, "SELECT pessoa FROM evento")[0].pessoa, "nuvio:aaa");
  assert.equal(q(sqlite, "SELECT alcance FROM pessoa WHERE id = 'nuvio:aaa'")[0].alcance, 2,
    "quem nunca respondeu adota a resposta do outro lado");
  assert.equal(q(sqlite, "SELECT pub FROM perfil WHERE pessoa = 'nuvio:aaa'")[0].pub, pubAntes, "handle preservado");
  assert.equal(q(sqlite, "SELECT para FROM fusao WHERE de = 'trakt:pedrinho'")[0].para, "nuvio:aaa");

  // TV antiga, pelo Trakt: agora fala como o perfil.
  const eu = await api("trakt", "tt-p", "POST", "/v1/eu");
  assert.equal(eu.body.id, "nuvio:aaa");
  assert.equal(eu.body.autenticado, "trakt");
  assert.deepEqual(eu.body.identidades.map((x) => [x.provedor, x.sujeito, x.verificado]), [["trakt", "pedrinho", 1]]);
  assert.equal(eu.body.nome, "Henrique", "o nome do Trakt nao sobrescreve o do perfil");

  // O amigo ve um contato so, com o id ligado para juntar o feed do Trakt.
  let ct = (await api("nuvio", "tok-b", "GET", "/v1/contatos")).body.contatos;
  assert.deepEqual(ct.map((c) => [c.id, c.ids]), [["nuvio:aaa", ["trakt:pedrinho"]]]);
  assert.equal((await api("nuvio", "tok-a", "POST", "/v1/identidades/visivel", { provedor: "trakt", visivel: 0 })).body.n, 1);
  ct = (await api("nuvio", "tok-b", "GET", "/v1/contatos")).body.contatos;
  assert.deepEqual(ct[0].ids, [], "so eu: o id ligado nao sai");

  // TV antiga do amigo ainda tem `trakt:pedrinho` guardado.
  const rec = await api("nuvio", "tok-b", "POST", "/v1/rec", { para: "trakt:pedrinho", imdb: "tt0000003" });
  assert.equal(rec.status, 200);
  assert.equal(q(sqlite, "SELECT para FROM rec WHERE imdb = 'tt0000003'")[0].para, "nuvio:aaa");
  const perf = await api("nuvio", "tok-b", "GET", "/v1/amigo?id=trakt:pedrinho");
  assert.equal(perf.status, 200);
  assert.equal(perf.body.id, "nuvio:aaa");

  // Repetir o vinculo e inofensivo.
  const de2 = await api("trakt", "tt-p", "POST", "/v1/identidades/vincular", { provedor: "nuvio", token: "sb-a" });
  assert.deepEqual([de2.status, de2.body.ja], [200, 1]);
});

test("conflitos: identidade de outro perfil, segunda conta do mesmo servico, token falso", async (t) => {
  const { api } = cenario(t, { trakt: { "tk-p": "pedrinho", "tk-x": "outro" }, nuvio: { "sb-a": "aaa" } });
  await api("trakt", "tt-p", "POST", "/v1/eu");
  assert.equal((await api("trakt", "tt-p", "POST", "/v1/identidades/vincular", { provedor: "nuvio", token: "sb-a" })).status, 200);
  // Carolina tenta ligar o mesmo Trakt (pela conta Nuvio dela).
  const c = await api("nuvio", "tok-c", "POST", "/v1/identidades/vincular", { provedor: "trakt", token: "tk-p" });
  assert.equal(c.status, 409);
  // Henrique tenta uma SEGUNDA conta Trakt.
  const a = await api("nuvio", "tok-a", "POST", "/v1/identidades/vincular", { provedor: "trakt", token: "tk-x" });
  assert.equal(a.status, 409);
  assert.equal((await api("nuvio", "tok-c", "POST", "/v1/identidades/vincular", { provedor: "trakt", token: "falso" })).status, 401);
  assert.equal((await api("nuvio", "tok-c", "POST", "/v1/identidades/vincular", { provedor: "nuvio", token: "sb-a" })).status, 400,
    "conta Nuvio nao vincula outra conta Nuvio");
  assert.equal((await api("nuvio", "tok-c", "POST", "/v1/identidades/vincular", { provedor: "orkut" })).status, 400);
});

test("pela conta Nuvio: ligar um Trakt sem pessoa antiga nao funde nada", async (t) => {
  const { sqlite, api } = cenario(t, { trakt: { "tk-n": "novo" } });
  await api("nuvio", "tok-c", "POST", "/v1/eu");
  const r = await api("nuvio", "tok-c", "POST", "/v1/identidades/vincular", { provedor: "trakt", token: "tk-n" });
  assert.deepEqual([r.status, r.body.fundiu, r.body.pessoa], [200, 0, "nuvio:ccc"]);
  assert.equal(q(sqlite, "SELECT COUNT(*) AS n FROM fusao")[0].n, 0);
});

test("privacidade na fusao: se os dois responderam, fica o nivel mais fechado; bloqueio vence contato", async (t) => {
  const { sqlite, api } = cenario(t, { nuvio: { "sb-a": "aaa" } });
  for (const tk of ["tok-a", "tok-e"]) await api("nuvio", tk, "POST", "/v1/eu");
  await api("trakt", "tt-p", "POST", "/v1/eu");
  await api("nuvio", "tok-a", "POST", "/v1/alcance", { nivel: 1 });
  await api("trakt", "tt-p", "POST", "/v1/alcance", { nivel: 2 });
  // Henrique (Nuvio) e amigo da Elisa; o Pedrinho (Trakt) bloqueou a Elisa.
  const codE = q(sqlite, "SELECT codigo FROM pessoa WHERE id = 'nuvio:eee'")[0].codigo;
  await api("nuvio", "tok-a", "POST", "/v1/contatos", { codigo: codE });
  sqlite.prepare("INSERT INTO bloqueio (quem, alvo, criado) VALUES ('trakt:pedrinho', 'nuvio:eee', 1)").run();
  assert.equal((await api("trakt", "tt-p", "POST", "/v1/identidades/vincular", { provedor: "nuvio", token: "sb-a" })).status, 200);
  assert.equal(q(sqlite, "SELECT alcance FROM pessoa WHERE id = 'nuvio:aaa'")[0].alcance, 1);
  assert.equal(q(sqlite, "SELECT COUNT(*) AS n FROM contato WHERE a = 'nuvio:aaa' OR b = 'nuvio:aaa'")[0].n, 0);
  assert.equal(q(sqlite, "SELECT quem FROM bloqueio WHERE alvo = 'nuvio:eee'")[0].quem, "nuvio:aaa");
});

test("desvincular nao separa o passado; o Trakt volta a ser uma pessoa nova", async (t) => {
  const { sqlite, api } = cenario(t, { nuvio: { "sb-a": "aaa" } });
  await api("trakt", "tt-p", "POST", "/v1/eu");
  await api("trakt", "tt-p", "POST", "/v1/alcance", { nivel: 1 });
  await api("trakt", "tt-p", "POST", "/v1/atividade", { ev: "fim", imdb: "tt0000001" });
  await api("trakt", "tt-p", "POST", "/v1/identidades/vincular", { provedor: "nuvio", token: "sb-a" });
  assert.equal((await api("nuvio", "tok-a", "POST", "/v1/identidades/desvincular", { provedor: "trakt" })).body.n, 1);
  const eu = await api("trakt", "tt-p", "POST", "/v1/eu");
  assert.equal(eu.body.id, "trakt:pedrinho");
  assert.equal(q(sqlite, "SELECT pessoa FROM evento")[0].pessoa, "nuvio:aaa");
});

test("perfil nao principal: o indice e conferido no Supabase com o mesmo token, e falha fechado", async (t) => {
  {
    const { api } = cenario(t, { nuvio: { "sb-a": "aaa" }, perfis: { "sb-a": [1, 2] } });
    await api("trakt", "tt-p", "POST", "/v1/eu");
    assert.equal((await api("trakt", "tt-p", "POST", "/v1/identidades/vincular",
      { provedor: "nuvio", token: "sb-a", perfil: 5 })).status, 403);
    const ok = await api("trakt", "tt-p", "POST", "/v1/identidades/vincular", { provedor: "nuvio", token: "sb-a", perfil: 2 });
    assert.deepEqual([ok.status, ok.body.pessoa], [200, "nuvio:aaa:2"]);
    assert.equal((await api("trakt", "tt-p", "POST", "/v1/eu")).body.id, "nuvio:aaa:2");
  }
  {
    const { api } = cenario(t, { nuvio: { "sb-a": "aaa" }, perfisFalha: true });
    await api("trakt", "tt-p", "POST", "/v1/eu");
    assert.equal((await api("trakt", "tt-p", "POST", "/v1/identidades/vincular",
      { provedor: "nuvio", token: "sb-a", perfil: 2 })).status, 503);
  }
});

test("Letterboxd fica declarado (nunca funde nem sai); Simkl exige client id e prova", async (t) => {
  const { sqlite, api } = cenario(t, { simkl: { "sk-1": 4242 }, env: {} });
  await api("nuvio", "tok-a", "POST", "/v1/eu");
  await api("nuvio", "tok-c", "POST", "/v1/eu");
  for (const tk of ["tok-a", "tok-c"]) {
    const r = await api("nuvio", tk, "POST", "/v1/identidades/vincular", { provedor: "letterboxd", usuario: "Dave" });
    assert.deepEqual([r.status, r.body.verificado], [200, 0], "dois perfis podem declarar o mesmo usuario");
  }
  assert.equal(q(sqlite, "SELECT COUNT(*) AS n FROM identidade WHERE provedor = 'letterboxd' AND verificado = 0")[0].n, 2);
  assert.equal((await api("nuvio", "tok-a", "POST", "/v1/identidades/vincular", { provedor: "simkl", token: "sk-1" })).status, 501);
});

test("Simkl com client id: id da conta provado pela API", async (t) => {
  const { api } = cenario(t, { simkl: { "sk-1": 4242 }, env: { SIMKL_CLIENT_ID: "sc" } });
  await api("nuvio", "tok-a", "POST", "/v1/eu");
  assert.equal((await api("nuvio", "tok-a", "POST", "/v1/identidades/vincular", { provedor: "simkl", token: "nao" })).status, 401);
  const r = await api("nuvio", "tok-a", "POST", "/v1/identidades/vincular", { provedor: "simkl", token: "sk-1" });
  assert.equal(r.status, 200);
  const l = await api("nuvio", "tok-a", "GET", "/v1/identidades");
  assert.deepEqual(l.body.identidades.map((x) => [x.provedor, x.sujeito, x.metodo]), [["simkl", "4242", "token"]]);
});

test("feed limpo: de uma serie so sai o PRIMEIRO comecou, numa linha so", async (t) => {
  const { sqlite, api } = cenario(t);
  for (const tk of ["tok-a", "tok-b"]) await api("nuvio", tk, "POST", "/v1/eu");
  const codB = q(sqlite, "SELECT codigo FROM pessoa WHERE id = 'nuvio:bbb'")[0].codigo;
  await api("nuvio", "tok-a", "POST", "/v1/contatos", { codigo: codB });
  await api("nuvio", "tok-b", "POST", "/v1/alcance", { nivel: 1 });
  const agora = Math.floor(Date.now() / 1000);
  const ins = sqlite.prepare("INSERT INTO evento (pessoa, ev, imdb, midia, temporada, episodio, criado) VALUES ('nuvio:bbb', 'inicio', 'tt9', 'series', 1, ?, ?)");
  ins.run(3, agora - 9000);   // horas antes: outro fato
  ins.run(3, agora - 1200);   // TV 1
  ins.run(3, agora - 60);     // TV 2, mesmo episodio
  ins.run(4, agora - 30);     // episodio seguinte
  const f = await api("nuvio", "tok-a", "GET", "/v1/feed?desde=0");
  // 06/10/2026: retomar o episodio (outra TV) e o episodio seguinte nao sao
  // noticia; o comeco da serie, horas antes, e a unica linha.
  assert.deepEqual(f.body.itens.map((x) => [x.ev, x.episodio, agora - x.criado]), [["inicio", 3, 9000]]);
});

test("comparacao com cobertura: por midia, em comum e o tamanho da amostra", async (t) => {
  const { sqlite, api } = cenario(t);
  for (const tk of ["tok-a", "tok-b"]) await api("nuvio", tk, "POST", "/v1/eu");
  const codB = q(sqlite, "SELECT codigo FROM pessoa WHERE id = 'nuvio:bbb'")[0].codigo;
  await api("nuvio", "tok-a", "POST", "/v1/contatos", { codigo: codB });
  for (const tk of ["tok-a", "tok-b"]) await api("nuvio", tk, "POST", "/v1/alcance", { nivel: 1 });
  const ev = (tk, c) => api("nuvio", tk, "POST", "/v1/atividade", c);
  await ev("tok-a", { ev: "reacao", imdb: "tt1", reacao: 1 });
  await ev("tok-b", { ev: "reacao", imdb: "tt1", reacao: 1 });
  await ev("tok-a", { ev: "reacao", imdb: "tt2", reacao: 1, midia: "series" });
  await ev("tok-b", { ev: "reacao", imdb: "tt2", reacao: -1, midia: "series" });
  await ev("tok-a", { ev: "reacao", imdb: "tt3", reacao: 1 });            // so eu
  await ev("tok-a", { ev: "fim", imdb: "tt7" });
  await ev("tok-b", { ev: "fim", imdb: "tt7" });
  await ev("tok-a", { ev: "fim", imdb: "tt8", midia: "series", temporada: 1, episodio: 1 });
  await ev("tok-b", { ev: "fim", imdb: "tt8", midia: "series", temporada: 2, episodio: 5 });
  const g = (await api("nuvio", "tok-a", "GET", "/v1/amigo?id=nuvio:bbb")).body.gosto;
  assert.deepEqual([g.total, g.iguais, g.pct], [2, 1, 50]);
  assert.deepEqual(g.filmes, { total: 1, iguais: 1 });
  assert.deepEqual(g.series, { total: 1, iguais: 0 });
  assert.deepEqual(g.comum, { filmes: 1, series: 1 });
  assert.deepEqual(g.cobertura, { eu: 3, ele: 2 });
  assert.equal(g.generos, null, "generos: sem fonte, nunca um chute");
});

test("Simkl: 412/5xx do emissor nao e token recusado (502, nao 401)", async (t) => {
  const { api } = cenario(t, { simkl: { "sk-1": 4242, __http: 412 }, env: { SIMKL_CLIENT_ID: "sc" } });
  await api("nuvio", "tok-a", "POST", "/v1/eu");
  assert.equal((await api("nuvio", "tok-a", "POST", "/v1/identidades/vincular", { provedor: "simkl", token: "sk-1" })).status, 502);
});
