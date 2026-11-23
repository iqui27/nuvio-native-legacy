// Farol: POST /v1/farol/busca — traducao de frase livre em filtro fechado
// {generos, duracao_max, decada, parecido_com, tipo} via LLM pequeno
// (provedor por env, segredo fora do repo, interruptor FAROL_LLM), execucao no
// TMDB com TMDB_KEY (ate 12 ids TMDB/IMDb), cotas atomicas 30/dia por
// HMAC(FAROL_SAL, conta) e global, frase <= 200 nunca gravada, 422 interno com
// queda para busca por palavras e privacidade de perfil infantil/privado.
//
//   node servidor/recomendacoes/teste-farol-busca.mjs
import assert from "node:assert/strict";
import { createHash, createHmac } from "node:crypto";
import { readFileSync } from "node:fs";
import { DatabaseSync } from "node:sqlite";
import test from "node:test";
import worker from "./src/index.js";
import { limitar } from "./src/amigos.js";
import {
  FAROL_BUSCA_CORPO_MAX,
  FAROL_BUSCA_DIA,
  FAROL_BUSCA_DIA_GLOBAL,
  FAROL_BUSCA_FRASE_MAX,
  FAROL_BUSCA_IDS_MAX,
  rotaFarolBusca,
  validarFiltroLlm,
} from "./src/farol-busca.js";

const MIGRACOES = [
  "schema.sql",
  "migracao-003-registro.sql",
  "migracao-005-amigos.sql",
  "migracao-008-identidade.sql",
  "migracao-010-juntos.sql",
  "migracao-011-diario.sql",
  "migracao-018-farol.sql",
  "migracao-019-farol-janela.sql",
];

function criarBanco(t) {
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
      run: async () => {
        const r = st.run(...valores);
        return { meta: { changes: Number(r.changes), last_row_id: Number(r.lastInsertRowid) } };
      },
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
  sqlite.prepare("INSERT INTO sessao (hash, id, nome, expira) VALUES (?, ?, ?, 9999999999)")
    .run(createHash("sha256").update("nuvio:tok-busca-a").digest("hex"), "nuvio:conta-busca-a", "");
  sqlite.prepare("INSERT INTO sessao (hash, id, nome, expira) VALUES (?, ?, ?, 9999999999)")
    .run(createHash("sha256").update("nuvio:tok-busca-b").digest("hex"), "nuvio:conta-busca-b", "");
  return { sqlite, DB };
}

