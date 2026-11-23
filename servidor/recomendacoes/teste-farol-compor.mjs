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
import * as farolGostoMod from "./src/farolgosto.js";
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
    consentiu: 1,
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
    assert.equal(r.body.pecas[0].titulo.m, "FT_FIL_PORQUE_TERMINOU");
    assert.deepEqual(r.body.pecas[0].titulo.a, ["tt11280740"]);
    assert.equal(r.body.pecas[0].titulo.x, undefined);
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
      consentiu: 1,
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
  // O KV persiste apenas modelos `m` e referencias controladas (sem texto livre `x`).
  assert.ok(!entradaKv.valor.includes("\"x\""), "KV nao persiste texto livre x");
  assert.ok(!entradaKv.valor.includes("Suspense"), "KV nao persiste literal de gosto");
  avancarRelogio(3600);
  const r2 = await post("/v1/farol/compor", req1, { token: "tok-gosto-a", perfil: 1 });
  assert.equal(r2.status, 200);
  assert.equal(r2.body.origem, "llm");
  assert.equal(r2.body.pecas[0].titulo.m, "FT_FIL_PORQUE_TERMINOU");
  assert.deepEqual(r2.body.pecas[0].titulo.a, ["tt11280740"]);
  assert.equal(r2.body.pecas[0].titulo.x, undefined);
  assert.equal(validarCena(JSON.stringify(r2.body), 7).ok, true);
  assert.equal(chamadasLlm.length, 1);

  // Outra conta/perfil com o MESMO gosto e candidatos: divide o mesmo cache sem chamar o LLM.
  const r3 = await post("/v1/farol/compor", req1, { token: "tok-gosto-b", perfil: 2 });
  assert.equal(r3.status, 200);
  assert.deepEqual(r3.body, r2.body);
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
    consentiu: 1,
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
      v: 1, idioma: "pt-BR", consentiu: 1, frase: "algo curto", perguntas: ["tempo", "peso"],
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
      pessoas: [{ id: 137427, nome: "Denis Villeneuve", peso: 1.6 }],
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
    v: 1, idioma: "pt-BR", consentiu: 1, frase: canarioFrase, perguntas: ["tempo", "peso"],
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

