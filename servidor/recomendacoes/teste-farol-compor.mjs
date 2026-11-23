// Farol: POST /v1/farol/compor e POST /v1/farol/escolha (PLANO-GOSTO.md §5.4, issue 40).
//
// Cobre:
//   - Corpus compartilhado inteiro tests/farol_cenas/casos.json (85 boas + 403 ruins)
//     no validador portado `validarCena` (paridade exata com src/farolpeca.c);
//   - /v1/farol/compor nos 4 modos (home, comparar, dica, retomada) + defesa dupla
//     (titulo fora dos candidatos, tt inventado, texto longo/ALMA, JSON torto -> nunca chega);
//   - Cache compartilhado em KV CATS por 24 h (sem conta/perfil; mesma entrada nao chama LLM);
//   - Tetos atomicos (6/dia compor -> 7a = 429; 20/dia escolha -> 21a = 429; global FAROL_LLM_TETO_DIA);
//   - Interruptor FAROL_LLM=0 -> {origem:"regras"} sem tocar LLM/D1/KV;
//   - Privacidade: inspecao completa de D1, KV e logs (so contadores HMAC e cena no cache;
//     zero linha em `pessoa`, zero corpo/frase/gosto); perfil infantil e sem consentimento;
//   - Corpo > 12 KB -> 413 (com e sem Content-Length), 400/401/405 sem gastar cota.
//
//   node servidor/recomendacoes/teste-farol-compor.mjs

import assert from "node:assert/strict";
import { createHash, createHmac } from "node:crypto";
import { readFileSync } from "node:fs";
import { DatabaseSync } from "node:sqlite";
import test from "node:test";
import worker from "./src/index.js";
import { limitar } from "./src/amigos.js";
import {
  FAROL_CACHE_TTL_S,
  FAROL_CENA_MAX,
  FAROL_COMPOR_CORPO_MAX,
  FAROL_COMPOR_DIA,
  FAROL_ESCOLHA_CORPO_MAX,
  FAROL_ESCOLHA_DIA,
  FAROL_LLM_TETO_PADRAO,
  chaveCacheCompor,
  farolTextoAceita,
  rotaFarolCompor,
  rotaFarolEscolha,
  validarCena,
  validarCenaLlm,
  validarRespostaEscolhaLlm,
} from "./src/farolgosto.js";

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
    .run(createHash("sha256").update("nuvio:tok-gosto-a").digest("hex"), "nuvio:conta-gosto-a", "");
  sqlite.prepare("INSERT INTO sessao (hash, id, nome, expira) VALUES (?, ?, ?, 9999999999)")
    .run(createHash("sha256").update("nuvio:tok-gosto-b").digest("hex"), "nuvio:conta-gosto-b", "");
  return { sqlite, DB };
}

function criarKv(agoraFn = () => Math.floor(Date.now() / 1000)) {
  const mapa = new Map();
  return {
    mapa,
    async get(k) {
      const e = mapa.get(k);
      if (!e) return null;
      if (e.expira && agoraFn() >= e.expira) { mapa.delete(k); return null; }
      return e.valor;
    },
    async put(k, valor, opts = {}) {
      const ttl = Number(opts?.expirationTtl || 0);
      mapa.set(k, { valor, ttl, expira: ttl > 0 ? agoraFn() + ttl : 0 });
    },
    async delete(k) { mapa.delete(k); },
  };
}

function pedidoHomeBase(extra = {}) {
  return {
    v: 1,
    modo: "home",
    idioma: "pt-BR",
    hora: "noite_semana",
    gosto: {
      generos: [{ id: 53, nome: "Suspense", peso: 1.8 }, { id: 878, nome: "Ficcao", peso: 1.4 }],
      temas: [{ id: 4565, nome: "distopia", peso: 1.2 }],
      pessoas: [{ id: 137427, nome: "Denis Villeneuve", peso: 1.6 }],
      ancoras: [{ id: "tt11280740", titulo: "Ruptura" }],
      nivel: 2,
    },
    candidatos: [
      { id: "tt0114369", titulo: "Seven", ano: 1995, generos: [53, 80], minutos: 127 },
      { id: "tt0133093", titulo: "Matrix", ano: 1999, generos: [878, 28], minutos: 136 },
      { id: "tt0816692", titulo: "Interestelar", ano: 2014, generos: [878, 18], minutos: 169 },
      { id: "tt1375666", titulo: "A Origem", ano: 2010, generos: [878, 53], minutos: 148 },
      { id: "tt2543164", titulo: "A Chegada", ano: 2016, generos: [878, 18], minutos: 116 },
      { id: "tt1856101", titulo: "Blade Runner 2049", ano: 2017, generos: [878, 18], minutos: 164 },
      { id: "tt0482571", titulo: "O Grande Truque", ano: 2006, generos: [53, 9648], minutos: 130 },
    ],
    fatos: [],
    ...extra,
  };
}

