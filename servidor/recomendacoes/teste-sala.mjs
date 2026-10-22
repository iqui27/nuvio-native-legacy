// Watch Together no nuvio-recomendacoes (src/sala.js, migracao 010) contra o
// WORKER INTEIRO, com SQLite real em memoria e sem rede (mesmo arranjo de
// teste-enquete.mjs). Node 22+:
//   node --test servidor/recomendacoes/teste-sala.mjs
import assert from "node:assert/strict";
import { createHash } from "node:crypto";
import { readFileSync } from "node:fs";
import { DatabaseSync } from "node:sqlite";
import test from "node:test";
import worker from "./src/index.js";
import { conferir } from "../juntos/src/bilhete.js";

const MIGRACOES = ["schema.sql", "migracao-001-avatar-nota.sql", "migracao-002-descobrivel.sql",
  "migracao-003-registro.sql", "migracao-004-triagem.sql", "migracao-005-amigos.sql",
  "migracao-006-social.sql", "migracao-007-resposta.sql", "migracao-008-identidade.sql",
  "migracao-009-enquete.sql", "migracao-010-juntos.sql"];
const SEGREDO = "s3gr3d0";

function banco(t) {
  const sqlite = new DatabaseSync(":memory:");
  t.after(() => sqlite.close());
  for (const m of MIGRACOES) {
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
    prepare(sql) { const base = stmt(sql, []); return { ...base, bind: (...v) => stmt(sql, v) }; },
    async batch(lista) {
      sqlite.exec("BEGIN");
      try { for (const s of lista) s._exec(); sqlite.exec("COMMIT"); } catch (e) { sqlite.exec("ROLLBACK"); throw e; }
      return [];
    },
  };
  const sessao = (token, id, nome) => sqlite.prepare("INSERT INTO sessao (hash, id, nome, expira) VALUES (?, ?, ?, 9999999999)")
    .run(createHash("sha256").update(`nuvio:${token}`).digest("hex"), id, nome);
  return { sqlite, DB, sessao };
}

function cenario(t, env = {}) {
  const b = banco(t);
  const original = globalThis.fetch;
  globalThis.fetch = async (u) => { throw new Error("rede inesperada: " + u); };
  t.after(() => { globalThis.fetch = original; });
  for (const [tok, id, nome] of [["tok-a", "nuvio:aaa", "Henrique"], ["tok-b", "nuvio:bbb", "Rafa"],
    ["tok-c", "nuvio:ccc", "Fabi"], ["tok-e", "nuvio:eee", "Estranho"]]) b.sessao(tok, id, nome);
  const amigos = (x, y) => {
    for (const [a, c] of [[x, y], [y, x]]) b.sqlite.prepare("INSERT INTO contato (a, b, criado, via) VALUES (?, ?, 1, 'pedido')").run(a, c);
  };
  const e = { DB: b.DB, SUPABASE_URL: "https://sb.test", SUPABASE_ANON_KEY: "anon", TRAKT_CLIENT_ID: "tc",
    SALA_SEGREDO: SEGREDO, JUNTOS_URL: "https://juntos.test", ...env };
  const api = async (token, metodo, rota, corpo, perfil) => {
    const headers = { authorization: `Bearer ${token}`, "x-nuvio-auth": "nuvio" };
    if (perfil) headers["x-nuvio-perfil"] = String(perfil);
    const res = await worker.fetch(new Request("https://w.test" + rota, { method: metodo, headers,
      body: metodo === "POST" ? JSON.stringify(corpo ?? {}) : undefined }), e);
    const txt = await res.text();
    let body = null;
    try { body = txt ? JSON.parse(txt) : null; } catch { body = txt; }
    return { status: res.status, body };
  };
  return { ...b, api, amigos, env: e };
}

const NOVA = { ep: "tt1160419", titulo: "Duna: Parte 2", poster: "https://image.tmdb.org/x.jpg",
  imp: { hash: "c".repeat(40), idx: 3, url: "https://real-debrid.com/d/SEGREDO", arquivo: "Dune.Part.Two.2024.1080p.mkv",
    bytes: 2100000000, durMs: 9930000 } };
const agoraS = () => Math.floor(Date.now() / 1000);

test("sem segredo ou sem JUNTOS_URL: 503 (a TV esconde o recurso)", async (t) => {
  const { api } = cenario(t, { SALA_SEGREDO: "" });
  assert.equal((await api("tok-a", "POST", "/v1/sala", NOVA)).status, 503);
});