test("perfil infantil (corpo, sala_infantil ou erro na consulta) e sem consentimento afirmativo nunca chamam o LLM", async (t) => {
  const { sqlite, post, chamadasLlm } = cenario(t);

  // infantil: 1, consentiu: 0/false e consentiu AUSENTE -> {origem:"regras"} sem chamar LLM nem gastar cota.
  const semOptIn = [
    { infantil: 1 },
    { consentiu: 0 },
    { consentiu: false },
  ];
  for (const extra of semOptIn) {
    const r1 = await post("/v1/farol/compor", pedidoHomeBase(extra));
    assert.equal(r1.status, 200);
    assert.deepEqual(r1.body, { origem: "regras" });

    const r2 = await post("/v1/farol/escolha", {
      v: 1, idioma: "pt-BR", frase: "desenho curto", perguntas: ["tempo", "peso"], consentiu: 1, ...extra,
    });
    assert.equal(r2.status, 200);
    assert.deepEqual(r2.body, { origem: "regras" });
  }

  // Ausencia de `consentiu` nas duas rotas deve cair em {origem:"regras"} (P1.1).
  const reqComporSemConsentiu = pedidoHomeBase();
  delete reqComporSemConsentiu.consentiu;
  const rComporOmisso = await post("/v1/farol/compor", reqComporSemConsentiu);
  assert.equal(rComporOmisso.status, 200);
  assert.deepEqual(rComporOmisso.body, { origem: "regras" });

  const rEscolhaOmisso = await post("/v1/farol/escolha", {
    v: 1, idioma: "pt-BR", frase: "desenho curto", perguntas: ["tempo", "peso"],
  });
  assert.equal(rEscolhaOmisso.status, 200);
  assert.deepEqual(rEscolhaOmisso.body, { origem: "regras" });

  // Perfil infantil registrado no banco (`sala_infantil`), mesmo com consentiu: 1.
  sqlite.prepare("INSERT INTO sala_infantil (pessoa, visto) VALUES ('nuvio:conta-gosto-a:2', 1)").run();
  const rBanco = await post("/v1/farol/compor", pedidoHomeBase({ consentiu: true }), { perfil: 2 });
  assert.equal(rBanco.status, 200);
  assert.deepEqual(rBanco.body, { origem: "regras" });

  // Falha na consulta infantil (`sala_infantil` indisponivel/erro no D1) -> fail-closed para {origem:"regras"} nas duas rotas (P1.1).
  sqlite.exec("DROP TABLE sala_infantil");
  const rComporDbErro = await post("/v1/farol/compor", pedidoHomeBase({ consentiu: true }), { perfil: 1 });
  assert.equal(rComporDbErro.status, 200);
  assert.deepEqual(rComporDbErro.body, { origem: "regras" });

  const rEscolhaDbErro = await post("/v1/farol/escolha", {
    v: 1, idioma: "pt-BR", consentiu: true, frase: "algo leve", perguntas: ["tempo", "peso"],
  }, { perfil: 1 });
  assert.equal(rEscolhaDbErro.status, 200);
  assert.deepEqual(rEscolhaDbErro.body, { origem: "regras" });

  assert.equal(chamadasLlm.length, 0);
  assert.equal(q(sqlite, "SELECT count(*) AS n FROM limite")[0].n, 0);
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

  const escolhaStr = JSON.stringify({ v: 1, idioma: "pt-BR", consentiu: 1, frase: "ok", perguntas: ["tempo"] });
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

// =============================================================================
// 7. Regressoes dos P1 da revisao FAROL40-r1
// =============================================================================

test("P1.2: cache compartilhado nao grava literais de gosto mesmo quando o LLM ecoa canarios", async (t) => {
  const canarioTema = "CanarioTemaEco9911";
  const canarioPessoa = "CanarioPessoaEco8822";
  const canarioAncora = "CanarioAncoraEco7733";

  // Caso A: LLM devolve m+a valido e ecoa canarios dentro de `x` (dica.l1.x e titulo.x).
  {
    const { kv, post } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1,
        tela: "home",
        origem: "llm",
        pecas: [
          {
            t: "fileira",
            id: "gosto1",
            titulo: { m: "FT_FIL_PORQUE_TERMINOU", a: ["tt11280740"], x: `Parecido com ${canarioAncora}` },
            forma: "cartaz",
            itens: ["c0", "c1", "c2", "c3", "c4", "c5"],
          },
          {
            t: "dica",
            id: "d_tema",
            l1: { m: "FT_DICA_TRAKT", x: `Curte ${canarioTema} e ${canarioPessoa}` },
            acao: { tipo: "nada" },
          },
        ],
      }),
    });

    const r = await post("/v1/farol/compor", pedidoHomeBase({
      consentiu: true,
      gosto: {
        generos: [{ id: 53, nome: "Suspense", peso: 1.8 }],
        temas: [{ id: 999, nome: canarioTema, peso: 1.5 }],
        pessoas: [{ id: 137427, nome: canarioPessoa, peso: 1.4 }],
        ancoras: [{ id: "tt11280740", titulo: canarioAncora }],
      },
    }));
    assert.equal(r.status, 200);
    assert.equal(r.body.origem, "llm");
    assert.equal(kv.mapa.size, 1);
    const [[, entrada]] = [...kv.mapa.entries()];
    for (const c of [canarioTema, canarioPessoa, canarioAncora, "Suspense", "\"x\""]) {
      assert.ok(!entrada.valor.includes(c), `KV vazou ${c}`);
    }
    const cenaKv = JSON.parse(entrada.valor);
    assert.equal(cenaKv.pecas[1].l1.m, "FT_DICA_TRAKT");
    assert.equal(cenaKv.pecas[1].l1.x, undefined);
  }

  // Caso B: peca com texto apenas `x` (sem `m`) -> recusada (nao exibe prosa nao aterrada e nao quebra cache).
  {
    const { kv, post } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1,
        tela: "home",
        origem: "llm",
        pecas: [{
          t: "dica",
          id: "d_so_x",
          l1: { x: `Tema: ${canarioTema}` },
          acao: { tipo: "nada" },
        }],
      }),
    });
    const r = await post("/v1/farol/compor", pedidoHomeBase({
      consentiu: 1,
      gosto: { temas: [{ id: 999, nome: canarioTema, peso: 1.5 }] },
    }));
    assert.equal(r.status, 200);
    assert.deepEqual(r.body, { origem: "regras" });
    assert.equal(kv.mapa.size, 0, "cena sem modelo m nao entra no cache nem na resposta");
  }

  // Caso C: LLM tenta colocar canario dentro de `a` (ex.: FT_FIL_TEMA) -> recusado.
  {
    const { kv, post } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1,
        tela: "home",
        origem: "llm",
        pecas: [{
          t: "fileira",
          id: "gosto1",
          titulo: { m: "FT_FIL_TEMA", a: [canarioTema] },
          forma: "cartaz",
          itens: ["c0", "c1", "c2", "c3", "c4", "c5"],
        }],
      }),
    });
    const r = await post("/v1/farol/compor", pedidoHomeBase({
      consentiu: 1,
      gosto: { temas: [{ id: 999, nome: canarioTema, peso: 1.5 }] },
    }));
    assert.equal(r.status, 200);
    assert.deepEqual(r.body, { origem: "regras" });
    assert.equal(kv.mapa.size, 0);
  }
});