function cenaHomeValida() {
  return {
    v: 1,
    tela: "home",
    origem: "llm",
    pecas: [{
      t: "fileira",
      id: "gosto1",
      titulo: { m: "FT_FIL_PORQUE_TERMINOU", a: ["tt11280740"], x: "Suspense e ficção para esta noite" },
      forma: "cartaz",
      itens: ["c0", "c1", "c2", "c3", "c4", "c5"],
      motivo: {
        c0: { m: "FT_MOT_GENERO", a: ["g53"], x: "Suspense tenso na medida certa." },
        c4: { m: "FT_MOT_PESSOA", a: ["p137427"] },
      },
    }],
  };
}

function criarRedeFalsa({
  respostaLlm = () => cenaHomeValida(),
  statusLlm = 200,
  falhaRedeLlm = false,
} = {}) {
  const chamadasLlm = [];
  const fetchFalso = async (url, opcoes = {}) => {
    const u = new URL(String(url));
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
  return { fetchFalso, chamadasLlm };
}

function cenario(t, extraEnv = {}, opcoesRede = {}) {
  const b = criarBanco(t);
  const rede = criarRedeFalsa(opcoesRede);
  const fetchOriginal = globalThis.fetch;
  globalThis.fetch = rede.fetchFalso;
  t.after(() => { globalThis.fetch = fetchOriginal; });

  let relogio = 1795000000;
  const kv = criarKv(() => relogio);
  const env = {
    DB: b.DB,
    CATS: kv,
    FAROL_SAL: "sal-sintetico-compor",
    FAROL_LLM: "1",
    FAROL_LLM_PROVEDOR: "openai",
    FAROL_LLM_KEY: "chave-llm-compor-sintetica",
    ...extraEnv,
  };

  const post = async (caminho, corpo, {
    token = "tok-gosto-a",
    perfil,
    metodo = "POST",
    envCustom = env,
    semContentLength = false,
  } = {}) => {
    const headers = { authorization: `Bearer ${token}`, "x-nuvio-auth": "nuvio" };
    if (perfil !== undefined) headers["x-nuvio-perfil"] = String(perfil);
    const init = { method: metodo, headers };
    if (metodo === "POST") {
      const txt = typeof corpo === "string" ? corpo : JSON.stringify(corpo);
      if (semContentLength) {
        const bytes = new TextEncoder().encode(txt);
        init.body = new ReadableStream({
          start(ctrl) {
            for (let i = 0; i < bytes.length; i += 2048) ctrl.enqueue(bytes.subarray(i, i + 2048));
            ctrl.close();
          },
        });
        init.duplex = "half";
      } else {
        init.body = txt;
      }
    }
    const res = await worker.fetch(new Request(`https://w.test${caminho}`, init), envCustom);
    return { status: res.status, body: await res.json().catch(() => null) };
  };

  return {
    ...b, ...rede, kv, env, post,
    avancarRelogio: (s) => { relogio += s; },
    relogio: () => relogio,
  };
}

const q = (sqlite, sql, ...v) => sqlite.prepare(sql).all(...v);

// =============================================================================
// 1. Corpus compartilhado tests/farol_cenas/ (paridade total com src/farolpeca.c)
// =============================================================================

test("validarCena confere todos os casos do corpus compartilhado tests/farol_cenas/casos.json", () => {
  const pasta = new URL("../../tests/farol_cenas/", import.meta.url);
  const manifesto = JSON.parse(readFileSync(new URL("casos.json", pasta), "utf8"));
  assert.ok(Array.isArray(manifesto.casos) && manifesto.casos.length >= 120);

  let boas = 0;
  let ruins = 0;
  for (const caso of manifesto.casos) {
    const bytes = readFileSync(new URL(caso.arq, pasta));
    const res = validarCena(bytes, caso.cand);
    if (caso.rc !== "OK") {
      ruins++;
      assert.equal(res.ok, false, `${caso.arq}: esperava ${caso.rc}, veio OK`);
      assert.equal(res.rcNome, caso.rc, `${caso.arq}: rcNome`);
      continue;
    }
    if (caso.arq.startsWith("boa-")) boas++; else ruins++;
    assert.equal(res.ok, true, `${caso.arq}: esperava OK, veio ${res.rcNome}`);
    assert.equal(res.rc, caso.boas, `${caso.arq}: rc`);
    assert.equal(res.nPecas, caso.boas, `${caso.arq}: boas`);
    assert.equal(res.nRecusadas, caso.rec, `${caso.arq}: rec`);

    const errosEsperados = caso.erros || {};
    for (let pos = 0; pos < 6; pos++) {
      const esp = errosEsperados[String(pos)] || "FPE_OK";
      assert.equal(res.erroNomes[pos], esp, `${caso.arq}: erro[${pos}]`);
    }
  }
  assert.ok(boas >= 40, `boas=${boas}`);
  assert.ok(ruins >= 80, `ruins=${ruins}`);
});

test("farolTextoAceita recusa linha longa, 3a linha, URL, emoji e palavras proibidas do ALMA", () => {
  assert.equal(farolTextoAceita("Suspense curto para esta noite."), true);
  assert.equal(farolTextoAceita("Linha um curta.\nLinha dois tambem curta."), true);
  assert.equal(farolTextoAceita("a".repeat(62)), true);
  assert.equal(farolTextoAceita("a".repeat(63)), false);
  assert.equal(farolTextoAceita("um\ndois\ntres"), false);
  assert.equal(farolTextoAceita("Veja em https://site.test"), false);
  assert.equal(farolTextoAceita("Veja em exemplo.com"), false);
  assert.equal(farolTextoAceita("Ótimo filme 🎉"), false);
  assert.equal(farolTextoAceita("Olá! Posso ajudar?"), false);
  assert.equal(farolTextoAceita("Prontinho, separei estes."), false);
  assert.equal(farolTextoAceita("Desculpe o atraso."), false);
  assert.equal(farolTextoAceita("Ótima escolha para hoje."), false);
});

// =============================================================================
// 2. POST /v1/farol/compor — cena valida, provedores e defesa dupla
// =============================================================================

test("compor devolve FarolCena v1 valida nos modos home, comparar, dica e retomada (openai, anthropic, gemini)", async (t) => {
  // Home (OpenAI + FAROL_LLM_KEY)
  {
    const { post, chamadasLlm } = cenario(t);
    const r = await post("/v1/farol/compor", pedidoHomeBase());
    assert.equal(r.status, 200, JSON.stringify(r.body));
    assert.equal(r.body.v, 1);
    assert.equal(r.body.origem, "llm");
    assert.equal(r.body.tela, "home");
    assert.equal(r.body.pecas.length, 1);
    assert.equal(r.body.pecas[0].t, "fileira");
    assert.equal(r.body.pecas[0].titulo.x, "Suspense e ficção para esta noite");
    const v = validarCena(JSON.stringify(r.body), 7);
    assert.equal(v.ok, true);
    assert.equal(v.nRecusadas, 0);
    assert.equal(chamadasLlm.length, 1);
  }

  // Comparar (Anthropic + secret alias FAROL_LLM_CHAVE + markdown fence)
  {
    const { post, chamadasLlm } = cenario(
      t,
      {
        FAROL_LLM_PROVEDOR: "anthropic",
        FAROL_LLM_MODELO: "claude-haiku-4-5-20251001",
        FAROL_LLM_KEY: "",
        FAROL_LLM_CHAVE: "chave-alias-anthropic",
      },
      {
        respostaLlm: () => '```json\n' + JSON.stringify({
          v: 1,
          tela: "comparar",
          origem: "llm",
          pecas: [{
            t: "comparar",
            itens: ["c0", "c1"],
            eixos: ["duracao", "nota", "porque"],
            veredito: { m: "FT_CMP_VEREDITO_CABE", a: ["c0"], x: "Seven cabe melhor nesta noite." },
            escolha: "c0",
          }],
        }) + "\n```",
      }
    );
    const r = await post("/v1/farol/compor", pedidoHomeBase({ modo: "comparar" }));
    assert.equal(r.status, 200);
    assert.equal(r.body.origem, "llm");
    assert.equal(r.body.pecas[0].t, "comparar");
    assert.equal(chamadasLlm[0].headers["x-api-key"], "chave-alias-anthropic");
    assert.ok(!chamadasLlm[0].url.includes("chave-alias-anthropic"));
  }

  // Retomada + Dica (Gemini)
  {
    const { post, chamadasLlm } = cenario(
      t,
      { FAROL_LLM_PROVEDOR: "gemini", FAROL_LLM_MODELO: "gemini-2.5-flash" },
      {
        respostaLlm: () => ({
          v: 1,
          tela: "detalhe",
          origem: "llm",
          pecas: [{
            t: "info",
            alvo: "tt0903747",
            itens: [{
              k: "retomada",
              l: {
                m: "FT_INFO_RETOMADA_SIMPLES",
                a: ["2", "4"],
                x: "No T2E4 o plano muda depois do encontro no deserto.",
              },
            }],
          }],
        }),
      }
    );
    const r = await post("/v1/farol/compor", {
      v: 1,
      modo: "retomada",
      idioma: "pt-BR",
      hora: "noite_semana",
      gosto: {},
      candidatos: [],
      fatos: [{ alvo: "tt0903747", temporada: 2, episodio: 4, sinopse: "Resumo do episodio ja visto." }],
    });
    assert.equal(r.status, 200);
    assert.equal(r.body.origem, "llm");
    assert.equal(r.body.pecas[0].t, "info");
    assert.equal(chamadasLlm[0].headers["x-goog-api-key"], "chave-llm-compor-sintetica");
  }
});

test("LLM devolvendo titulo fora dos candidatos, texto longo, ALMA proibido ou JSON torto nunca chega ao cliente", async (t) => {
  const saidasRuins = [
    // 1. cN fora da lista de candidatos (so ha 7 candidatos: c0..c6)
    {
      v: 1, tela: "home", pecas: [{
        t: "fileira", id: "gosto1",
        titulo: { m: "FT_FIL_HORA" },
        forma: "cartaz",
        itens: ["c0", "c1", "c2", "c3", "c4", "c99"],
      }],
    },
    // 2. IMDb tt inventado que nao veio nos candidatos
    {
      v: 1, tela: "home", pecas: [{
        t: "fileira", id: "gosto1",
        titulo: { m: "FT_FIL_HORA" },
        forma: "cartaz",
        itens: ["c0", "c1", "c2", "c3", "c4", "tt9999999"],
      }],
    },
    // 3. Texto livre > 62 chars sem modelo m de reserva
    {
      v: 1, tela: "home", pecas: [{
        t: "fileira", id: "gosto1",
        titulo: { x: "Este titulo longo demais ultrapassa sessenta e dois caracteres facilmente na TV" },
        forma: "cartaz",
        itens: ["c0", "c1", "c2", "c3", "c4", "c5"],
      }],
    },
    // 4. Palavra proibida do ALMA sem modelo de reserva
    {
      v: 1, tela: "home", pecas: [{
        t: "fileira", id: "gosto1",
        titulo: { x: "Olá! Ótima escolha para hoje!" },
        forma: "cartaz",
        itens: ["c0", "c1", "c2", "c3", "c4", "c5"],
      }],
    },
    // 5. JSON torto / texto solto
    "Desculpe, aqui esta sua lista de filmes: { quebrado",
    // 6. Envelope com chave desconhecida
    { v: 1, tela: "home", extra: 1, pecas: cenaHomeValida().pecas },
    // 7. Versao errada
    { v: 2, tela: "home", pecas: cenaHomeValida().pecas },
    // 8. Cena acima de 8 KB
    JSON.stringify({ v: 1, tela: "home", pecas: [] }) + " ".repeat(FAROL_CENA_MAX + 50),
  ];

  for (const ruim of saidasRuins) {
    const { post, kv } = cenario(t, {}, { respostaLlm: () => ruim });
    const r = await post("/v1/farol/compor", pedidoHomeBase());
    assert.equal(r.status, 200);
    assert.deepEqual(r.body, { origem: "regras" }, `esperava regras para: ${JSON.stringify(ruim).slice(0, 80)}`);
    assert.equal(kv.mapa.size, 0, "saida invalida do LLM nao entra no cache KV");
  }

  // Texto longo COM modelo `m` de reserva: o `x` ruim e descartado e nunca chega ao cliente,
  // mas a peca valida sobe usando o modelo `m`.
  {
    const textoLongo = "Este texto longo demais passa muito dos sessenta e dois caracteres permitidos";
    const { post } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [{
          t: "fileira", id: "gosto1",
          titulo: { m: "FT_FIL_HORA", x: textoLongo },
          forma: "cartaz",
          itens: ["c0", "c1", "c2", "c3", "c4", "c5"],
        }],
      }),
    });
    const r = await post("/v1/farol/compor", pedidoHomeBase());
    assert.equal(r.status, 200);
    assert.equal(r.body.origem, "llm");
    assert.equal(r.body.pecas[0].titulo.m, "FT_FIL_HORA");
    assert.equal(r.body.pecas[0].titulo.x, undefined, "x reprovado nunca chega ao cliente");
    assert.ok(!JSON.stringify(r.body).includes(textoLongo));
  }

  // Cena mista: 1 peca boa + 1 peca com candidato fora (c99) -> peca ruim nunca chega ao cliente.
  {
    const { post } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [
          cenaHomeValida().pecas[0],
          {
            t: "dica", id: "d_ruim",
            l1: { m: "FT_DICA_CURTO", a: ["c99"] },
            acao: { tipo: "abrir", alvo: "c99" },
          },
        ],
      }),
    });
    const r = await post("/v1/farol/compor", pedidoHomeBase());
    assert.equal(r.status, 200);
    assert.equal(r.body.origem, "llm");
    assert.equal(r.body.pecas.length, 1);
    assert.ok(!JSON.stringify(r.body).includes("c99"));
  }

  // Falha HTTP 503 ou erro de rede do LLM -> {origem:"regras"}.
  for (const opts of [{ statusLlm: 503 }, { falhaRedeLlm: true }]) {
    const { post } = cenario(t, {}, opts);
    const r = await post("/v1/farol/compor", pedidoHomeBase());
    assert.equal(r.status, 200);
    assert.deepEqual(r.body, { origem: "regras" });
  }
});