test("criar: bilhete do anfitriao confere, impressao sem URL, convite so para amigo nao bloqueado", async (t) => {
  const { api, amigos, sqlite } = cenario(t);
  amigos("nuvio:aaa", "nuvio:bbb");
  amigos("nuvio:aaa", "nuvio:ccc");
  sqlite.prepare("INSERT INTO bloqueio (quem, alvo, criado) VALUES ('nuvio:ccc', 'nuvio:aaa', 1)").run();
  const r = await api("tok-a", "POST", "/v1/sala", { ...NOVA, convidados: ["nuvio:bbb", "nuvio:ccc", "nuvio:eee", "nuvio:aaa"] });
  assert.equal(r.status, 200);
  assert.match(r.body.id, /^[0-9a-f]{32}$/);
  assert.match(r.body.codigo, /^[0-9A-HJKMNP-TV-Z]{6}$/);
  assert.deepEqual(r.body.convidados, ["nuvio:bbb"]);
  assert.equal(r.body.ws, "wss://juntos.test/ws");
  const b = await conferir([SEGREDO], r.body.bilhete, agoraS());
  assert.equal(b.papel, "anfitriao");
  assert.equal(b.pessoa, "nuvio:aaa");
  assert.equal(b.cfg.imp.hash, "c".repeat(40));
  assert.ok(!JSON.stringify(b).includes("real-debrid"), "URL de debrid vazou no bilhete");
  assert.ok(!JSON.stringify(sqlite.prepare("SELECT * FROM sala").all()).includes("real-debrid"), "URL no D1");
  assert.ok(b.exp - agoraS() <= 60);
  assert.equal(await conferir(["outro"], r.body.bilhete, agoraS()), null);
});

test("convite chega, entrar por id da bilhete de convidado; sem convite 403; recusar some", async (t) => {
  const { api, amigos } = cenario(t);
  amigos("nuvio:aaa", "nuvio:bbb");
  amigos("nuvio:aaa", "nuvio:ccc");
  const s = (await api("tok-a", "POST", "/v1/sala", { ...NOVA, convidados: ["nuvio:bbb", "nuvio:ccc"] })).body;
  const c = await api("tok-b", "GET", "/v1/sala/convites");
  assert.equal(c.body.convites.length, 1);
  assert.equal(c.body.convites[0].de.nome, "Henrique");
  assert.equal(c.body.convites[0].imp.arquivo, "Dune.Part.Two.2024.1080p.mkv");
  const e = await api("tok-b", "POST", "/v1/sala/entrar", { id: s.id });
  assert.equal(e.status, 200);
  assert.equal(e.body.pend, 0);
  assert.equal((await conferir([SEGREDO], e.body.bilhete, agoraS())).papel, "convidado");
  assert.equal((await api("tok-b", "GET", "/v1/sala/convites")).body.convites.length, 0, "entrou: convite saiu da lista");
  assert.equal((await api("tok-e", "POST", "/v1/sala/entrar", { id: s.id })).status, 403);
  await api("tok-c", "POST", "/v1/sala/recusar", { id: s.id });
  assert.equal((await api("tok-c", "GET", "/v1/sala/convites")).body.convites.length, 0);
  // o anfitriao reentra (reconexao) como anfitriao
  assert.equal((await conferir([SEGREDO], (await api("tok-a", "POST", "/v1/sala/entrar", { id: s.id })).body.bilhete, agoraS())).papel, "anfitriao");
});

test("codigo: amigo entra direto, estranho fica pendente; errar 10 vezes por hora e 429", async (t) => {
  const { api, amigos } = cenario(t);
  amigos("nuvio:aaa", "nuvio:bbb");
  const s = (await api("tok-a", "POST", "/v1/sala", NOVA)).body;
  const cod = s.codigo.slice(0, 3) + "-" + s.codigo.slice(3).toLowerCase();
  assert.equal((await api("tok-b", "POST", "/v1/sala/entrar", { codigo: cod })).body.pend, 0);
  const e = await api("tok-e", "POST", "/v1/sala/entrar", { codigo: s.codigo });
  assert.equal(e.body.pend, 1);
  assert.equal((await conferir([SEGREDO], e.body.bilhete, agoraS())).pend, 1);
  for (let i = 0; i < 10; i++) assert.equal((await api("tok-c", "POST", "/v1/sala/entrar", { codigo: "ZZZZZZ" })).status, 404);
  assert.equal((await api("tok-c", "POST", "/v1/sala/entrar", { codigo: "ZZZZZZ" })).status, 429);
});

test("silenciar e bloqueio: convite nao aparece; bloqueado nao entra nem por codigo", async (t) => {
  const { api, amigos, sqlite } = cenario(t);
  amigos("nuvio:aaa", "nuvio:bbb");
  amigos("nuvio:aaa", "nuvio:ccc");
  await api("tok-b", "POST", "/v1/sala/silenciar", { de: "nuvio:aaa" });
  await api("tok-c", "POST", "/v1/sala/silenciar", { de: "*" });
  const s = (await api("tok-a", "POST", "/v1/sala", { ...NOVA, convidados: ["nuvio:bbb", "nuvio:ccc"] })).body;
  assert.deepEqual(s.convidados, []);
  await api("tok-b", "POST", "/v1/sala/silenciar", { de: "nuvio:aaa", sim: 0 });
  sqlite.prepare("INSERT INTO bloqueio (quem, alvo, criado) VALUES ('nuvio:aaa', 'nuvio:eee', 1)").run();
  assert.equal((await api("tok-e", "POST", "/v1/sala/entrar", { codigo: s.codigo })).status, 404);
});