test("P1.3: argumentos de titulo por modelo FT_* exigem referencia valida do pedido (nunca titulo literal)", async (t) => {
  const req = pedidoHomeBase({
    fatos: [{ alvo: "tt0903747", temporada: 2, episodio: 4 }],
  });

  const cenasComTituloInvalido = [
    // Literal ausente de candidatos/ancoras/fatos ("Planeta Imaginário" da revisao)
    {
      v: 1, tela: "home", pecas: [{
        t: "fileira", id: "gosto1",
        titulo: { m: "FT_FIL_PORQUE_TERMINOU", a: ["Planeta Imaginário"] },
        forma: "cartaz",
        itens: ["c0", "c1", "c2", "c3", "c4", "c5"],
      }],
    },
    // Literal mesmo que seja nome de candidato ou ancora do pedido (precisa ser ref c<n>/tt<n>)
    {
      v: 1, tela: "home", pecas: [{
        t: "fileira", id: "gosto1",
        titulo: { m: "FT_FIL_PORQUE_TERMINOU", a: ["Ruptura"] },
        forma: "cartaz",
        itens: ["c0", "c1", "c2", "c3", "c4", "c5"],
      }],
    },
    // FT_DICA_FALTA_L1 tem o titulo no argumento indice 2
    {
      v: 1, tela: "home", pecas: [{
        t: "dica", id: "d1",
        l1: { m: "FT_DICA_FALTA_L1", a: ["3", "2", "Planeta Imaginário"] },
        acao: { tipo: "abrir", alvo: "tt0903747" },
      }],
    },
    // FT_MOT_DIRETOR tem titulo no indice 0
    {
      v: 1, tela: "home", pecas: [{
        t: "fileira", id: "gosto1",
        titulo: { m: "FT_FIL_HORA" },
        forma: "cartaz",
        itens: ["c0", "c1", "c2", "c3", "c4", "c5"],
        motivo: { c0: { m: "FT_MOT_DIRETOR", a: ["Seven", "2"] } },
      }],
    },
    // FT_CMP_VEREDITO_CABE nao pode citar ancora (apenas candidato) nem titulo literal
    {
      v: 1, tela: "comparar", pecas: [{
        t: "comparar", itens: ["c0", "c1"], eixos: ["duracao"],
        veredito: { m: "FT_CMP_VEREDITO_CABE", a: ["tt11280740"] },
      }],
    },
    {
      v: 1, tela: "comparar", pecas: [{
        t: "comparar", itens: ["c0", "c1"], eixos: ["duracao"],
        veredito: { m: "FT_CMP_VEREDITO_CABE", a: ["Seven"] },
      }],
    },
  ];

  for (const ruim of cenasComTituloInvalido) {
    const val = validarCenaLlm(JSON.stringify(ruim), req, 1795000000);
    assert.equal(val.ok, false, `esperava rejeicao para ${JSON.stringify(ruim)}`);
    const { post, kv } = cenario(t, {}, { respostaLlm: () => ruim });
    const r = await post("/v1/farol/compor", req);
    assert.equal(r.status, 200);
    assert.deepEqual(r.body, { origem: "regras" });
    assert.equal(kv.mapa.size, 0);
  }

  // Referencia valida no argumento indice 2 de FT_DICA_FALTA_L1 passa limpa.
  const okDica = validarCenaLlm(JSON.stringify({
    v: 1, tela: "home", pecas: [{
      t: "dica", id: "d1",
      l1: { m: "FT_DICA_FALTA_L1", a: ["3", "2", "tt0903747"] },
      acao: { tipo: "abrir", alvo: "tt0903747" },
    }],
  }), req, 1795000000);
  assert.equal(okDica.ok, true);
});