// =============================================================================
// 3. Cache em KV CATS (24 h, compartilhado sem conta nem perfil)
// =============================================================================

test("mesma entrada em 24 h nao chama o LLM de novo e compartilha cache entre contas sem gravar pessoa", async (t) => {
  const { post, chamadasLlm, kv, avancarRelogio } = cenario(t);
  assert.equal(FAROL_CACHE_TTL_S, 86400);

  const req1 = pedidoHomeBase();
  const r1 = await post("/v1/farol/compor", req1, { token: "tok-gosto-a", perfil: 1 });
  assert.equal(r1.status, 200);
  assert.equal(r1.body.origem, "llm");
  assert.equal(chamadasLlm.length, 1);
  assert.equal(kv.mapa.size, 1);

  const [chaveKv, entradaKv] = [...kv.mapa.entries()][0];
  assert.match(chaveKv, /^farol:compor:v1:[0-9a-f]{64}$/);
  assert.equal(entradaKv.ttl, 86400);

  // Mesma entrada dentro de 24 h (mesma conta): acerta cache e NAO chama o LLM.
  avancarRelogio(3600);
  const r2 = await post("/v1/farol/compor", req1, { token: "tok-gosto-a", perfil: 1 });
  assert.equal(r2.status, 200);
  assert.deepEqual(r2.body, r1.body);
  assert.equal(chamadasLlm.length, 1);

  // Outra conta/perfil com o MESMO gosto e candidatos: divide o mesmo cache sem chamar o LLM.
  const r3 = await post("/v1/farol/compor", req1, { token: "tok-gosto-b", perfil: 2 });
  assert.equal(r3.status, 200);
  assert.deepEqual(r3.body, r1.body);
  assert.equal(chamadasLlm.length, 1);

  // Apos 24 h, TTL vence e volta a chamar o LLM.
  avancarRelogio(86400);
  const r4 = await post("/v1/farol/compor", req1, { token: "tok-gosto-a", perfil: 1 });
  assert.equal(r4.status, 200);
  assert.equal(chamadasLlm.length, 2);

  // Ordem diferente de candidatos gera chave de cache diferente (c0/c1 apontam outro filme).
  const invertido = pedidoHomeBase({ candidatos: [...req1.candidatos].reverse() });
  const k1 = await chaveCacheCompor(req1);
  const k2 = await chaveCacheCompor(invertido);
  assert.notEqual(k1, k2);
});