function criarRedeFalsa({
  respostaLlm = () => ({
    generos: ["suspense", "crime"],
    duracao_max: 105,
    decada: 1990,
    parecido_com: null,
    tipo: "filme",
  }),
  statusLlm = 200,
  falhaRedeLlm = false,
  falhaTmdb = false,
  totalDiscover = 16,
} = {}) {
  const chamadasLlm = [];
  const chamadasTmdb = [];

  const fetchFalso = async (url, opcoes = {}) => {
    const u = new URL(String(url));
    if (u.hostname === "api.themoviedb.org") {
      chamadasTmdb.push({ url: u.toString(), path: u.pathname, params: Object.fromEntries(u.searchParams) });
      if (falhaTmdb) return new Response("upstream down", { status: 503 });
      assert.equal(u.searchParams.get("api_key"), "chave-tmdb-sintetica");

      const extMatch = /^\/3\/(movie|tv)\/(\d+)\/external_ids$/.exec(u.pathname);
      if (extMatch) {
        const prefixo = extMatch[1] === "tv" ? "9" : "1";
        const idNum = Number(extMatch[2]);
        return Response.json({ id: idNum, imdb_id: `tt${prefixo}${String(idNum).padStart(6, "0")}` });
      }
      if (u.pathname === "/3/search/tv" || u.pathname === "/3/search/movie") {
        const q = (u.searchParams.get("query") || "").toLowerCase();
        if (q.includes("inexistente")) return Response.json({ results: [] });
        const ehTv = u.pathname.endsWith("/tv");
        return Response.json({
          results: [{
            id: ehTv ? 70523 : 807,
            name: ehTv ? "Dark" : undefined,
            title: ehTv ? undefined : "Seven",
            genre_ids: ehTv ? [9648, 18] : [53, 80],
            first_air_date: ehTv ? "2017-12-01" : undefined,
            release_date: ehTv ? undefined : "1995-09-22",
          }],
        });
      }
      if (/^\/3\/(movie|tv)\/\d+\/recommendations$/.test(u.pathname)) {
        const ehTv = u.pathname.startsWith("/3/tv/");
        return Response.json({
          results: [
            // Inclui a propria semente para provar que ela nao volta duplicada.
            { id: ehTv ? 70523 : 807, media_type: ehTv ? "tv" : "movie", genre_ids: [9648, 53], release_date: "1995-01-01", first_air_date: "2017-01-01" },
            { id: ehTv ? 95396 : 629, media_type: ehTv ? "tv" : "movie", genre_ids: [9648, 53, 80], release_date: "1995-08-16", first_air_date: "2022-02-18" },
            { id: ehTv ? 66732 : 274, media_type: ehTv ? "tv" : "movie", genre_ids: [9648, 53, 80], release_date: "1991-02-14", first_air_date: "2016-07-15" },
            // Fora da decada de 1990 quando filtro exige 1990:
            { id: 99901, media_type: ehTv ? "tv" : "movie", genre_ids: [53], release_date: "2021-05-01", first_air_date: "2021-05-01" },
          ],
        });
      }
      if (u.pathname === "/3/discover/movie" || u.pathname === "/3/discover/tv") {
        const ehTv = u.pathname.endsWith("/tv");
        const base = ehTv ? 2000 : 1000;
        const results = Array.from({ length: totalDiscover }, (_, i) => ({
          id: base + i + 1,
          title: ehTv ? undefined : `Filme ${i + 1}`,
          name: ehTv ? `Serie ${i + 1}` : undefined,
          genre_ids: ehTv ? [9648, 18] : (i === 0 ? [27] : [53, 80]),
          release_date: ehTv ? undefined : "1996-04-10",
          first_air_date: ehTv ? "1998-09-10" : undefined,
        }));
        return Response.json({ results });
      }
      if (u.pathname === "/3/search/multi") {
        return Response.json({
          results: [
            { id: 550, media_type: "movie", title: "Clube da Luta", genre_ids: [18, 53], release_date: "1999-10-15" },
            { id: 1396, media_type: "tv", name: "Breaking Bad", genre_ids: [18, 80], first_air_date: "2008-01-20" },
            {
              id: 287,
              media_type: "person",
              name: "Ator Exemplo",
              known_for: [
                { id: 16869, media_type: "movie", title: "Bastardos", genre_ids: [18, 10752], release_date: "2009-08-19" },
              ],
            },
          ],
        });
      }
      throw new Error("rota tmdb inesperada: " + u.pathname);
    }

    // Chamada ao provedor de LLM configurado.
    const corpoReq = opcoes.body ? JSON.parse(opcoes.body) : null;
    chamadasLlm.push({ url: u.toString(), headers: opcoes.headers || {}, body: corpoReq });
    if (falhaRedeLlm) throw new Error("llm timeout");
    if (statusLlm !== 200) return new Response("llm error", { status: statusLlm });

    const valor = typeof respostaLlm === "function" ? respostaLlm(corpoReq, u) : respostaLlm;
    const texto = typeof valor === "string" ? valor : JSON.stringify(valor);
    if (u.hostname.includes("anthropic.com")) {
      return Response.json({ content: [{ type: "text", text: texto }] });
    }
    if (u.hostname.includes("googleapis.com")) {
      return Response.json({ candidates: [{ content: { parts: [{ text: texto }] } }] });
    }
    return Response.json({ choices: [{ message: { content: texto } }] });
  };

  return { fetchFalso, chamadasLlm, chamadasTmdb };
}