test("P1.4: chave de cache distingue identidades canonicas (id+ref) e hit KV valida referencias contra pedido atual", async (t) => {
  const fazerCandidatos = (ref0) => [
    { id: "cand-primario-0", ref: ref0, titulo: "Filme Zero", ano: 2020 },
    { id: "tt0133093", titulo: "Matrix", ano: 1999 },
    { id: "tt0816692", titulo: "Interestelar", ano: 2014 },
    { id: "tt1375666", titulo: "A Origem", ano: 2010 },
    { id: "tt2543164", titulo: "A Chegada", ano: 2016 },
    { id: "tt1856101", titulo: "Blade Runner 2049", ano: 2017 },
  ];

  const reqA = pedidoHomeBase({
    gosto: { ancoras: [{ id: "anc-1", ref: "tt1111111", titulo: "Ancora A" }] },
    candidatos: fazerCandidatos("tt0114369"),
  });
  const reqB = pedidoHomeBase({
    gosto: { ancoras: [{ id: "anc-1", ref: "tt1111111", titulo: "Ancora A" }] },
    candidatos: fazerCandidatos("tt9999999"),
  });
  const reqC = pedidoHomeBase({
    gosto: { ancoras: [{ id: "anc-1", ref: "tt2222222", titulo: "Ancora A" }] },
    candidatos: fazerCandidatos("tt0114369"),
  });

  // 1. Mesmo id primario com ref diferente gera chaves distintas (candidato e ancora).
  const kA = await chaveCacheCompor(reqA);
  const kB = await chaveCacheCompor(reqB);
  const kC = await chaveCacheCompor(reqC);
  assert.notEqual(kA, kB, "ref diferente no candidato deve mudar a chave");
  assert.notEqual(kA, kC, "ref diferente na ancora deve mudar a chave");

  // 2. Segunda conta com mesmo id primario e ref diferente NAO recebe alvo IMDb da primeira.
  {
    let nChamada = 0;
    const { post, chamadasLlm } = cenario(t, {}, {
      respostaLlm: () => {
        nChamada++;
        const ref0 = nChamada === 1 ? "tt0114369" : "tt9999999";
        return {
          v: 1, tela: "home", origem: "llm",
          pecas: [{
            t: "fileira", id: "gosto1",
            titulo: { m: "FT_FIL_HORA" },
            forma: "cartaz",
            itens: [ref0, "c1", "c2", "c3", "c4", "c5"],
          }],
        };
      },
    });

    const rA = await post("/v1/farol/compor", reqA, { token: "tok-gosto-a", perfil: 1 });
    assert.equal(rA.status, 200);
    assert.equal(rA.body.pecas[0].itens[0], "tt0114369");

    const rB = await post("/v1/farol/compor", reqB, { token: "tok-gosto-b", perfil: 1 });
    assert.equal(rB.status, 200);
    assert.equal(rB.body.pecas[0].itens[0], "tt9999999");
    assert.equal(chamadasLlm.length, 2);
  }

  // 3. Mesmo havendo entrada no KV na chave do pedido atual com tt estranho ou titulo literal,
  // o hit do KV revalida contra o pedido atual e recusa a entrada invalida.
  {
    const { kv, post, chamadasLlm } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [{
          t: "fileira", id: "gosto1",
          titulo: { m: "FT_FIL_HORA" },
          forma: "cartaz",
          itens: ["c0", "c1", "c2", "c3", "c4", "c5"],
        }],
      }),
    });
    const chaveA = await chaveCacheCompor(reqA);

    // Injeta cena com IMDb estranho `tt8888888` ausente de reqA:
    await kv.put(chaveA, JSON.stringify({
      v: 1, tela: "home", feita: 1795000000, vale_s: 86400, origem: "llm",
      pecas: [{
        t: "fileira", id: "gosto1",
        titulo: { m: "FT_FIL_HORA" },
        forma: "cartaz",
        itens: ["tt8888888", "c1", "c2", "c3", "c4", "c5"],
      }],
    }), { expirationTtl: 86400 });

    const r = await post("/v1/farol/compor", reqA, { token: "tok-gosto-a", perfil: 1 });
    assert.equal(r.status, 200);
    assert.equal(r.body.origem, "llm");
    assert.equal(r.body.pecas[0].itens[0], "c0", "hit KV invalido deve ser rejeitado e recomputado");
    assert.equal(chamadasLlm.length, 1);

    // Injeta cena com titulo literal em FT_FIL_PORQUE_TERMINOU:
    await kv.put(chaveA, JSON.stringify({
      v: 1, tela: "home", feita: 1795000000, vale_s: 86400, origem: "llm",
      pecas: [{
        t: "fileira", id: "gosto1",
        titulo: { m: "FT_FIL_PORQUE_TERMINOU", a: ["Planeta Imaginário"] },
        forma: "cartaz",
        itens: ["c0", "c1", "c2", "c3", "c4", "c5"],
      }],
    }), { expirationTtl: 86400 });

    const r2 = await post("/v1/farol/compor", reqA, { token: "tok-gosto-b", perfil: 1 });
    assert.equal(r2.status, 200);
    assert.equal(r2.body.pecas[0].titulo.m, "FT_FIL_HORA");
    assert.equal(chamadasLlm.length, 2);
  }
});

// =============================================================================
// 8. Regressoes dos 5 P1 da revisao FAROL40-r2 e contrato fechado
// =============================================================================