// =============================================================================
// 4. Tetos diarios: 7a chamada em compor -> 429; 21a em escolha -> 429; global
// =============================================================================

test("tetos diarios: 7a chamada em compor da 429, 21a em escolha da 429, corrida atomica e teto global", async (t) => {
  assert.equal(FAROL_COMPOR_DIA, 6);
  assert.equal(FAROL_ESCOLHA_DIA, 20);
  assert.equal(FAROL_LLM_TETO_PADRAO, 20000);

  const { sqlite, env, fetchFalso, chamadasLlm } = cenario(t);
  let relogio = 1795000000;
  const h = {
    json: (body) => ({ status: 200, body }),
    erro: (msg, status) => ({ status, body: { erro: msg } }),
    agora: () => relogio,
  };

  // 5 chamadas validas na mesma conta (perfis 1 e 2 compartilham a cota da conta).
  for (let i = 0; i < 5; i++) {
    const perfil = i % 2 === 0 ? "nuvio:conta-gosto-a:1" : "nuvio:conta-gosto-a:2";
    const r = await rotaFarolCompor(env, { id: perfil }, pedidoHomeBase({ hora: `faixa_${i}` }), h, limitar, fetchFalso);
    assert.equal(r.status, 200);
    assert.equal(r.body.origem, "llm");
  }

  // 6a e 7a em paralelo: exatamente uma passa (200) e a outra recebe 429 sem chamar o LLM.
  const antesLlm = chamadasLlm.length;
  const [p6, p7] = await Promise.all([
    rotaFarolCompor(env, { id: "nuvio:conta-gosto-a:1" }, pedidoHomeBase({ hora: "corrida_a" }), h, limitar, fetchFalso),
    rotaFarolCompor(env, { id: "nuvio:conta-gosto-a:2" }, pedidoHomeBase({ hora: "corrida_b" }), h, limitar, fetchFalso),
  ]);
  assert.deepEqual([p6.status, p7.status].sort(), [200, 429]);
  assert.equal(chamadasLlm.length, antesLlm + 1);

  // 7a/8a chamada sequencial do dia -> 429 (nao chama LLM).
  const r7 = await rotaFarolCompor(env, { id: "nuvio:conta-gosto-a:1" }, pedidoHomeBase({ hora: "setima" }), h, limitar, fetchFalso);
  assert.equal(r7.status, 429);
  assert.equal(chamadasLlm.length, antesLlm + 1);

  // Outra conta ainda passa ate bater no teto global FAROL_LLM_TETO_DIA.
  const rOutra = await rotaFarolCompor(env, { id: "nuvio:conta-gosto-b" }, pedidoHomeBase({ hora: "outra_conta" }), h, limitar, fetchFalso);
  assert.equal(rOutra.status, 200);

  // Teto global cheio -> 429 e estorna o debito pessoal.
  const envTeto = { ...env, FAROL_LLM_TETO_DIA: "10" };
  sqlite.prepare("UPDATE limite SET n = 10 WHERE chave = 'farol_llm:dia'").run();
  const hContaB = createHmac("sha256", env.FAROL_SAL).update("nuvio:conta-gosto-b").digest("hex");
  const antesB = q(sqlite, "SELECT n FROM limite WHERE chave = ?", `farol_compor:d:${hContaB}`)[0].n;
  const rGlob = await rotaFarolCompor(envTeto, { id: "nuvio:conta-gosto-b" }, pedidoHomeBase({ hora: "estouro_global" }), h, limitar, fetchFalso);
  assert.equal(rGlob.status, 429);
  const depoisB = q(sqlite, "SELECT n FROM limite WHERE chave = ?", `farol_compor:d:${hContaB}`)[0].n;
  assert.equal(depoisB, antesB, "429 global estorna debito pessoal");

  // Dia seguinte (+86400 s) libera novamente.
  relogio += 86400;
  const rNovoDia = await rotaFarolCompor(env, { id: "nuvio:conta-gosto-a:1" }, pedidoHomeBase({ hora: "novo_dia" }), h, limitar, fetchFalso);
  assert.equal(rNovoDia.status, 200);
});