function cenario(t, extraEnv = {}, opcoesRede = {}) {
  const b = criarBanco(t);
  const rede = criarRedeFalsa(opcoesRede);
  const fetchOriginal = globalThis.fetch;
  globalThis.fetch = rede.fetchFalso;
  t.after(() => { globalThis.fetch = fetchOriginal; });

  const env = {
    DB: b.DB,
    FAROL_SAL: "sal-sintetico-busca",
    FAROL_LLM: "1",
    FAROL_LLM_PROVEDOR: "openai",
    FAROL_LLM_KEY: "chave-llm-sintetica",
    TMDB_KEY: "chave-tmdb-sintetica",
    ...extraEnv,
  };

  const post = async (corpo, { token = "tok-busca-a", perfil, metodo = "POST", envCustom = env } = {}) => {
    const headers = { authorization: `Bearer ${token}`, "x-nuvio-auth": "nuvio" };
    if (perfil !== undefined) headers["x-nuvio-perfil"] = String(perfil);
    const init = { method: metodo, headers };
    if (metodo === "POST") init.body = typeof corpo === "string" ? corpo : JSON.stringify(corpo);
    const res = await worker.fetch(new Request("https://w.test/v1/farol/busca", init), envCustom);
    return { status: res.status, body: await res.json().catch(() => null) };
  };

  return { ...b, ...rede, env, post };
}

const q = (sqlite, sql, ...v) => sqlite.prepare(sql).all(...v);

test("traduz frase em filtro estruturado no LLM, consulta TMDB e devolve ate 12 ids TMDB/IMDb + filtro", async (t) => {
  const { post, chamadasLlm, chamadasTmdb } = cenario(t);
  const r = await post({ frase: "um suspense curto dos anos 90", idioma: "pt-BR" });
  assert.equal(r.status, 200, JSON.stringify(r.body));
  assert.equal(r.body.ok, 1);
  assert.equal(r.body.origem, "llm");
  assert.equal(r.body.llm_status, 200);
  assert.deepEqual(r.body.filtro, {
    generos: ["suspense", "crime"],
    duracao_max: 105,
    decada: 1990,
    parecido_com: null,
    tipo: "filme",
  });
  assert.equal(r.body.ids.length, FAROL_BUSCA_IDS_MAX);
  assert.equal(FAROL_BUSCA_IDS_MAX, 12);
  for (const item of r.body.ids) {
    assert.ok(Number.isInteger(item.tmdb) && item.tmdb > 0);
    assert.match(item.imdb, /^tt\d+$/);
    assert.equal(item.tipo, "movie");
  }
  assert.equal(chamadasLlm.length, 1);
  assert.equal(chamadasLlm[0].headers.authorization, "Bearer chave-llm-sintetica");
  const disc = chamadasTmdb.find((c) => c.path === "/3/discover/movie");
  assert.ok(disc);
  assert.equal(disc.params.with_genres, "53,80");
  assert.equal(disc.params["with_runtime.lte"], "105");
  assert.equal(disc.params["primary_release_date.gte"], "1990-01-01");
  assert.equal(disc.params["primary_release_date.lte"], "1999-12-31");
  assert.equal(disc.params.include_adult, "false");
});

test("parecido_com busca obra-semente, prioriza recomendacoes sem incluir a propria semente e completa com discover", async (t) => {
  const { post, chamadasTmdb } = cenario(t, {}, {
    respostaLlm: () => ({
      generos: ["misterio"],
      duracao_max: null,
      decada: null,
      parecido_com: "Dark",
      tipo: "serie",
    }),
  });
  const r = await post({ frase: "uma serie de misterio parecida com Dark" });
  assert.equal(r.status, 200);
  assert.equal(r.body.filtro.parecido_com, "Dark");
  assert.equal(r.body.filtro.tipo, "serie");
  assert.equal(r.body.ids.length, 12);
  // 70523 era a propria semente Dark -> nunca entra como recomendacao dela mesma.
  assert.ok(r.body.ids.every((x) => x.tmdb !== 70523));
  // As duas recomendacoes validas de /tv/70523/recommendations saem na frente.
  assert.equal(r.body.ids[0].tmdb, 95396);
  assert.equal(r.body.ids[1].tmdb, 66732);
  assert.ok(r.body.ids.every((x) => x.tipo === "series" && /^tt9\d+$/.test(x.imdb)));
  assert.ok(chamadasTmdb.some((c) => c.path === "/3/search/tv"));
  assert.ok(chamadasTmdb.some((c) => c.path === "/3/tv/70523/recommendations"));
});