test("R2-P1.1: rejeita id textual de afinidade e exige contrato fechado por modelo FT_* (duracao, hora, data, escolha)", async (t) => {
  // 1. gosto.temas/generos/pessoas com id textual -> 400 sem chamar LLM nem gravar KV.
  for (const gostoRuim of [
    { temas: [{ id: "CanarioIdLivreR2" }] },
    { generos: [{ id: "CanarioGenLivre" }] },
    { pessoas: [{ id: "CanarioPesLivre" }] },
    { temas: [{ id: 0 }] },
  ]) {
    const { post, chamadasLlm, kv } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [{
          t: "dica", id: "d1",
          l1: { m: "FT_DICA_SALVO_L1", a: ["CanarioIdLivreR2"] },
          acao: { tipo: "nada" },
        }],
      }),
    });
    const r = await post("/v1/farol/compor", pedidoHomeBase({ gosto: gostoRuim }));
    assert.equal(r.status, 400, JSON.stringify(gostoRuim));
    assert.equal(chamadasLlm.length, 0);
    assert.equal(kv.mapa.size, 0);
  }

  // 2. Contrato fechado de argumentos por modelo: valores livres em modelos de duracao/data/hora/quali/onde/amigos falham.
  const reqComFatos = pedidoHomeBase({
    fatos: [{ alvo: "tt0903747", titulo: "Breaking Bad", onde: "Netflix" }],
  });
  const cenasInvalidasContrato = [
    // FT_DICA_SALVO_L1 fora do formato fechado (<n><d|w|M|y>)
    { v: 1, tela: "home", pecas: [{ t: "dica", id: "d1", l1: { m: "FT_DICA_SALVO_L1", a: ["CanarioIdLivreR2"] }, acao: { tipo: "nada" } }] },
    { v: 1, tela: "home", pecas: [{ t: "dica", id: "d1", l1: { m: "FT_DICA_SALVO_L1", a: ["0d"] }, acao: { tipo: "nada" } }] },
    // FT_DICA_SALVO_L2 fora do formato fechado
    { v: 1, tela: "home", pecas: [{ t: "dica", id: "d1", l1: { m: "FT_DICA_SALVO_L1", a: ["5M"] }, l2: { m: "FT_DICA_SALVO_L2", a: ["CanarioDur"] }, acao: { tipo: "nada" } }] },
    // FT_MOT_SALVO fora do formato fechado
    { v: 1, tela: "home", pecas: [{ t: "fileira", id: "gosto1", titulo: { m: "FT_FIL_HORA" }, forma: "cartaz", itens: ["c0", "c1", "c2", "c3", "c4", "c5"], motivo: { c0: { m: "FT_MOT_SALVO", a: ["muito tempo"] } } }] },
    // FT_INFO_RETOMADA arg[2] nao pode ser texto livre
    { v: 1, tela: "detalhe", pecas: [{ t: "info", alvo: "tt0903747", itens: [{ k: "retomada", l: { m: "FT_INFO_RETOMADA", a: ["2", "4", "CanarioRet"] } }] }] },
    // FT_INFO_TERMINA fora de HH:MM / H:MM AM|PM
    { v: 1, tela: "detalhe", pecas: [{ t: "info", alvo: "tt0903747", itens: [{ k: "tempo", l: { m: "FT_INFO_TERMINA", a: ["meia-noite"] } }] }] },
    // FT_INFO_REVER fora de YYYY-MM ou estrelas > 5
    { v: 1, tela: "detalhe", pecas: [{ t: "info", alvo: "tt0903747", itens: [{ k: "rever", l: { m: "FT_INFO_REVER", a: ["2024-13", "4"] } }] }] },
    { v: 1, tela: "detalhe", pecas: [{ t: "info", alvo: "tt0903747", itens: [{ k: "rever", l: { m: "FT_INFO_REVER", a: ["2024-03", "9"] } }] }] },
    // FT_INFO_ONDE nao aterrado em fatos
    { v: 1, tela: "detalhe", pecas: [{ t: "info", alvo: "tt0903747", itens: [{ k: "onde", l: { m: "FT_INFO_ONDE", a: ["CanarioServico"] } }] }] },
    // FT_INFO_SAGA com pos > total ou FT_ESC_PASSO com passo > de
    { v: 1, tela: "detalhe", pecas: [{ t: "info", alvo: "tt0903747", itens: [{ k: "saga", l: { m: "FT_INFO_SAGA", a: ["4", "3"] } }] }] },
    // Modelos fora do compor (diagnostico ou amigos com u<id>)
    { v: 1, tela: "home", pecas: [{ t: "dica", id: "d1", l1: { m: "FT_FONTE_TROCADA", a: ["Canario"] }, acao: { tipo: "nada" } }] },
    { v: 1, tela: "home", pecas: [{ t: "fileira", id: "gosto1", titulo: { m: "FT_FIL_HORA" }, forma: "cartaz", itens: ["c0", "c1", "c2", "c3", "c4", "c5"], motivo: { c0: { m: "FT_MOT_AMIGO", a: ["u1", "8"] } } }] },
    // P1 fileira com id nao suportado pela TV (src/farol.c:525 so aceita gosto1/gosto2)
    { v: 1, tela: "home", pecas: [{ t: "fileira", id: "outra", titulo: { m: "FT_FIL_HORA" }, forma: "cartaz", itens: ["c0", "c1", "c2", "c3", "c4", "c5"] }] },
    // P5 escolha com pergunta/opcao fora do banco fechado da TV
    { v: 1, tela: "home", pecas: [{ t: "escolha", passo: 1, de: 3, pergunta: { m: "FT_ESC_TEMPO" }, opcoes: [{ id: "inventada", r: { m: "FT_ESC_TEMPO_45" } }, { id: "tanto", r: { m: "FT_ESC_TANTO_FAZ" } }] }] },
    { v: 1, tela: "home", pecas: [{ t: "escolha", passo: 1, de: 3, pergunta: { m: "FT_ESC_TEMPO" }, opcoes: [{ id: "t45", r: { m: "FT_ESC_PESO_LEVE" } }, { id: "tanto", r: { m: "FT_ESC_TANTO_FAZ" } }] }] },
  ];
  for (const ruim of cenasInvalidasContrato) {
    const modo = ruim.tela === "detalhe" ? "retomada" : "home";
    const req = { ...reqComFatos, modo };
    const { post, kv } = cenario(t, {}, { respostaLlm: () => ruim });
    const r = await post("/v1/farol/compor", req);
    assert.equal(r.status, 200);
    assert.deepEqual(r.body, { origem: "regras" }, `esperava regras para ${JSON.stringify(ruim)}`);
    assert.equal(kv.mapa.size, 0);
  }

  // 3. Formatos fechados validos passam e sao cacheados.
  const cenasValidasContrato = [
    { modo: "home", cena: { v: 1, tela: "home", pecas: [{ t: "dica", id: "d1", l1: { m: "FT_DICA_SALVO_L1", a: ["5M"] }, l2: { m: "FT_DICA_SALVO_L2", a: ["1h32"] }, acao: { tipo: "nada" } }] } },
    { modo: "home", cena: { v: 1, tela: "home", pecas: [{ t: "escolha", passo: 1, de: 3, pergunta: { m: "FT_ESC_TEMPO" }, opcoes: [{ id: "t45", r: { m: "FT_ESC_TEMPO_45" } }, { id: "tanto", r: { m: "FT_ESC_TANTO_FAZ" } }] }] } },
    { modo: "retomada", cena: { v: 1, tela: "detalhe", pecas: [{ t: "info", alvo: "tt0903747", itens: [{ k: "tempo", l: { m: "FT_INFO_TERMINA", a: ["23:40"] } }, { k: "onde", l: { m: "FT_INFO_ONDE", a: ["Netflix"] } }, { k: "retomada", l: { m: "FT_INFO_RETOMADA", a: ["2", "4", "7w"] } }] }] } },
  ];
  for (const { modo, cena } of cenasValidasContrato) {
    const { post, kv } = cenario(t, {}, { respostaLlm: () => cena });
    const r = await post("/v1/farol/compor", { ...reqComFatos, modo });
    assert.equal(r.status, 200);
    assert.equal(r.body.origem, "llm", JSON.stringify(cena));
    assert.equal(kv.mapa.size, 1);
  }
});