test("escolha: traduz frase em respostas fechadas, valida saida do LLM e bloqueia na 21a chamada", async (t) => {
  const { post, chamadasLlm } = cenario(t, {}, {
    respostaLlm: () => ({
      respostas: { tempo: "t100", peso: "leve", companhia: "crianca" },
      falta: "formato",
    }),
  });

  const corpoEscolha = {
    v: 1,
    idioma: "pt-BR",
    frase: "algo leve de uma hora para ver com meu filho",
    perguntas: ["tempo", "peso", "companhia", "formato"],
    gosto: { generos: [{ id: 16, nome: "Animacao", peso: 1.5 }] },
  };

  for (let i = 0; i < FAROL_ESCOLHA_DIA; i++) {
    const r = await post("/v1/farol/escolha", corpoEscolha);
    assert.equal(r.status, 200, `chamada ${i + 1}`);
    assert.equal(r.body.origem, "llm");
    assert.deepEqual(r.body.respostas, { tempo: "t100", peso: "leve", companhia: "crianca" });
    assert.equal(r.body.falta, "formato");
  }
  assert.equal(chamadasLlm.length, 20);

  // 21a chamada no dia -> 429 sem chamar o LLM.
  const r21 = await post("/v1/farol/escolha", corpoEscolha);
  assert.equal(r21.status, 429);
  assert.equal(chamadasLlm.length, 20);

  // Saidas invalidas do LLM no validador de escolha -> 422 interno -> {origem:"regras"}.
  for (const ruim of [
    "nao e json",
    { respostas: { inventada: "sim" }, falta: null },
    { respostas: { tempo: "opcao_invalida" }, falta: null },
    { respostas: { tempo: "t45" }, falta: "tempo" }, // falta ja respondida
    { respostas: { tempo: "t45" }, falta: "humor" }, // falta fora de perguntas pedidas
    { respostas: {}, falta: null },
    { respostas: { tempo: "t45" }, falta: null, extra: 1 },
  ]) {
    const v = validarRespostaEscolhaLlm(ruim, ["tempo", "peso", "companhia", "formato"]);
    assert.equal(v.ok, false, JSON.stringify(ruim));
    assert.equal(v.status, 422);
    const c = cenario(t, {}, { respostaLlm: () => ruim });
    const r = await c.post("/v1/farol/escolha", corpoEscolha);
    assert.equal(r.status, 200);
    assert.deepEqual(r.body, { origem: "regras" });
  }
});