test("provedor LLM configuravel por variavel de ambiente (openai, anthropic, gemini) sem segredo em URL", async (t) => {
  for (const [provedor, modelo, hostEsperado, checarHeader, respCustom] of [
    ["anthropic", "claude-haiku-4-5-20251001", "api.anthropic.com", (h) => h["x-api-key"] === "chave-llm-sintetica",
      '```json\n{"generos":["comedia"],"duracao_max":null,"decada":null,"parecido_com":null,"tipo":"qualquer"}\n```'],
    ["gemini", "gemini-2.5-flash", "generativelanguage.googleapis.com", (h) => h["x-goog-api-key"] === "chave-llm-sintetica", undefined],
  ]) {
    const { post, chamadasLlm } = cenario(
      t,
      { FAROL_LLM_PROVEDOR: provedor, FAROL_LLM_MODELO: modelo },
      respCustom ? { respostaLlm: () => respCustom } : {}
    );
    const r = await post({ frase: "comedia leve" });
    assert.equal(r.status, 200, provedor);
    if (respCustom) {
      assert.equal(r.body.filtro.tipo, "qualquer");
      assert.ok(r.body.ids.some((x) => x.tipo === "movie"));
      assert.ok(r.body.ids.some((x) => x.tipo === "series"));
    }
    assert.equal(chamadasLlm.length, 1);
    assert.ok(chamadasLlm[0].url.includes(hostEsperado), chamadasLlm[0].url);
    assert.ok(!chamadasLlm[0].url.includes("chave-llm-sintetica"), "chave nunca na URL");
    assert.ok(checarHeader(chamadasLlm[0].headers), provedor);
  }
});

test("interruptor FAROL_LLM e segredos ausentes/invalidos respondem 503 sem chamar rede nem gastar cota", async (t) => {
  const { sqlite, env, post, chamadasLlm, chamadasTmdb } = cenario(t);
  for (const desligado of ["0", "false", "off", "nao", "desligado", false, 0]) {
    const r = await post({ frase: "suspense" }, { envCustom: { ...env, FAROL_LLM: desligado } });
    assert.equal(r.status, 503, `FAROL_LLM=${desligado}`);
  }
  assert.equal((await post({ frase: "suspense" }, { envCustom: { ...env, FAROL_SAL: "" } })).status, 503);
  assert.equal((await post({ frase: "suspense" }, { envCustom: { ...env, TMDB_KEY: "" } })).status, 503);
  assert.equal((await post({ frase: "suspense" }, { envCustom: { ...env, FAROL_LLM_KEY: "" } })).status, 503);
  assert.equal((await post({ frase: "suspense" }, { envCustom: { ...env, FAROL_LLM_PROVEDOR: "desconhecido" } })).status, 503);
  assert.equal((await post({ frase: "suspense" }, { envCustom: { ...env, FAROL_LLM_URL: "http://169.254.169.254/latest" } })).status, 503);
  assert.equal((await post(null, { metodo: "GET" })).status, 405);
  assert.equal((await post({ frase: "suspense" }, { token: "invalido" })).status, 401);
  assert.equal(chamadasLlm.length, 0);
  assert.equal(chamadasTmdb.length, 0);
  assert.equal(q(sqlite, "SELECT count(*) AS n FROM limite")[0].n, 0);
});

test("frase <= 200 caracteres, corpo <= 8 KB e entradas invalidas recusadas sem consumir cota", async (t) => {
  const { sqlite, post } = cenario(t);
  assert.equal(FAROL_BUSCA_FRASE_MAX, 200);
  assert.equal(FAROL_BUSCA_CORPO_MAX, 8192);

  // Exatamente 200 caracteres acentuados (400 bytes UTF-8) passa.
  const exata200 = "é".repeat(200);
  assert.equal((await post({ frase: exata200 })).status, 200);
  assert.equal(q(sqlite, "SELECT n FROM limite WHERE chave LIKE 'farol_busca:d:%'")[0].n, 1);

  // 201 caracteres, vazia, controle, URL, chave proibida (`diario`), JSON quebrado -> 400.
  for (const ruim of [
    { frase: "a".repeat(201) },
    { frase: "é".repeat(201) },
    { frase: "" },
    { frase: "     " },
    { frase: "suspense\u0000curto" },
    { frase: "https://fonte.secreta.test/video.mkv" },
    { frase: 12345 },
    {},
    { frase: "suspense", diario: [{ chave: "tt0114369" }] },
    { frase: "suspense", extra: 1 },
    { frase: "suspense", idioma: "portugues_brasil_longo" },
    { frase: "suspense", generos: "suspense" },
    { frase: "suspense", infantil: "sim" },
  ]) {
    assert.equal((await post(ruim)).status, 400, JSON.stringify(ruim));
  }
  assert.equal((await post("{nao-json")).status, 400);
  assert.equal((await post('{"frase":"ok","constructor":{}}')).status, 400);

  // Corpo cru acima de 8 KB -> 413 medido antes de materializar.
  const grande = JSON.stringify({ frase: "ok" }) + " ".repeat(FAROL_BUSCA_CORPO_MAX + 10);
  assert.equal((await post(grande)).status, 413);

  // Nenhuma das recusas acima incrementou a cota alem do envio valido unico.
  assert.equal(q(sqlite, "SELECT n FROM limite WHERE chave LIKE 'farol_busca:d:%'")[0].n, 1);
});