test("R2-P1.2: titulo fora do pedido dentro de `x` (com m+a valido) nunca chega a TV no miss nem no hit", async (t) => {
  const req = pedidoHomeBase({
    gosto: { ancoras: [{ id: "tt0200001", titulo: "Filme Aterrado" }] },
  });
  const { post, kv, chamadasLlm } = cenario(t, {}, {
    respostaLlm: () => ({
      v: 1,
      tela: "home",
      origem: "llm",
      pecas: [{
        t: "fileira",
        id: "gosto1",
        titulo: { m: "FT_FIL_PORQUE_TERMINOU", a: ["tt0200001"], x: "Mais como Planeta Imaginário" },
        forma: "cartaz",
        itens: ["c0", "c1", "c2", "c3", "c4", "c5"],
      }],
    }),
  });

  const miss = await post("/v1/farol/compor", req, { token: "tok-gosto-a", perfil: 1 });
  assert.equal(miss.status, 200);
  assert.equal(miss.body.origem, "llm");
  assert.equal(miss.body.pecas[0].titulo.m, "FT_FIL_PORQUE_TERMINOU");
  assert.deepEqual(miss.body.pecas[0].titulo.a, ["tt0200001"]);
  assert.equal(miss.body.pecas[0].titulo.x, undefined);
  assert.ok(!JSON.stringify(miss.body).includes("Planeta Imaginário"));

  const hit = await post("/v1/farol/compor", req, { token: "tok-gosto-b", perfil: 1 });
  assert.equal(hit.status, 200);
  assert.deepEqual(hit.body, miss.body);
  assert.equal(chamadasLlm.length, 1);
  assert.equal(kv.mapa.size, 1);
  assert.ok(![...kv.mapa.values()][0].valor.includes("Planeta Imaginário"));
});

test("R2-P1.3: tema `k<id>` entrega a mesma cena renderizavel no miss e no hit sem gravar literal no KV", async (t) => {
  const req = pedidoHomeBase({
    gosto: {
      temas: [{ id: 4565, nome: "mundos paralelos", peso: 1.5 }],
      ancoras: [{ id: "tt11280740", titulo: "Ruptura" }],
    },
  });
  const { post, kv, chamadasLlm } = cenario(t, {}, {
    respostaLlm: () => ({
      v: 1,
      tela: "home",
      origem: "llm",
      pecas: [{
        t: "fileira",
        id: "gosto1",
        titulo: { m: "FT_FIL_TEMA", a: ["k4565"], x: "Histórias de mundos paralelos" },
        sub: { m: "FT_FIL_SUB_TEMA", a: ["k4565"] },
        forma: "cartaz",
        itens: ["c0", "c1", "c2", "c3", "c4", "c5"],
        motivo: { c0: { m: "FT_MOT_TERMINOU_TEMA", a: ["tt11280740", "k4565"] } },
      }],
    }),
  });

  const miss = await post("/v1/farol/compor", req, { token: "tok-gosto-a", perfil: 1 });
  assert.equal(miss.status, 200);
  assert.equal(miss.body.origem, "llm");
  assert.equal(miss.body.pecas[0].titulo.m, "FT_FIL_TEMA");
  assert.deepEqual(miss.body.pecas[0].titulo.a, ["k4565"]);
  assert.equal(miss.body.pecas[0].titulo.x, "Do mesmo tema: mundos paralelos");
  assert.equal(miss.body.pecas[0].sub.x, "Mais sobre mundos paralelos");
  assert.equal(miss.body.pecas[0].motivo.c0.x, "Você terminou Ruptura e gosta de mundos paralelos.");
  assert.ok(!JSON.stringify(miss.body).includes("Histórias de mundos paralelos"));

  // KV nao guarda `x` nem os literais de tema/ancora.
  assert.equal(kv.mapa.size, 1);
  const kvVal = [...kv.mapa.values()][0].valor;
  assert.ok(!kvVal.includes("\"x\""));
  assert.ok(!kvVal.includes("mundos paralelos"));
  assert.ok(!kvVal.includes("Ruptura"));

  // Hit devolve exatamente a mesma cena renderizavel em 1 unica chamada ao LLM.
  const hit = await post("/v1/farol/compor", req, { token: "tok-gosto-b", perfil: 2 });
  assert.equal(hit.status, 200);
  assert.deepEqual(hit.body, miss.body);
  assert.equal(chamadasLlm.length, 1);

  // Sem `nome` para o tema `k4565`, TV nao consegue renderizar -> recusada igualmente no miss e no hit.
  const reqSemNome = pedidoHomeBase({ gosto: { temas: [{ id: 4565, peso: 1.5 }] } });
  const cSemNome = cenario(t, {}, {
    respostaLlm: () => ({
      v: 1, tela: "home", origem: "llm",
      pecas: [{
        t: "fileira", id: "gosto1",
        titulo: { m: "FT_FIL_TEMA", a: ["k4565"], x: "Histórias de mundos paralelos" },
        forma: "cartaz",
        itens: ["c0", "c1", "c2", "c3", "c4", "c5"],
      }],
    }),
  });
  const rSem = await cSemNome.post("/v1/farol/compor", reqSemNome);
  assert.equal(rSem.status, 200);
  assert.deepEqual(rSem.body, { origem: "regras" });
  assert.equal(cSemNome.kv.mapa.size, 0);
});