// =============================================================================
// 5. Interruptor FAROL_LLM=0 e privacidade (D1, KV, logs, infantil, consentiu)
// =============================================================================

test("FAROL_LLM=0 e chave ausente devolvem {origem:'regras'} sem chamar LLM nem gravar em D1/KV", async (t) => {
  const { sqlite, kv, env, post, chamadasLlm } = cenario(t);
  for (const desligado of ["0", "false", "off", "nao", "desligado", false, 0]) {
    const r1 = await post("/v1/farol/compor", pedidoHomeBase(), { envCustom: { ...env, FAROL_LLM: desligado } });
    assert.equal(r1.status, 200);
    assert.deepEqual(r1.body, { origem: "regras" });

    const r2 = await post("/v1/farol/escolha", {
      v: 1, idioma: "pt-BR", frase: "algo curto", perguntas: ["tempo", "peso"],
    }, { envCustom: { ...env, FAROL_LLM: desligado } });
    assert.equal(r2.status, 200);
    assert.deepEqual(r2.body, { origem: "regras" });
  }

  // Sem chave ou sem FAROL_SAL -> tambem cai em {origem:"regras"} sem chamar LLM.
  for (const envQuebrado of [
    { ...env, FAROL_LLM_KEY: "", FAROL_LLM_CHAVE: "" },
    { ...env, FAROL_SAL: "" },
    { ...env, FAROL_LLM_PROVEDOR: "invalido" },
  ]) {
    const r = await post("/v1/farol/compor", pedidoHomeBase(), { envCustom: envQuebrado });
    assert.equal(r.status, 200);
    assert.deepEqual(r.body, { origem: "regras" });
  }

  assert.equal(chamadasLlm.length, 0);
  assert.equal(kv.mapa.size, 0);
  assert.equal(q(sqlite, "SELECT count(*) AS n FROM limite")[0].n, 0);
  assert.equal(q(sqlite, "SELECT count(*) AS n FROM pessoa")[0].n, 0);
});