test("frase nunca e gravada em tabela nem log; rota nao cadastra pessoa e guarda so HMAC + contagem", async (t) => {
  const { sqlite, post } = cenario(t);
  const logs = [];
  const origLog = console.log, origWarn = console.warn, origErr = console.error;
  console.log = (...a) => logs.push(a.join(" "));
  console.warn = (...a) => logs.push(a.join(" "));
  console.error = (...a) => logs.push(a.join(" "));
  t.after(() => { console.log = origLog; console.warn = origWarn; console.error = origErr; });

  const fraseCanario = "canariofrasejamaisgravada918273 suspense anos 90";
  const r = await post({ frase: fraseCanario });
  assert.equal(r.status, 200);
  assert.equal(q(sqlite, "SELECT count(*) AS n FROM pessoa")[0].n, 0);

  const esperadoHmac = createHmac("sha256", "sal-sintetico-busca")
    .update("nuvio:conta-busca-a")
    .digest("hex");
  const lim = q(sqlite, "SELECT chave, n FROM limite ORDER BY chave").map((x) => ({ ...x }));
  assert.deepEqual(lim, [
    { chave: `farol_busca:d:${esperadoHmac}`, n: 1 },
    { chave: "farol_busca:dia", n: 1 },
  ]);

  // Varre todas as tabelas do SQLite: nem a frase nem o id da conta (fora da sessao semeada) existem.
  const tabelas = q(sqlite, "SELECT name FROM sqlite_schema WHERE type = 'table' AND name NOT LIKE 'sqlite_%'");
  for (const { name } of tabelas) {
    const dump = JSON.stringify(q(sqlite, `SELECT * FROM "${name}"`));
    assert.ok(!dump.includes("canariofrasejamaisgravada918273"), `frase vazou em ${name}`);
    if (name !== "sessao") assert.ok(!dump.includes("conta-busca-a"), `id cru em ${name}`);
  }
  const logTexto = logs.join("\n");
  assert.ok(!logTexto.includes("canariofrasejamaisgravada918273"));
  assert.ok(!logTexto.includes("chave-llm-sintetica"));
  assert.ok(!logTexto.includes("chave-tmdb-sintetica"));
});