test("R2-P1.4: afinidades canonicas (g/k/p e selos) exigem presenca na entrada e no namespace certo", async (t) => {
  const req = pedidoHomeBase({
    gosto: {
      generos: [{ id: 53, nome: "Suspense", peso: 1.5 }],
      temas: [{ id: 4565, nome: "distopia", peso: 1.2 }],
      pessoas: [{ id: 137427, nome: "Denis Villeneuve", peso: 1.6 }],
    },
    candidatos: [
      { id: "tt0114369", titulo: "Seven", ano: 1995, generos: [53], minutos: 127 },
      { id: "tt0133093", titulo: "Matrix", ano: 1999, generos: [878], minutos: 136 },
      { id: "tt0816692", titulo: "Interestelar", ano: 2014, generos: [878], minutos: 169 },
      { id: "tt1375666", titulo: "A Origem", ano: 2010, generos: [878], minutos: 148 },
      { id: "tt2543164", titulo: "A Chegada", ano: 2016, generos: [878], minutos: 116 },
      { id: "tt1856101", titulo: "Blade Runner 2049", ano: 2017, generos: [878], minutos: 164 },
    ],
  });

  const ruins = [
    // Pessoa ausente da entrada (caso do relatorio)
    { v: 1, tela: "home", pecas: [{ t: "fileira", id: "gosto1", titulo: { m: "FT_FIL_HORA" }, forma: "cartaz", itens: ["c0", "c1", "c2", "c3", "c4", "c5"], motivo: { c0: { m: "FT_MOT_PESSOA", a: ["p987654321"] } } }] },
    // Namespace trocado: id de pessoa (137427) usado como genero ou tema
    { v: 1, tela: "home", pecas: [{ t: "fileira", id: "gosto1", titulo: { m: "FT_FIL_GENERO", a: ["g137427"] }, forma: "cartaz", itens: ["c0", "c1", "c2", "c3", "c4", "c5"] }] },
    { v: 1, tela: "home", pecas: [{ t: "fileira", id: "gosto1", titulo: { m: "FT_FIL_TEMA", a: ["k137427"] }, forma: "cartaz", itens: ["c0", "c1", "c2", "c3", "c4", "c5"] }] },
    // Namespace trocado: id de tema (4565) usado como pessoa
    { v: 1, tela: "home", pecas: [{ t: "fileira", id: "gosto1", titulo: { m: "FT_FIL_PESSOA", a: ["p4565"] }, forma: "cartaz", itens: ["c0", "c1", "c2", "c3", "c4", "c5"] }] },
    // Selo fora da entrada em `porque`
    { v: 1, tela: "home", pecas: [{ t: "porque", alvo: "c0", confianca: 2, linhas: [{ m: "FT_MOT_GENERO", a: ["g53"] }], selos: [{ k: "pessoa", id: 987654321 }] }] },
  ];

  for (const ruim of ruins) {
    const { post, kv } = cenario(t, {}, { respostaLlm: () => ruim });
    const r = await post("/v1/farol/compor", req);
    assert.equal(r.status, 200);
    assert.deepEqual(r.body, { origem: "regras" }, JSON.stringify(ruim));
    assert.equal(kv.mapa.size, 0);
  }

  // Afinidade vinda de candidato.motivos e aceita no namespace correto.
  const reqMotivo = pedidoHomeBase({
    gosto: { generos: [{ id: 53, nome: "Suspense", peso: 1.5 }] },
    candidatos: req.candidatos.map((c, idx) => (idx === 0 ? { ...c, motivos: ["p777001"] } : c)),
  });
  const { post } = cenario(t, {}, {
    respostaLlm: () => ({
      v: 1, tela: "home", origem: "llm",
      pecas: [{
        t: "fileira", id: "gosto1", titulo: { m: "FT_FIL_HORA" }, forma: "cartaz",
        itens: ["c0", "c1", "c2", "c3", "c4", "c5"],
        motivo: { c0: { m: "FT_MOT_PESSOA", a: ["p777001"] } },
      }],
    }),
  });
  const okMot = await post("/v1/farol/compor", reqMotivo);
  assert.equal(okMot.status, 200);
  assert.equal(okMot.body.origem, "llm");
});