test("nada do corpo e gravado: D1 so tem contadores HMAC (sem pessoa) e KV so tem a cena no cache", async (t) => {
  const { sqlite, kv, post, chamadasLlm } = cenario(t, {}, {
    respostaLlm: (reqBody) => {
      const user = JSON.parse(reqBody.messages[1].content);
      if (user.frase) return { respostas: { tempo: "t45" }, falta: "peso" };
      return cenaHomeValida();
    },
  });

  const logs = [];
  const origLog = console.log, origWarn = console.warn, origErr = console.error;
  console.log = (...a) => logs.push(a.join(" "));
  console.warn = (...a) => logs.push(a.join(" "));
  console.error = (...a) => logs.push(a.join(" "));
  t.after(() => { console.log = origLog; console.warn = origWarn; console.error = origErr; });

  const canarioTitulo = "CanarioTituloJamaisGravado7711";
  const canarioTema = "canariotemasecreto8822";
  const canarioFrase = "canariofraseescolhajamaisgravada9933";

  const pedidoCompor = pedidoHomeBase({
    gosto: {
      generos: [{ id: 53, nome: "Suspense", peso: 1.5 }],
      temas: [{ id: 999, nome: canarioTema, peso: 1.2 }],
      ancoras: [{ id: "tt11280740", titulo: canarioTitulo }],
    },
    candidatos: [
      { id: "tt0114369", titulo: canarioTitulo, ano: 1995 },
      { id: "tt0133093", titulo: "Matrix", ano: 1999 },
      { id: "tt0816692", titulo: "Interestelar", ano: 2014 },
      { id: "tt1375666", titulo: "A Origem", ano: 2010 },
      { id: "tt2543164", titulo: "A Chegada", ano: 2016 },
      { id: "tt1856101", titulo: "Blade Runner 2049", ano: 2017 },
    ],
  });

  const rc = await post("/v1/farol/compor", pedidoCompor);
  assert.equal(rc.status, 200);
  assert.equal(rc.body.origem, "llm");

  const re = await post("/v1/farol/escolha", {
    v: 1, idioma: "pt-BR", frase: canarioFrase, perguntas: ["tempo", "peso"],
    gosto: { temas: [{ id: 999, nome: canarioTema, peso: 1.0 }] },
  });
  assert.equal(re.status, 200);
  assert.equal(re.body.origem, "llm");
  assert.equal(chamadasLlm.length, 2);

  // 1. D1: zero linhas em `pessoa`; `limite` contem apenas os contadores esperados.
  assert.equal(q(sqlite, "SELECT count(*) AS n FROM pessoa")[0].n, 0);
  const esperadoHmac = createHmac("sha256", "sal-sintetico-compor")
    .update("nuvio:conta-gosto-a")
    .digest("hex");
  const lim = q(sqlite, "SELECT chave, n FROM limite ORDER BY chave").map((x) => ({ ...x }));
  assert.deepEqual(lim, [
    { chave: `farol_compor:d:${esperadoHmac}`, n: 1 },
    { chave: `farol_escolha:d:${esperadoHmac}`, n: 1 },
    { chave: "farol_llm:dia", n: 2 },
  ]);

  // Varre TODAS as tabelas do SQLite: nenhum canario nem id cru (fora de `sessao` semeada).
  const tabelas = q(sqlite, "SELECT name FROM sqlite_schema WHERE type = 'table' AND name NOT LIKE 'sqlite_%'");
  for (const { name } of tabelas) {
    const dump = JSON.stringify(q(sqlite, `SELECT * FROM "${name}"`));
    for (const c of [canarioTitulo, canarioTema, canarioFrase]) {
      assert.ok(!dump.includes(c), `canario vazou em tabela ${name}`);
    }
    if (name !== "sessao") assert.ok(!dump.includes("conta-gosto-a"), `id cru em ${name}`);
  }

  // 2. KV CATS: exatamente 1 entrada (so a cena do compor; escolha nunca vai ao KV),
  // sem conta, sem pessoa_h, sem corpo/gosto/frase/titulo canario.
  assert.equal(kv.mapa.size, 1);
  for (const [k, v] of kv.mapa.entries()) {
    assert.ok(!k.includes("conta-gosto-a") && !k.includes(esperadoHmac));
    for (const c of [canarioTitulo, canarioTema, canarioFrase, "conta-gosto-a", esperadoHmac]) {
      assert.ok(!v.valor.includes(c), `dado vazou no KV: ${c}`);
    }
    const cenaKv = JSON.parse(v.valor);
    assert.equal(cenaKv.v, 1);
    assert.equal(cenaKv.origem, "llm");
    assert.ok(Array.isArray(cenaKv.pecas) && cenaKv.pecas.length === 1);
  }

  // 3. Logs sem canarios nem chaves.
  const logTexto = logs.join("\n");
  for (const s of [canarioTitulo, canarioTema, canarioFrase, "chave-llm-compor-sintetica", "sal-sintetico-compor"]) {
    assert.ok(!logTexto.includes(s));
  }
});

test("perfil infantil (corpo ou sala_infantil) e sem consentimento nunca chamam o LLM", async (t) => {
  const { sqlite, post, chamadasLlm } = cenario(t);

  // infantil: 1 e consentiu: 0 -> {origem:"regras"} sem chamar LLM.
  for (const extra of [{ infantil: 1 }, { consentiu: 0 }]) {
    const r1 = await post("/v1/farol/compor", pedidoHomeBase(extra));
    assert.equal(r1.status, 200);
    assert.deepEqual(r1.body, { origem: "regras" });

    const r2 = await post("/v1/farol/escolha", {
      v: 1, idioma: "pt-BR", frase: "desenho curto", perguntas: ["tempo", "peso"], ...extra,
    });
    assert.equal(r2.status, 200);
    assert.deepEqual(r2.body, { origem: "regras" });
  }

  // Perfil infantil registrado no banco (`sala_infantil`), mesmo sem flag no corpo.
  sqlite.prepare("INSERT INTO sala_infantil (pessoa, visto) VALUES ('nuvio:conta-gosto-a:2', 1)").run();
  const rBanco = await post("/v1/farol/compor", pedidoHomeBase(), { perfil: 2 });
  assert.equal(rBanco.status, 200);
  assert.deepEqual(rBanco.body, { origem: "regras" });
  assert.equal(chamadasLlm.length, 0);
});