test("resposta do LLM fora do schema gera 422 interno e cai em busca por palavras no TMDB", async (t) => {
  // 1. Validador puro: todos os desvios de schema retornam status 422 interno.
  const invalidos = [
    "Desculpe, nao consigo responder em JSON.",
    "[]",
    "null",
    "42",
    "{}",
    { generos: [], duracao_max: null, decada: null, parecido_com: null, tipo: "qualquer" },
    { generos: ["suspense"], chave_inventada: true },
    JSON.parse('{"generos":["suspense"],"constructor":{}}'),
    { generos: "suspense", tipo: "filme" },
    { generos: ["genero_inexistente"], tipo: "filme" },
    { generos: ["acao", "drama", "crime", "suspense", "terror"], tipo: "filme" },
    { generos: ["acao"], duracao_max: "90", tipo: "filme" },
    { generos: ["acao"], duracao_max: 5, tipo: "filme" },
    { generos: ["acao"], duracao_max: 999, tipo: "filme" },
    { generos: ["acao"], decada: 1995, tipo: "filme" },
    { generos: ["acao"], decada: "1990s", tipo: "filme" },
    { generos: ["acao"], decada: 1850, tipo: "filme" },
    { generos: ["acao"], parecido_com: "https://vazou.test/x", tipo: "filme" },
    { generos: ["acao"], parecido_com: "user@dominio.test", tipo: "filme" },
    { generos: ["acao"], tipo: "podcast" },
  ];
  for (const caso of invalidos) {
    const v = validarFiltroLlm(caso);
    assert.equal(v.ok, false, JSON.stringify(caso));
    assert.equal(v.status, 422, JSON.stringify(caso));
  }

  // 2. Ponta a ponta: LLM devolve fora do schema -> 422 interno, cai em /3/search/multi e responde 200.
  const { post, chamadasTmdb } = cenario(t, {}, {
    respostaLlm: () => ({ generos: ["fora_do_esquema"], sql: "DROP TABLE pessoa" }),
  });
  const r = await post({ frase: "quero assistir Clube da Luta hoje" });
  assert.equal(r.status, 200);
  assert.equal(r.body.ok, 1);
  assert.equal(r.body.origem, "palavras");
  assert.equal(r.body.llm_status, 422);
  assert.deepEqual(r.body.filtro, {
    generos: [],
    duracao_max: null,
    decada: null,
    parecido_com: null,
    tipo: "qualquer",
  });
  assert.ok(r.body.ids.length >= 2);
  assert.equal(r.body.ids[0].tmdb, 550);
  assert.match(r.body.ids[0].imdb, /^tt\d+$/);
  const multi = chamadasTmdb.find((c) => c.path === "/3/search/multi");
  assert.ok(multi);
  assert.equal(multi.params.query, "Clube Luta");

  // 3. Falha HTTP/timeout do LLM tambem cai em busca por palavras; falha do TMDB devolve 502.
  const cFalhaLlm = cenario(t, {}, { statusLlm: 503 });
  const rLlm503 = await cFalhaLlm.post({ frase: "Breaking Bad" });
  assert.equal(rLlm503.status, 200);
  assert.equal(rLlm503.body.origem, "palavras");
  assert.equal(rLlm503.body.llm_status, 502);

  const cFalhaTmdb = cenario(t, {}, { falhaTmdb: true });
  assert.equal((await cFalhaTmdb.post({ frase: "suspense curto" })).status, 502);
});

test("teto de 30/dia por HMAC da conta (compartilhado entre perfis), corrida atomica e teto global", async (t) => {
  assert.equal(FAROL_BUSCA_DIA, 30);
  assert.equal(FAROL_BUSCA_DIA_GLOBAL, 20000);
  const { sqlite, env, fetchFalso, chamadasLlm } = cenario(t);
  let relogio = 1795000000;
  const h = {
    json: (body) => ({ status: 200, body }),
    erro: (msg, status) => ({ status, body: { erro: msg } }),
    agora: () => relogio,
  };

  // 15 chamadas no perfil 1 + 14 chamadas no perfil 2 da MESMA conta = 29.
  for (let i = 0; i < 15; i++) {
    const r = await rotaFarolBusca(env, { id: "nuvio:conta-busca-a:1" }, { frase: "suspense " + i }, h, limitar, fetchFalso);
    assert.equal(r.status, 200);
  }
  for (let i = 0; i < 14; i++) {
    const r = await rotaFarolBusca(env, { id: "nuvio:conta-busca-a:2" }, { frase: "comedia " + i }, h, limitar, fetchFalso);
    assert.equal(r.status, 200);
  }

  // O 30o e o 31o em paralelo nos perfis 1 e 2: exatamente um passa (200) e o outro barra (429).
  const antesLlm = chamadasLlm.length;
  const [p30, p31] = await Promise.all([
    rotaFarolBusca(env, { id: "nuvio:conta-busca-a:1" }, { frase: "corrida a" }, h, limitar, fetchFalso),
    rotaFarolBusca(env, { id: "nuvio:conta-busca-a:2" }, { frase: "corrida b" }, h, limitar, fetchFalso),
  ]);
  assert.deepEqual([p30.status, p31.status].sort(), [200, 429]);
  assert.equal(chamadasLlm.length, antesLlm + 1, "pedido 429 nao chama o LLM");

  // Outra conta no mesmo dia ainda tem cota propria, ate bater no teto global.
  assert.equal(
    (await rotaFarolBusca(env, { id: "nuvio:conta-busca-b" }, { frase: "drama" }, h, limitar, fetchFalso)).status,
    200
  );

  // Teto global diario cheio (20 000): barra com 429 e devolve o debito pessoal tentado.
  sqlite.prepare("UPDATE limite SET n = ? WHERE chave = 'farol_busca:dia'").run(FAROL_BUSCA_DIA_GLOBAL);
  const hContaB = createHmac("sha256", env.FAROL_SAL).update("nuvio:conta-busca-b").digest("hex");
  const antesContaB = q(sqlite, "SELECT n FROM limite WHERE chave = ?", `farol_busca:d:${hContaB}`)[0].n;
  const glob = await rotaFarolBusca(env, { id: "nuvio:conta-busca-b" }, { frase: "ficcao" }, h, limitar, fetchFalso);
  assert.equal(glob.status, 429);
  const depoisContaB = q(sqlite, "SELECT n FROM limite WHERE chave = ?", `farol_busca:d:${hContaB}`)[0].n;
  assert.equal(depoisContaB, antesContaB, "429 global estorna o incremento pessoal");

  // No dia seguinte (+86400 s), a janela vira e volta a aceitar.
  relogio += 86400;
  assert.equal(
    (await rotaFarolBusca(env, { id: "nuvio:conta-busca-a:1" }, { frase: "novo dia" }, h, limitar, fetchFalso)).status,
    200
  );
});