test("R2-P1.5: tipo canonico e namespace de id participam da chave de cache", async (t) => {
  assert.equal(typeof farolGostoMod.tipoCanonico, "function");
  assert.equal(farolGostoMod.tipoCanonico("filme"), "filme");
  assert.equal(farolGostoMod.tipoCanonico("movie"), "filme");
  assert.equal(farolGostoMod.tipoCanonico("serie"), "serie");
  assert.equal(farolGostoMod.tipoCanonico("series"), "serie");

  const mkReq = (tipo, id = 321) => pedidoHomeBase({
    modo: "dica",
    candidatos: [{ c: "c0", id, tipo }],
  });

  const kFilme = await chaveCacheCompor(mkReq("filme", 321));
  const kMovie = await chaveCacheCompor(mkReq("movie", 321));
  const kSerie = await chaveCacheCompor(mkReq("serie", 321));
  const kSeries = await chaveCacheCompor(mkReq("series", 321));
  const kFilmeStr = await chaveCacheCompor(mkReq("filme", "321"));

  assert.equal(kFilme, kMovie, "filme e movie sao o mesmo tipo canonico");
  assert.equal(kSerie, kSeries, "serie e series sao o mesmo tipo canonico");
  assert.notEqual(kFilme, kSerie, "filme e serie com mesmo id 321 devem ter chaves distintas");
  assert.notEqual(kFilme, kFilmeStr, "id numerico e string devem ter namespaces distintos");

  let n = 0;
  const { post, chamadasLlm } = cenario(t, {}, {
    respostaLlm: () => {
      n++;
      return {
        v: 1, tela: "home", origem: "llm",
        pecas: [{
          t: "dica", id: `d_${n}`,
          l1: { m: n === 1 ? "FT_DICA_CURTO" : "FT_DICA_TEMPORADA", a: ["c0"] },
          acao: { tipo: "abrir", alvo: "c0" },
        }],
      };
    },
  });

  const rFilme = await post("/v1/farol/compor", mkReq("filme"), { token: "tok-gosto-a", perfil: 1 });
  assert.equal(rFilme.status, 200);
  assert.equal(rFilme.body.pecas[0].l1.m, "FT_DICA_CURTO");

  const rSerie = await post("/v1/farol/compor", mkReq("serie"), { token: "tok-gosto-b", perfil: 1 });
  assert.equal(rSerie.status, 200);
  assert.equal(rSerie.body.pecas[0].l1.m, "FT_DICA_TEMPORADA");
  assert.equal(chamadasLlm.length, 2);
});

test("R2-KV-Legado: nunca devolve JSON bruto do KV e rejeita entradas legadas com `x` ou contrato invalido", async (t) => {
  const req = pedidoHomeBase();
  const chave = await chaveCacheCompor(req);

  const entradasLegadas = [
    // Entrada legada com `x` reprovado + `m` valido (ressalva do relatorio r2)
    { v: 1, tela: "home", origem: "llm", pecas: [{ t: "fileira", id: "gosto1", titulo: { m: "FT_FIL_HORA", x: "https://exemplo.invalid" }, forma: "cartaz", itens: ["c0", "c1", "c2", "c3", "c4", "c5"] }] },
    // Entrada legada com `x` valido guardado no KV
    { v: 1, tela: "home", origem: "llm", pecas: [{ t: "fileira", id: "gosto1", titulo: { m: "FT_FIL_HORA", x: "Texto legado em KV" }, forma: "cartaz", itens: ["c0", "c1", "c2", "c3", "c4", "c5"] }] },
    // Entrada legada com afinidade ausente do pedido atual
    { v: 1, tela: "home", origem: "llm", pecas: [{ t: "fileira", id: "gosto1", titulo: { m: "FT_FIL_HORA" }, forma: "cartaz", itens: ["c0", "c1", "c2", "c3", "c4", "c5"], motivo: { c0: { m: "FT_MOT_PESSOA", a: ["p987654321"] } } }] },
    // Entrada legada com `tela` divergente do modo
    { v: 1, tela: "comparar", origem: "llm", pecas: [{ t: "fileira", id: "gosto1", titulo: { m: "FT_FIL_HORA" }, forma: "cartaz", itens: ["c0", "c1", "c2", "c3", "c4", "c5"] }] },
  ];

  for (const leg of entradasLegadas) {
    const { kv, post, chamadasLlm } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [{
          t: "fileira", id: "gosto1", titulo: { m: "FT_FIL_HORA" },
          forma: "cartaz", itens: ["c0", "c1", "c2", "c3", "c4", "c5"],
        }],
      }),
    });
    await kv.put(chave, JSON.stringify(leg), { expirationTtl: 86400 });
    const r = await post("/v1/farol/compor", req);
    assert.equal(r.status, 200);
    assert.equal(r.body.origem, "llm");
    assert.equal(r.body.tela, "home");
    assert.equal(r.body.pecas[0].titulo.x, undefined);
    assert.ok(!JSON.stringify(r.body).includes("exemplo.invalid"));
    assert.ok(!JSON.stringify(r.body).includes("Texto legado"));
    assert.equal(chamadasLlm.length, 1, `entrada legada devia ter sido rejeitada: ${JSON.stringify(leg)}`);
  }
});