// =============================================================================
// 6. Teto de 12 KB (-> 413) e validacao de entrada (400/401/405 sem cota)
// =============================================================================

test("corpo > 12 KB da 413 (com e sem Content-Length) e entradas invalidas dao 400/401/405 sem gastar cota", async (t) => {
  assert.equal(FAROL_COMPOR_CORPO_MAX, 12288);
  assert.equal(FAROL_ESCOLHA_CORPO_MAX, 12288);
  const { sqlite, post, chamadasLlm } = cenario(t);

  // Exatamente 12 KB (12288 bytes) passa.
  const baseStr = JSON.stringify(pedidoHomeBase());
  const exato12k = baseStr + " ".repeat(FAROL_COMPOR_CORPO_MAX - baseStr.length);
  assert.equal(new TextEncoder().encode(exato12k).length, 12288);
  assert.equal((await post("/v1/farol/compor", exato12k)).status, 200);
  assert.equal(chamadasLlm.length, 1);

  // 12289 bytes (1 byte acima de 12 KB) -> 413 com Content-Length E em stream sem Content-Length.
  const grande1 = baseStr + " ".repeat(FAROL_COMPOR_CORPO_MAX - baseStr.length + 1);
  assert.equal(new TextEncoder().encode(grande1).length, 12289);
  assert.equal((await post("/v1/farol/compor", grande1)).status, 413);
  assert.equal((await post("/v1/farol/compor", grande1, { semContentLength: true })).status, 413);

  const escolhaStr = JSON.stringify({ v: 1, idioma: "pt-BR", frase: "ok", perguntas: ["tempo"] });
  const grandeEscolha = escolhaStr + " ".repeat(FAROL_ESCOLHA_CORPO_MAX - escolhaStr.length + 1);
  assert.equal((await post("/v1/farol/escolha", grandeEscolha)).status, 413);
  assert.equal((await post("/v1/farol/escolha", grandeEscolha, { semContentLength: true })).status, 413);

  // Entradas invalidas em /compor e /escolha -> 400.
  for (const ruim of [
    {},
    { v: 2, modo: "home" },
    { v: 1, modo: "desconhecido" },
    { v: 1, modo: "home", diario: [{ id: "tt0114369" }] },
    { v: 1, modo: "home", gosto: { historico: ["tt0114369"] } },
    { v: 1, modo: "home", idioma: "invalido_muito_longo" },
    { v: 1, modo: "home", candidatos: Array.from({ length: 25 }, (_, i) => ({ id: `tt${i}` })) },
  ]) {
    assert.equal((await post("/v1/farol/compor", ruim)).status, 400, JSON.stringify(ruim));
  }
  for (const ruim of [
    {},
    { v: 1, frase: "", perguntas: ["tempo"] },
    { v: 1, frase: "a".repeat(201), perguntas: ["tempo"] },
    { v: 1, frase: "https://vazou.test", perguntas: ["tempo"] },
    { v: 1, frase: "ok", perguntas: [] },
    { v: 1, frase: "ok", perguntas: ["inexistente"] },
    { v: 1, frase: "ok", perguntas: ["tempo", "tempo"] },
    { v: 1, frase: "ok", perguntas: ["tempo"], diario: [] },
  ]) {
    assert.equal((await post("/v1/farol/escolha", ruim)).status, 400, JSON.stringify(ruim));
  }
  assert.equal((await post("/v1/farol/compor", "{json-quebrado")).status, 400);
  assert.equal((await post("/v1/farol/escolha", "{json-quebrado")).status, 400);

  // Metodo errado -> 405; token invalido -> 401.
  assert.equal((await post("/v1/farol/compor", null, { metodo: "GET" })).status, 405);
  assert.equal((await post("/v1/farol/escolha", null, { metodo: "GET" })).status, 405);
  assert.equal((await post("/v1/farol/compor", pedidoHomeBase(), { token: "invalido" })).status, 401);
  assert.equal((await post("/v1/farol/escolha", { v: 1, frase: "ok", perguntas: ["tempo"] }, { token: "invalido" })).status, 401);

  // Nenhuma das recusas acima gastou cota alem do unico pedido valido de 12 KB.
  assert.equal(chamadasLlm.length, 1);
  assert.equal(q(sqlite, "SELECT n FROM limite WHERE chave LIKE 'farol_compor:d:%'")[0].n, 1);
  assert.equal(q(sqlite, "SELECT count(*) AS n FROM limite WHERE chave LIKE 'farol_escolha:d:%'")[0].n, 0);
});