test("perfil infantil: so convite de quem a conta conhece; sem codigo de estranho", async (t) => {
  const { api, amigos } = cenario(t);
  // Conta bbb: perfil 1 adulto (principal nuvio:bbb), perfil 2 infantil.
  // aaa e amigo do adulto; eee e amigo SO do perfil infantil.
  amigos("nuvio:aaa", "nuvio:bbb");
  amigos("nuvio:aaa", "nuvio:bbb:2");
  amigos("nuvio:eee", "nuvio:bbb:2");
  // a TV da crianca ja falou com /v1/sala* uma vez: o servidor sabe que e infantil
  await api("tok-b", "GET", "/v1/sala/convites?infantil=1", null, 2);
  const deA = (await api("tok-a", "POST", "/v1/sala", { ...NOVA, convidados: ["nuvio:bbb:2"] })).body;
  const deE = (await api("tok-e", "POST", "/v1/sala", { ...NOVA, convidados: ["nuvio:bbb:2"] })).body;
  assert.deepEqual(deA.convidados, ["nuvio:bbb:2"]);
  assert.deepEqual(deE.convidados, [], "amigo so do perfil infantil nao convida a crianca");
  const c = await api("tok-b", "GET", "/v1/sala/convites?infantil=1", null, 2);
  assert.deepEqual(c.body.convites.map((x) => x.de.id), ["nuvio:aaa"]);
  // crianca nao entra por codigo na sala de quem a conta nao conhece
  assert.equal((await api("tok-b", "POST", "/v1/sala/entrar", { codigo: deE.codigo, infantil: 1 }, 2)).status, 403);
  const ok = await api("tok-b", "POST", "/v1/sala/entrar", { id: deA.id, infantil: 1 }, 2);
  assert.equal(ok.status, 200);
  assert.equal((await conferir([SEGREDO], ok.body.bilhete, agoraS())).inf, 1);
  // sala criada pela crianca: estranho por codigo e recusado sem chegar ao modal
  const sk = (await api("tok-b", "POST", "/v1/sala", { ...NOVA, infantil: 1 }, 2)).body;
  assert.equal((await api("tok-e", "POST", "/v1/sala/entrar", { codigo: sk.codigo })).status, 403);
  assert.equal((await api("tok-a", "POST", "/v1/sala/entrar", { codigo: sk.codigo })).body.pend, 0);
});

test("fim: so o anfitriao apaga a sala e os convites", async (t) => {
  const { api, amigos, sqlite } = cenario(t);
  amigos("nuvio:aaa", "nuvio:bbb");
  const s = (await api("tok-a", "POST", "/v1/sala", { ...NOVA, convidados: ["nuvio:bbb"] })).body;
  assert.equal(s.web, "https://juntos.test/j/");
  assert.equal((await api("tok-b", "POST", "/v1/sala/fim", { id: s.id })).status, 200);
  assert.equal(sqlite.prepare("SELECT COUNT(*) n FROM sala").get().n, 1, "so o anfitriao encerra");
  await api("tok-a", "POST", "/v1/sala/fim", { id: s.id });
  assert.equal(sqlite.prepare("SELECT COUNT(*) n FROM sala").get().n, 0);
  assert.equal(sqlite.prepare("SELECT COUNT(*) n FROM convite").get().n, 0);
  assert.equal((await api("tok-b", "POST", "/v1/sala/entrar", { id: s.id })).status, 404);
});

test("feed: 'assistiram juntos' vira uma linha com os outros; quem tem alcance 0 nao aparece", async (t) => {
  const { api, amigos } = cenario(t);
  amigos("nuvio:aaa", "nuvio:bbb");
  amigos("nuvio:aaa", "nuvio:ccc");
  amigos("nuvio:aaa", "nuvio:eee");
  for (const tok of ["tok-b", "tok-c"]) await api(tok, "POST", "/v1/alcance", { nivel: 1 });
  await api("tok-e", "POST", "/v1/alcance", { nivel: 0 });
  const ev = { ev: "junto", imdb: "tt1160419", midia: "movie", titulo: "Duna: Parte 2", seg: 7200 };
  for (const tok of ["tok-b", "tok-c", "tok-e"]) await api(tok, "POST", "/v1/atividade", ev);
  const f = await api("tok-a", "GET", "/v1/feed");
  const juntos = f.body.itens.filter((x) => x.ev === "junto");
  assert.equal(juntos.length, 1, "uma linha por sessao");
  assert.equal(juntos[0].com.length, 1);
  const nomes = [juntos[0].deNome, juntos[0].com[0].nome].sort();
  assert.deepEqual(nomes, ["Fabi", "Rafa"]);
  assert.ok(!JSON.stringify(f.body).includes("Estranho"), "alcance 0 nao aparece");
});