test("perfil infantil, perfil privado e sem consentimento nunca mandam historico nem generos ao LLM", async (t) => {
  const { sqlite, post, chamadasLlm } = cenario(t);

  // 1. Perfil adulto com consentimento: envia generos canonicos + ids vistos e exclui vistos dos resultados.
  const rAdulto = await post({
    frase: "um suspense curto",
    generos: ["Suspense", "Crime"],
    historico: ["tt1001002", 1003],
  });
  assert.equal(rAdulto.status, 200);
  const cargaAdulto = chamadasLlm.at(-1).body.messages[1].content;
  assert.ok(cargaAdulto.includes("tt1001002"));
  assert.ok(cargaAdulto.includes("tmdb:1003"));
  assert.ok(cargaAdulto.includes("suspense"));
  assert.ok(rAdulto.body.ids.every((x) => x.imdb !== "tt1001002" && x.tmdb !== 1003));

  // 2. Perfil infantil no corpo (`infantil: 1`): limpa historico e generos antes do LLM e filtra terror no TMDB.
  const rInfantil = await post({
    frase: "aventura de robo",
    infantil: 1,
    generos: ["terror", "acao"],
    historico: ["tt1001002", "Titulo Privado"],
  });
  assert.equal(rInfantil.status, 200);
  const cargaInfantil = chamadasLlm.at(-1).body.messages[1].content;
  assert.deepEqual(JSON.parse(cargaInfantil), { frase: "aventura de robo", idioma: "pt-BR" });
  // Item 1001 na fixture tinha genre_ids [27] (terror) -> filtrado para perfil infantil.
  assert.ok(rInfantil.body.ids.every((x) => x.tmdb !== 1001));

  // 3. Perfil infantil ja marcado no banco (`sala_infantil`), mesmo sem `infantil: 1` no corpo.
  sqlite.prepare("INSERT INTO sala_infantil (pessoa, visto) VALUES ('nuvio:conta-busca-a:2', 1)").run();
  const rInfBanco = await post({
    frase: "animacao espacial",
    generos: ["ficcao"],
    historico: ["tt0133093"],
  }, { perfil: 2 });
  assert.equal(rInfBanco.status, 200);
  assert.deepEqual(JSON.parse(chamadasLlm.at(-1).body.messages[1].content), {
    frase: "animacao espacial",
    idioma: "pt-BR",
  });

  // 4. Perfil privado (`privado: 1`) e sem consentimento (`consentiu: 0`): nada alem de frase + idioma.
  for (const extra of [{ privado: 1 }, { consentiu: 0 }]) {
    const r = await post({
      frase: "comedia romantica",
      generos: ["comedia", "romance"],
      historico: ["tt0114369"],
      vistos: [807],
      ...extra,
    });
    assert.equal(r.status, 200);
    assert.deepEqual(JSON.parse(chamadasLlm.at(-1).body.messages[1].content), {
      frase: "comedia romantica",
      idioma: "pt-BR",
    });
  }
});
