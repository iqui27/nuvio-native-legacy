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
  dicaIdCanonico,
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
      { id: "tt2543164", titulo: "A Chegada", ano: 2016, generos: [878, 18], minutos: 116, motivos: [{ tipo: "FMO_PESSOA", a: 137427, b: 0 }] },
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
    const r = await post("/v1/farol/compor", pedidoHomeBase({ modo: "comparar", fatos: [{ tempo_max: 150 }] }));
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
      { id: "tt0114369", titulo: canarioTitulo, ano: 1995, generos: [53] },
      { id: "tt0133093", titulo: "Matrix", ano: 1999 },
      { id: "tt0816692", titulo: "Interestelar", ano: 2014 },
      { id: "tt1375666", titulo: "A Origem", ano: 2010 },
      { id: "tt2543164", titulo: "A Chegada", ano: 2016, motivos: [{ tipo: "FMO_PESSOA", a: 137427, b: 0 }] },
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
    fatos: [{ alvo: "tt0903747", temporada: 2, episodio: 4, faltam: 3 }],
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
    fatos: [{
      alvo: "tt0903747",
      titulo: "Breaking Bad",
      onde: "Netflix",
      salvo: "5M",
      minutos: 92,
      termina: "23:40",
      temporada: 2,
      episodio: 4,
      retomada: "7w",
    }],
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
  const baseCands = pedidoHomeBase().candidatos;
  const req = pedidoHomeBase({
    gosto: {
      temas: [{ id: 4565, nome: "mundos paralelos", peso: 1.5 }],
      ancoras: [{ id: "tt11280740", titulo: "Ruptura" }],
    },
    candidatos: baseCands.map((c, idx) =>
      idx === 0 ? { ...c, motivos: [{ tipo: "FMO_TERMINOU", a: "tt11280740", b: 4565 }] } : c
    ),
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
    candidatos: [{ c: "c0", id, tipo, minutos: 80, motivos: [{ tipo: 6, a: 80, b: 0 }] }],
    fatos: [{ c: 0, nova_temporada: true }],
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

// =============================================================================
// 9. Regressoes dos 4 P1 da revisao FAROL40-r3 e contrato de serializacao 52
// =============================================================================

test("R3-P1.1: motivo pertence ao candidato correto no miss, na chave KV e no hit (incluindo FarolMotivo numerico)", async (t) => {
  const candsBase = [
    { id: "tt0114369", titulo: "Seven", ano: 1995, generos: [53, 80], minutos: 127 },
    { id: "tt0133093", titulo: "Matrix", ano: 1999, generos: [878, 28], minutos: 136 },
    { id: "tt0816692", titulo: "Interestelar", ano: 2014, generos: [878, 18], minutos: 169 },
    { id: "tt1375666", titulo: "A Origem", ano: 2010, generos: [878, 53], minutos: 148 },
    { id: "tt2543164", titulo: "A Chegada", ano: 2016, generos: [878, 18], minutos: 116 },
    { id: "tt1856101", titulo: "Blade Runner 2049", ano: 2017, generos: [878, 18], minutos: 164 },
  ];

  // 1. Sonda exata da r3: c0=Seven sem pessoa, c1=Matrix com FMO_PESSOA 137427, sem gosto.pessoas.
  //    motivo.c0 = FT_MOT_PESSOA(p137427) tem de ser RECUSADO no miss (nao cai em Seven).
  const reqB = pedidoHomeBase({
    gosto: { generos: [{ id: 53, nome: "Suspense", peso: 1.5 }], nivel: 2 },
    candidatos: candsBase.map((c, i) => (i === 1 ? { ...c, motivos: [{ tipo: "FMO_PESSOA", a: 137427, b: 0 }] } : c)),
  });
  {
    const { post, kv } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [{
          t: "fileira", id: "gosto1", titulo: { m: "FT_FIL_HORA" }, forma: "cartaz",
          itens: ["c0", "c1", "c2", "c3", "c4", "c5"],
          motivo: { c0: { m: "FT_MOT_PESSOA", a: ["p137427"] } },
        }],
      }),
    });
    const r = await post("/v1/farol/compor", reqB);
    assert.equal(r.status, 200);
    assert.deepEqual(r.body, { origem: "regras" });
    assert.equal(kv.mapa.size, 0);
  }

  // Mesmo com gosto.pessoas contendo 137427, c0 sem evidencia propria nao pode receber p137427; c1 pode.
  const reqComGostoPessoa = pedidoHomeBase({
    gosto: { pessoas: [{ id: 137427, nome: "Denis Villeneuve", peso: 1.8 }], nivel: 2 },
    candidatos: candsBase.map((c, i) => (i === 1 ? { ...c, motivos: [{ tipo: 2, a: 137427, b: 0 }] } : c)),
  });
  {
    const { post } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [{
          t: "fileira", id: "gosto1", titulo: { m: "FT_FIL_HORA" }, forma: "cartaz",
          itens: ["c0", "c1", "c2", "c3", "c4", "c5"],
          motivo: { c0: { m: "FT_MOT_PESSOA", a: ["p137427"] } },
        }],
      }),
    });
    const r = await post("/v1/farol/compor", reqComGostoPessoa);
    assert.deepEqual(r.body, { origem: "regras" });
  }
  {
    const { post } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [{
          t: "fileira", id: "gosto1", titulo: { m: "FT_FIL_HORA" }, forma: "cartaz",
          itens: ["c0", "c1", "c2", "c3", "c4", "c5"],
          motivo: { c1: { m: "FT_MOT_PESSOA", a: ["p137427"] } },
        }],
      }),
    });
    const r = await post("/v1/farol/compor", reqComGostoPessoa);
    assert.equal(r.body.origem, "llm");
    assert.equal(r.body.pecas[0].motivo.c1.m, "FT_MOT_PESSOA");
  }

  // 2. Conta A (evidencia em c0) vs Conta B (evidencia so em c1): chaves distintas e zero hit cruzado.
  const reqA = pedidoHomeBase({
    gosto: { generos: [{ id: 53, nome: "Suspense", peso: 1.5 }], nivel: 2 },
    candidatos: candsBase.map((c, i) => (i === 0 ? { ...c, motivos: [{ tipo: "FMO_PESSOA", a: 137427, b: 0 }] } : c)),
  });
  const kA = await chaveCacheCompor(reqA);
  const kB = await chaveCacheCompor(reqB);
  assert.notEqual(kA, kB, "evidencia em c0 vs c1 deve gerar chaves KV distintas");

  {
    const { post, kv, chamadasLlm } = cenario(t, {}, {
      respostaLlm: (reqBody) => {
        const user = JSON.parse(reqBody.messages[1].content);
        const temEmC0 = Array.isArray(user.candidatos[0].motivos) && user.candidatos[0].motivos.length > 0;
        return {
          v: 1, tela: "home", origem: "llm",
          pecas: [{
            t: "fileira", id: "gosto1", titulo: { m: "FT_FIL_HORA" }, forma: "cartaz",
            itens: ["c0", "c1", "c2", "c3", "c4", "c5"],
            motivo: temEmC0
              ? { c0: { m: "FT_MOT_PESSOA", a: ["p137427"] } }
              : { c1: { m: "FT_MOT_PESSOA", a: ["p137427"] } },
          }],
        };
      },
    });
    const rA = await post("/v1/farol/compor", reqA, { token: "tok-gosto-a", perfil: 1 });
    assert.equal(rA.body.origem, "llm");
    assert.ok(rA.body.pecas[0].motivo.c0);
    const rB = await post("/v1/farol/compor", reqB, { token: "tok-gosto-b", perfil: 1 });
    assert.equal(rB.body.origem, "llm");
    assert.equal(rB.body.pecas[0].motivo.c0, undefined);
    assert.ok(rB.body.pecas[0].motivo.c1);
    assert.equal(chamadasLlm.length, 2);

    // Mesmo injetando cena de A sob a chave de B no KV, hit revalida por candidato e recompoe em B.
    const valA = kv.mapa.get(kA).valor;
    await kv.put(kB, valA, { expirationTtl: 86400 });
    const rB2 = await post("/v1/farol/compor", reqB, { token: "tok-gosto-b", perfil: 2 });
    assert.equal(rB2.body.origem, "llm");
    assert.equal(rB2.body.pecas[0].motivo.c0, undefined);
    assert.ok(rB2.body.pecas[0].motivo.c1);
    assert.equal(chamadasLlm.length, 3);
  }

  // 3. Adjacentes: `porque` e selo no candidato errado + motivo por chave `tt...` + numeric FarolMotivo 0..9.
  {
    const ruinsPorque = [
      { v: 1, tela: "home", pecas: [{ t: "porque", alvo: "c0", confianca: 2, linhas: [{ m: "FT_MOT_PESSOA", a: ["p137427"] }] }] },
      { v: 1, tela: "home", pecas: [{ t: "porque", alvo: "c0", confianca: 2, linhas: [{ m: "FT_MOT_GENERO", a: ["g53"] }], selos: [{ k: "pessoa", id: 137427 }] }] },
      { v: 1, tela: "home", pecas: [{ t: "fileira", id: "gosto1", titulo: { m: "FT_FIL_HORA" }, forma: "cartaz", itens: ["c0", "c1", "c2", "c3", "c4", "c5"], motivo: { tt0114369: { m: "FT_MOT_PESSOA", a: ["p137427"] } } }] },
    ];
    for (const ruim of ruinsPorque) {
      const { post, kv } = cenario(t, {}, { respostaLlm: () => ruim });
      const r = await post("/v1/farol/compor", reqB);
      assert.deepEqual(r.body, { origem: "regras" }, JSON.stringify(ruim));
      assert.equal(kv.mapa.size, 0);
    }
  }

  // Todos os enum numericos C FarolMotivoTipo (0..9) funcionam no candidato correto.
  {
    const reqTodosFmo = pedidoHomeBase({
      gosto: {
        generos: [{ id: 53, nome: "Suspense", peso: 1.5 }],
        temas: [{ id: 4565, nome: "distopia", peso: 1.3 }],
        pessoas: [{ id: 137427, nome: "Denis Villeneuve", peso: 1.7 }],
        ancoras: [{ id: "tt11280740", titulo: "Ruptura" }],
      },
      candidatos: [
        { ...candsBase[0], minutos: 127, motivos: [
          { tipo: 0, a: 0, b: 4565 },       // FMO_TERMINOU(ancora[0], tema 4565)
          { tipo: 1, a: 53, b: 0 },         // FMO_GENERO
          { tipo: 2, a: 137427, b: 0 },     // FMO_PESSOA
          { tipo: 6, a: 127, b: 0 },        // FMO_CABE_HOJE
        ] },
        { ...candsBase[1], motivos: [{ tipo: 3, a: 4565, b: 0 }, { tipo: 4, a: 0, b: 80 }] },
        { ...candsBase[2], motivos: [{ tipo: 5, a: 150, b: 0 }, { tipo: 7, a: 0, b: 0 }] },
        { ...candsBase[3], motivos: [{ tipo: 8, a: 53, b: 0 }, { tipo: 9, a: 0, b: 0 }] },
        candsBase[4],
        candsBase[5],
      ],
    });
    const { post } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [{
          t: "fileira", id: "gosto1",
          titulo: { m: "FT_FIL_PORQUE_TERMINOU", a: ["tt11280740"] },
          forma: "cartaz",
          itens: ["c0", "c1", "c2", "c3", "c4", "c5"],
          motivo: {
            c0: { m: "FT_MOT_TERMINOU_TEMA", a: ["tt11280740", "k4565"] },
            c1: { m: "FT_MOT_AMIGO_ANON" },
            c2: { m: "FT_MOT_SALVO", a: ["5M"] },
            c3: { m: "FT_MOT_FORA_CURVA", a: ["g53"] },
          },
        }],
      }),
    });
    const r = await post("/v1/farol/compor", reqTodosFmo);
    assert.equal(r.body.origem, "llm");
    assert.equal(r.body.pecas[0].motivo.c0.m, "FT_MOT_TERMINOU_TEMA");
    assert.equal(r.body.pecas[0].motivo.c2.a[0], "5M");
    assert.equal(r.body.pecas[0].motivo.c3.m, "FT_MOT_FORA_CURVA");
  }
});

test("R3-P1.2: numeros, duracoes e quantidades exigem correspondencia exata aos fatos ou motivos do item", async (t) => {
  const cands = pedidoHomeBase().candidatos.map((c, idx) =>
    idx === 0 ? { ...c, minutos: 127, motivos: [{ tipo: "FMO_CABE_HOJE", a: 127, b: 0 }] } : c
  );
  const req127 = pedidoHomeBase({ candidatos: cands });

  // 1. Sonda exata da r3: Seven com minutos=127 e FMO_CABE_HOJE=127 nao pode aceitar "20".
  {
    const { post, kv } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [{
          t: "fileira", id: "gosto1", titulo: { m: "FT_FIL_HORA" }, forma: "cartaz",
          itens: ["c0", "c1", "c2", "c3", "c4", "c5"],
          motivo: { c0: { m: "FT_MOT_CABE_HOJE", a: ["20"] } },
        }],
      }),
    });
    const r = await post("/v1/farol/compor", req127);
    assert.deepEqual(r.body, { origem: "regras" });
    assert.equal(kv.mapa.size, 0);
  }

  // Hit legado no KV com "20" sob minutos=127 tambem e rejeitado e recomposto.
  {
    const chave127 = await chaveCacheCompor(req127);
    const { kv, post, chamadasLlm } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [{
          t: "fileira", id: "gosto1", titulo: { m: "FT_FIL_HORA" }, forma: "cartaz",
          itens: ["c0", "c1", "c2", "c3", "c4", "c5"],
          motivo: { c0: { m: "FT_MOT_CABE_HOJE", a: ["127"] } },
        }],
      }),
    });
    await kv.put(chave127, JSON.stringify({
      v: 1, tela: "home", feita: 1795000000, vale_s: 86400, origem: "llm",
      pecas: [{
        t: "fileira", id: "gosto1", titulo: { m: "FT_FIL_HORA" }, forma: "cartaz",
        itens: ["c0", "c1", "c2", "c3", "c4", "c5"],
        motivo: { c0: { m: "FT_MOT_CABE_HOJE", a: ["20"] } },
      }],
    }), { expirationTtl: 86400 });
    const r = await post("/v1/farol/compor", req127);
    assert.equal(r.body.origem, "llm");
    assert.equal(r.body.pecas[0].motivo.c0.a[0], "127");
    assert.equal(chamadasLlm.length, 1);
  }

  // Mudanca em minutos/motivo altera a chave de cache.
  const req95 = pedidoHomeBase({
    candidatos: cands.map((c, idx) => (idx === 0 ? { ...c, minutos: 95, motivos: [{ tipo: "FMO_CABE_HOJE", a: 95, b: 0 }] } : c)),
  });
  assert.notEqual(await chaveCacheCompor(req127), await chaveCacheCompor(req95));

  // 2. Casos adjacentes de numeros, duracoes e quantidades em dica, info, motivo, achados e escolha.
  const reqFatos = pedidoHomeBase({
    candidatos: cands.map((c, idx) =>
      idx === 0 ? { ...c, minutos: 92, motivos: [{ tipo: "FMO_SALVO", a: 150, b: 0 }, { tipo: "FMO_PESSOA", a: 137427, b: "tt11280740", vezes: 3 }] }
        : idx === 1 ? { ...c, minutos: 80 }
        : c
    ),
    fatos: [
      {
        alvo: "tt0903747",
        temporada: 2,
        episodio: 4,
        faltam: 3,
        semanas: 5,
        salvo: "5M",
        minutos: 92,
        min_ep: 45,
        min_noite: 50,
        noites: 2,
        termina: "23:40",
        onde: "Netflix",
      },
      {
        alvo: "tt0903747",
        pos: 2,
        total: 3,
        vistos: 1,
        rever_data: "2024-03",
        estrelas: 4,
        retomada: "7w",
      },
    ],
  });

  const cenasContraditorias = [
    // FT_MOT_DIRETOR com contagem inventada (atestado=3, devolve=9)
    { modo: "home", cena: { v: 1, tela: "home", pecas: [{ t: "fileira", id: "gosto1", titulo: { m: "FT_FIL_HORA" }, forma: "cartaz", itens: ["c0", "c1", "c2", "c3", "c4", "c5"], motivo: { c0: { m: "FT_MOT_DIRETOR", a: ["tt11280740", "9"] } } }] } },
    // FT_MOT_SALVO com duracao inventada (atestado=5M, devolve=9y)
    { modo: "home", cena: { v: 1, tela: "home", pecas: [{ t: "fileira", id: "gosto1", titulo: { m: "FT_FIL_HORA" }, forma: "cartaz", itens: ["c0", "c1", "c2", "c3", "c4", "c5"], motivo: { c0: { m: "FT_MOT_SALVO", a: ["9y"] } } }] } },
    // FT_DICA_SALVO_L2 contradizendo minutos=92 (devolve "2h00")
    { modo: "home", cena: { v: 1, tela: "home", pecas: [{ t: "dica", id: "d1", l1: { m: "FT_DICA_SALVO_L1", a: ["5M"] }, l2: { m: "FT_DICA_SALVO_L2", a: ["2h00"] }, acao: { tipo: "abrir", alvo: "c0" } }] } },
    // FT_DICA_PAROU_L1 com episodio inventado (atestado=T2E4, devolve T2E9)
    { modo: "home", cena: { v: 1, tela: "home", pecas: [{ t: "dica", id: "d1", l1: { m: "FT_DICA_PAROU_L1", a: ["tt0903747", "2", "9"] }, acao: { tipo: "abrir", alvo: "tt0903747" } }] } },
    // FT_DICA_FALTA_L1 com faltam inventado (atestado=3, devolve 8)
    { modo: "home", cena: { v: 1, tela: "home", pecas: [{ t: "dica", id: "d1", l1: { m: "FT_DICA_FALTA_L1", a: ["8", "2", "tt0903747"] }, acao: { tipo: "abrir", alvo: "tt0903747" } }] } },
    // FT_INFO_TERMINA com hora inventada (atestado=23:40, devolve 22:15)
    { modo: "retomada", cena: { v: 1, tela: "detalhe", pecas: [{ t: "info", alvo: "tt0903747", itens: [{ k: "tempo", l: { m: "FT_INFO_TERMINA", a: ["22:15"] } }] }] } },
    // FT_INFO_REVER com nota inventada (atestado=4, devolve 5)
    { modo: "retomada", cena: { v: 1, tela: "detalhe", pecas: [{ t: "info", alvo: "tt0903747", itens: [{ k: "rever", l: { m: "FT_INFO_REVER", a: ["2024-03", "5"] } }] }] } },
    // FT_INFO_SAGA com total inventado (atestado=3, devolve 5)
    { modo: "retomada", cena: { v: 1, tela: "detalhe", pecas: [{ t: "info", alvo: "tt0903747", itens: [{ k: "saga", l: { m: "FT_INFO_SAGA", a: ["2", "5"] } }] }] } },
    // FT_ACHA_CURTOS_L1 com quantidade divergente (2 itens, diz "3") ou duracao maior que o corte (c0=92 >= 90)
    { modo: "home", cena: { v: 1, tela: "home", pecas: [{ t: "achados", l1: { m: "FT_ACHA_CURTOS_L1", a: ["3", "100"] }, itens: ["c0", "c1"] }] } },
    { modo: "home", cena: { v: 1, tela: "home", pecas: [{ t: "achados", l1: { m: "FT_ACHA_CURTOS_L1", a: ["2", "90"] }, itens: ["c0", "c1"] }] } },
    // FT_ESC_PASSO divergente de passo/de
    { modo: "home", cena: { v: 1, tela: "home", pecas: [{ t: "escolha", passo: 1, de: 3, pergunta: { m: "FT_ESC_PASSO", a: ["2", "3"] }, opcoes: [{ id: "t45", r: { m: "FT_ESC_TEMPO_45" } }, { id: "tanto", r: { m: "FT_ESC_TANTO_FAZ" } }] }] } },
    // FT_CMP_VEREDITO_CABE citando item fora de comparar.itens
    { modo: "comparar", cena: { v: 1, tela: "comparar", pecas: [{ t: "comparar", itens: ["c0", "c1"], eixos: ["duracao"], veredito: { m: "FT_CMP_VEREDITO_CABE", a: ["c2"] } }] } },
  ];
  for (const caso of cenasContraditorias) {
    const { post, kv } = cenario(t, {}, { respostaLlm: () => caso.cena });
    const r = await post("/v1/farol/compor", { ...reqFatos, modo: caso.modo });
    assert.deepEqual(r.body, { origem: "regras" }, JSON.stringify(caso.cena));
    assert.equal(kv.mapa.size, 0);
  }

  // Casos verdadeiros correspondentes passam.
  const cenasVerdadeiras = [
    { modo: "home", cena: { v: 1, tela: "home", pecas: [{ t: "achados", l1: { m: "FT_ACHA_CURTOS_L1", a: ["2", "100"] }, itens: ["c0", "c1"], motivo: { c0: { m: "FT_MOT_DIRETOR", a: ["tt11280740", "3"] } } }] } },
    { modo: "retomada", cena: { v: 1, tela: "detalhe", pecas: [{ t: "info", alvo: "tt0903747", itens: [{ k: "tempo", l: { m: "FT_INFO_TERMINA", a: ["23:40"] } }, { k: "rever", l: { m: "FT_INFO_REVER", a: ["2024-03", "4"] } }, { k: "saga", l: { m: "FT_INFO_SAGA_VOCE_VIU", a: ["2", "3", "1"] } }] }] } },
  ];
  for (const caso of cenasVerdadeiras) {
    const { post } = cenario(t, {}, { respostaLlm: () => caso.cena });
    const r = await post("/v1/farol/compor", { ...reqFatos, modo: caso.modo });
    assert.equal(r.body.origem, "llm", JSON.stringify(caso.cena));
  }
});

test("R3-P1.3: dica.id canonico, dica.icone no contrato do cliente e revalidacao de KV legado", async (t) => {
  const canario = "canariotituloprivado";
  const req = pedidoHomeBase({
    candidatos: pedidoHomeBase().candidatos.map((c, i) =>
      (i === 0 ? { ...c, titulo: canario, minutos: 80, motivos: [{ tipo: 6, a: 80, b: 0 }] } : c)
    ),
  });

  // 1. Sonda exata da r3: dica com id e icone iguais ao titulo canario -> recusado, zero KV.
  {
    const { post, kv } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [{
          t: "dica",
          id: canario,
          icone: canario,
          l1: { m: "FT_DICA_CURTO", a: ["c0"] },
          acao: { tipo: "abrir", alvo: "c0" },
        }],
      }),
    });
    const r = await post("/v1/farol/compor", req);
    assert.deepEqual(r.body, { origem: "regras" });
    assert.equal(kv.mapa.size, 0);
  }

  // 2. LLM devolve id arbitrario com icone valido (`aj_farol`): id e canonicalizado, literal nao vaza no retorno nem no KV.
  {
    const { post, kv, chamadasLlm } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [{
          t: "dica",
          id: canario,
          icone: "aj_farol",
          l1: { m: "FT_DICA_CURTO", a: ["c0"] },
          acao: { tipo: "abrir", alvo: "c0" },
        }],
      }),
    });
    const DICA_ICONES_CLIENTE = farolGostoMod.DICA_ICONES_CLIENTE;
    const dicaIdCanonico = farolGostoMod.dicaIdCanonico;
    assert.ok(DICA_ICONES_CLIENTE instanceof Set);
    assert.equal(typeof dicaIdCanonico, "function");
    assert.equal(DICA_ICONES_CLIENTE.has("aj_farol"), true);
    assert.equal(DICA_ICONES_CLIENTE.has("aj_clock"), true);
    assert.equal(DICA_ICONES_CLIENTE.has(canario), false);

    const miss = await post("/v1/farol/compor", req, { token: "tok-gosto-a", perfil: 1 });
    assert.equal(miss.body.origem, "llm");
    const esperadaId = dicaIdCanonico(miss.body.pecas[0]);
    assert.equal(miss.body.pecas[0].id, esperadaId);
    assert.match(esperadaId, /^d_curto_[0-9a-f]{8}$/);
    assert.equal(miss.body.pecas[0].icone, "aj_farol");
    assert.ok(!JSON.stringify(miss.body).includes(canario));

    const [[, entradaKv]] = [...kv.mapa.entries()];
    assert.ok(!entradaKv.valor.includes(canario));
    assert.equal(JSON.parse(entradaKv.valor).pecas[0].id, esperadaId);

    const hit = await post("/v1/farol/compor", req, { token: "tok-gosto-b", perfil: 1 });
    assert.deepEqual(hit.body, miss.body);
    assert.equal(chamadasLlm.length, 1);
  }

  // 3. Entradas legadas no KV com dica.id nao canonico ("d1" / canario) ou icone fora do contrato sao rejeitadas e recompostas.
  const chave = await chaveCacheCompor(req);
  const dicaCanon = farolGostoMod.dicaIdCanonico({ t: "dica", l1: { m: "FT_DICA_CURTO", a: ["c0"] }, acao: { tipo: "abrir", alvo: "c0" } });
  for (const leg of [
    { v: 1, tela: "home", feita: 1795000000, vale_s: 86400, origem: "llm", pecas: [{ t: "dica", id: canario, l1: { m: "FT_DICA_CURTO", a: ["c0"] }, acao: { tipo: "abrir", alvo: "c0" }, icone: "aj_farol" }] },
    { v: 1, tela: "home", feita: 1795000000, vale_s: 86400, origem: "llm", pecas: [{ t: "dica", id: "d1", l1: { m: "FT_DICA_CURTO", a: ["c0"] }, acao: { tipo: "abrir", alvo: "c0" }, icone: "aj_farol" }] },
    { v: 1, tela: "home", feita: 1795000000, vale_s: 86400, origem: "llm", pecas: [{ t: "dica", id: dicaCanon, l1: { m: "FT_DICA_CURTO", a: ["c0"] }, acao: { tipo: "abrir", alvo: "c0" }, icone: canario }] },
  ]) {
    const { kv, post, chamadasLlm } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [{ t: "dica", id: "d_ok", l1: { m: "FT_DICA_CURTO", a: ["c0"] }, acao: { tipo: "abrir", alvo: "c0" }, icone: "aj_clock" }],
      }),
    });
    await kv.put(chave, JSON.stringify(leg), { expirationTtl: 86400 });
    const r = await post("/v1/farol/compor", req);
    assert.equal(r.body.origem, "llm");
    assert.equal(r.body.pecas[0].icone, "aj_clock");
    assert.ok(!JSON.stringify(r.body).includes(canario));
    assert.equal(chamadasLlm.length, 1);
  }
});

test("R3-P1.4: reconstrucao deterministica de nome de pessoa ausente do elenco local com paridade miss/hit e KV limpo", async (t) => {
  const cands = pedidoHomeBase().candidatos.map((c, i) =>
    i === 0 ? { ...c, motivos: [{ tipo: "FMO_PESSOA", a: 600000, b: 0 }] } : c
  );
  const req = pedidoHomeBase({
    gosto: { pessoas: [{ id: 600000, nome: "Diretora B", peso: 1.8 }], nivel: 2 },
    candidatos: cands,
  });

  const { post, kv, chamadasLlm } = cenario(t, {}, {
    respostaLlm: () => ({
      v: 1, tela: "home", origem: "llm",
      pecas: [{
        t: "fileira",
        id: "gosto1",
        titulo: { m: "FT_FIL_PESSOA", a: ["p600000"], x: "Da mesma pessoa: Diretora B" },
        forma: "cartaz",
        itens: ["c0", "c1", "c2", "c3", "c4", "c5"],
        motivo: { c0: { m: "FT_MOT_PESSOA", a: ["p600000"] } },
      }],
    }),
  });

  const miss = await post("/v1/farol/compor", req, { token: "tok-gosto-a", perfil: 1 });
  assert.equal(miss.status, 200);
  assert.equal(miss.body.origem, "llm");
  assert.equal(miss.body.pecas[0].titulo.m, "FT_FIL_PESSOA");
  assert.deepEqual(miss.body.pecas[0].titulo.a, ["p600000"]);
  assert.equal(miss.body.pecas[0].titulo.x, "Com Diretora B, de quem você gosta");
  assert.equal(miss.body.pecas[0].motivo.c0.x, "Com Diretora B, que você acompanha.");
  assert.ok(!JSON.stringify(miss.body).includes("Da mesma pessoa"));

  // KV nao persiste `x` nem o nome "Diretora B".
  assert.equal(kv.mapa.size, 1);
  const kvVal = [...kv.mapa.values()][0].valor;
  assert.ok(!kvVal.includes("\"x\""));
  assert.ok(!kvVal.includes("Diretora B"));

  // Hit devolve exatamente o mesmo JSON com 1 unica chamada ao LLM.
  const hit = await post("/v1/farol/compor", req, { token: "tok-gosto-b", perfil: 2 });
  assert.deepEqual(hit.body, miss.body);
  assert.equal(chamadasLlm.length, 1);

  // Paridade nos idiomas distintos (ex.: en, ja, zh-tw).
  for (const [idioma, espTit, espMot] of [
    ["en", "With Diretora B, whose work you like", "With Diretora B, who you follow."],
    ["ja", "お気に入りの Diretora B", "Diretora B — フォローしている人"],
    ["zh-TW", "你喜歡的 Diretora B", "Diretora B——你關注的人"],
  ]) {
    const rLang = await post("/v1/farol/compor", { ...req, idioma });
    assert.equal(rLang.body.origem, "llm");
    assert.equal(rLang.body.pecas[0].titulo.x, espTit);
    assert.equal(rLang.body.pecas[0].motivo.c0.x, espMot);
  }
});

// =============================================================================
// 10. Regressoes dos 4 P1 da revisao FAROL40-r4
// =============================================================================

test("R4-P1.1: dica vincula l1, l2 e acao a mesma identidade e FT_FIL_PORQUE_TERMINOU exige ancora da fileira ou de gosto sem vazar outro candidato", async (t) => {
  const reqDica = pedidoHomeBase({
    modo: "dica",
    fatos: [
      { alvo: "c0", temporada: 2, episodio: 4, semanas: 5 },
      { alvo: "c1", temporada: 3, episodio: 7, semanas: 9 },
    ],
  });

  // 1. Reproducao exata da r4: l1 aponta c0, l2 usa semanas=9 de c1, acao abre c1 -> recusada no miss e no hit KV.
  for (const cenaRuim of [
    {
      v: 1, tela: "home", origem: "llm",
      pecas: [{
        t: "dica", id: "d1",
        l1: { m: "FT_DICA_PAROU_L1", a: ["c0", "2", "4"] },
        l2: { m: "FT_DICA_PAROU_L2", a: ["9"] },
        acao: { tipo: "abrir", alvo: "c1" },
      }],
    },
    // l1 e acao em c0, mas l2 pega semanas=9 de c1 -> recusada.
    {
      v: 1, tela: "home", origem: "llm",
      pecas: [{
        t: "dica", id: "d2",
        l1: { m: "FT_DICA_PAROU_L1", a: ["c0", "2", "4"] },
        l2: { m: "FT_DICA_PAROU_L2", a: ["9"] },
        acao: { tipo: "abrir", alvo: "c0" },
      }],
    },
    // l1 e l2 coerentes em c0 (semanas=5), mas acao abre c1 -> recusada.
    {
      v: 1, tela: "home", origem: "llm",
      pecas: [{
        t: "dica", id: "d3",
        l1: { m: "FT_DICA_PAROU_L1", a: ["c0", "2", "4"] },
        l2: { m: "FT_DICA_PAROU_L2", a: ["5"] },
        acao: { tipo: "abrir", alvo: "c1" },
      }],
    },
  ]) {
    const { post, kv } = cenario(t, {}, { respostaLlm: () => cenaRuim });
    const r = await post("/v1/farol/compor", reqDica);
    assert.deepEqual(r.body, { origem: "regras" }, JSON.stringify(cenaRuim));
    assert.equal(kv.mapa.size, 0);

    // Mesmo se uma entrada antiga estiver no KV, o hit revalida e recusa.
    const chave = await chaveCacheCompor(reqDica);
    const hitEnv = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [{
          t: "dica", id: "d_ok",
          l1: { m: "FT_DICA_PAROU_L1", a: ["c0", "2", "4"] },
          l2: { m: "FT_DICA_PAROU_L2", a: ["5"] },
          acao: { tipo: "abrir", alvo: "c0" },
        }],
      }),
    });
    await hitEnv.kv.put(chave, JSON.stringify({
      ...cenaRuim,
      feita: 1795000000,
      vale_s: 86400,
    }), { expirationTtl: 86400 });
    const rHit = await hitEnv.post("/v1/farol/compor", reqDica);
    assert.equal(rHit.body.origem, "llm");
    assert.equal(rHit.body.pecas[0].l2.a[0], "5");
    assert.equal(rHit.body.pecas[0].acao.alvo, "c0");
    assert.equal(hitEnv.chamadasLlm.length, 1);
  }

  // Controle: tudo vinculado a c0 (l1=T2E4, l2=5 semanas, acao=c0) passa no miss e no hit com 1 chamada.
  {
    const { post, chamadasLlm } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [{
          t: "dica", id: "d_ok",
          l1: { m: "FT_DICA_PAROU_L1", a: ["c0", "2", "4"] },
          l2: { m: "FT_DICA_PAROU_L2", a: ["5"] },
          acao: { tipo: "abrir", alvo: "c0" },
        }],
      }),
    });
    const miss = await post("/v1/farol/compor", reqDica, { token: "tok-gosto-a", perfil: 1 });
    assert.equal(miss.body.origem, "llm");
    assert.equal(miss.body.pecas[0].l2.a[0], "5");
    assert.equal(miss.body.pecas[0].acao.alvo, "c0");
    const hit = await post("/v1/farol/compor", reqDica, { token: "tok-gosto-b", perfil: 1 });
    assert.deepEqual(hit.body, miss.body);
    assert.equal(chamadasLlm.length, 1);
  }

  // 2. Segundo caso de escopo: unico FMO_TERMINOU("tt11280740") em c6, fora da fileira c0..c5.
  for (const ancorasGosto of [undefined, [{ id: "tt11280740", titulo: "Lua" }]]) {
    const reqFora = pedidoHomeBase({
      gosto: {
        generos: [{ id: 53, nome: "Suspense", peso: 1.5 }],
        ...(ancorasGosto ? { ancoras: ancorasGosto } : {}),
        nivel: 2,
      },
      candidatos: pedidoHomeBase().candidatos.map((c, idx) =>
        idx === 6 ? { ...c, motivos: [{ tipo: "FMO_TERMINOU", a: "tt11280740", b: 0 }] } : c
      ),
    });
    const cenaFora = {
      v: 1, tela: "home", origem: "llm",
      pecas: [{
        t: "fileira", id: "gosto1",
        titulo: { m: "FT_FIL_PORQUE_TERMINOU", a: ["tt11280740"] },
        forma: "cartaz",
        itens: ["c0", "c1", "c2", "c3", "c4", "c5"],
      }],
    };
    const { post, kv } = cenario(t, {}, { respostaLlm: () => cenaFora });
    const r = await post("/v1/farol/compor", reqFora);
    assert.deepEqual(r.body, { origem: "regras" });
    assert.equal(kv.mapa.size, 0);

    // Hit KV legado com a mesma cena tambem e recusado.
    const chave = await chaveCacheCompor(reqFora);
    const cHit = cenario(t, {}, {
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
    await cHit.kv.put(chave, JSON.stringify({ ...cenaFora, feita: 1795000000, vale_s: 86400 }), { expirationTtl: 86400 });
    const r2 = await cHit.post("/v1/farol/compor", reqFora);
    assert.equal(r2.body.pecas[0].titulo.m, "FT_FIL_HORA");
    assert.equal(cHit.chamadasLlm.length, 1);
  }
});

test("R4-P1.2: FT_INFO_SOBE exige prova positiva de subida e FT_CMP_VEREDITO_CABE exige limite pertinente ao item", async (t) => {
  // 1. Retomada: sobe:false, cai:false (ou ausencia de prova) nao pode aceitar FT_INFO_SOBE.
  for (const fato of [
    { alvo: "c0", temporada: 2, faltam: 6, min_ep: 45, sobe: false, cai: false },
    { alvo: "c0", temporada: 2, faltam: 6, min_ep: 45 },
    { alvo: "c0", temporada: 2, faltam: 6, min_ep: 45, temps: [{ numero: 1, notaX10: 70, comNota: 4 }, { numero: 2, notaX10: 80, comNota: 6 }] },
    { alvo: "c0", temporada: 2, faltam: 6, min_ep: 45, temps: [{ numero: 1, notaX10: 70, comNota: 5 }, { numero: 2, notaX10: 73, comNota: 5 }] },
  ]) {
    const req = pedidoHomeBase({ modo: "retomada", fatos: [fato] });
    const cenaSobe = {
      v: 1, tela: "detalhe", origem: "llm",
      pecas: [{
        t: "info", alvo: "c0",
        itens: [{ k: "continuar", l: { m: "FT_INFO_SOBE", a: ["2", "6", "45"] } }],
      }],
    };
    const { post, kv } = cenario(t, {}, { respostaLlm: () => cenaSobe });
    const r = await post("/v1/farol/compor", req);
    assert.deepEqual(r.body, { origem: "regras" }, JSON.stringify(fato));
    assert.equal(kv.mapa.size, 0);
  }

  // Controle com prova positiva de subida (sobe:true ou temps >=5 amostras e dif >= 0.4) passa no miss e no hit.
  for (const fatoOk of [
    { alvo: "c0", temporada: 2, faltam: 6, min_ep: 45, sobe: true, cai: false },
    { alvo: "c0", temporada: 2, faltam: 6, min_ep: 45, temps: [{ numero: 1, notaX10: 70, comNota: 5 }, { numero: 2, notaX10: 74, comNota: 5 }] },
  ]) {
    const reqOk = pedidoHomeBase({ modo: "retomada", fatos: [fatoOk] });
    const { post, chamadasLlm } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "detalhe", origem: "llm",
        pecas: [{
          t: "info", alvo: "c0",
          itens: [{ k: "continuar", l: { m: "FT_INFO_SOBE", a: ["2", "6", "45"] } }],
        }],
      }),
    });
    const miss = await post("/v1/farol/compor", reqOk, { token: "tok-gosto-a", perfil: 1 });
    assert.equal(miss.body.origem, "llm");
    const hit = await post("/v1/farol/compor", reqOk, { token: "tok-gosto-b", perfil: 1 });
    assert.deepEqual(hit.body, miss.body);
    assert.equal(chamadasLlm.length, 1);
  }

  // 2. Comparar: c0.minutos=127, tempo_max:90 para c0 e tempo_max:240 so para c1 -> FT_CMP_VEREDITO_CABE(c0) recusado;
  //    e sem prova de tempo_max tambem e recusado.
  for (const fatosCmp of [
    [{ alvo: "c0", tempo_max: 90 }, { alvo: "c1", tempo_max: 240 }],
    [{ alvo: "c1", tempo_max: 240 }],
    [],
  ]) {
    const reqCmp = pedidoHomeBase({ modo: "comparar", fatos: fatosCmp });
    const cenaCmp = {
      v: 1, tela: "comparar", origem: "llm",
      pecas: [{
        t: "comparar",
        itens: ["c0", "c1"],
        eixos: ["duracao", "nota"],
        veredito: { m: "FT_CMP_VEREDITO_CABE", a: ["c0"] },
        escolha: "c0",
      }],
    };
    const { post, kv } = cenario(t, {}, { respostaLlm: () => cenaCmp });
    const r = await post("/v1/farol/compor", reqCmp);
    assert.deepEqual(r.body, { origem: "regras" }, JSON.stringify(fatosCmp));
    assert.equal(kv.mapa.size, 0);
  }

  // Controle comparar: tempo_max pertinente a c0 (150 >= 127) passa no miss e no hit.
  {
    const reqCmpOk = pedidoHomeBase({ modo: "comparar", fatos: [{ alvo: "c0", tempo_max: 150 }, { alvo: "c1", tempo_max: 90 }] });
    const { post, chamadasLlm } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "comparar", origem: "llm",
        pecas: [{
          t: "comparar",
          itens: ["c0", "c1"],
          eixos: ["duracao", "nota"],
          veredito: { m: "FT_CMP_VEREDITO_CABE", a: ["c0"] },
          escolha: "c0",
        }],
      }),
    });
    const miss = await post("/v1/farol/compor", reqCmpOk, { token: "tok-gosto-a", perfil: 1 });
    assert.equal(miss.body.origem, "llm");
    assert.equal(miss.body.pecas[0].veredito.m, "FT_CMP_VEREDITO_CABE");
    const hit = await post("/v1/farol/compor", reqCmpOk, { token: "tok-gosto-b", perfil: 1 });
    assert.deepEqual(hit.body, miss.body);
    assert.equal(chamadasLlm.length, 1);
  }
});

test("R4-P1.3: nono motivo de pessoa preserva m+a quando o pool de 8 xs lota e mantem fileira valida no miss e no hit", async (t) => {
  const cands9 = Array.from({ length: 9 }, (_, i) => ({
    id: `tt000010${i}`,
    titulo: `Obra ${i}`,
    ano: 2020,
    minutos: 100,
    motivos: [{ tipo: "FMO_PESSOA", a: 600000, b: 0 }],
  }));
  const req9 = pedidoHomeBase({
    gosto: { pessoas: [{ id: 600000, nome: "Diretora B", peso: 1.8 }], nivel: 2 },
    candidatos: cands9,
  });
  const itens9 = cands9.map((_, i) => `c${i}`);
  const motivo9 = Object.fromEntries(itens9.map((k) => [k, { m: "FT_MOT_PESSOA", a: ["p600000"] }]));

  const { post, kv, chamadasLlm } = cenario(t, {}, {
    respostaLlm: () => ({
      v: 1, tela: "home", origem: "llm",
      pecas: [{
        t: "fileira",
        id: "gosto1",
        titulo: { m: "FT_FIL_HORA" },
        forma: "cartaz",
        itens: itens9,
        motivo: motivo9,
      }],
    }),
  });

  const miss = await post("/v1/farol/compor", req9, { token: "tok-gosto-a", perfil: 1 });
  assert.equal(miss.status, 200);
  assert.equal(miss.body.origem, "llm");
  assert.equal(miss.body.pecas.length, 1);
  assert.equal(miss.body.pecas[0].itens.length, 9);
  assert.equal(Object.keys(miss.body.pecas[0].motivo).length, 9);
  for (let i = 0; i < 8; i++) {
    assert.equal(miss.body.pecas[0].motivo[`c${i}`].m, "FT_MOT_PESSOA");
    assert.deepEqual(miss.body.pecas[0].motivo[`c${i}`].a, ["p600000"]);
    assert.equal(miss.body.pecas[0].motivo[`c${i}`].x, "Com Diretora B, que você acompanha.");
  }
  assert.equal(miss.body.pecas[0].motivo.c8.m, "FT_MOT_PESSOA");
  assert.deepEqual(miss.body.pecas[0].motivo.c8.a, ["p600000"]);
  assert.equal(miss.body.pecas[0].motivo.c8.x, undefined);

  const valMiss = validarCena(JSON.stringify(miss.body), 9);
  assert.equal(valMiss.ok, true);
  assert.equal(valMiss.nPecas, 1);
  assert.equal(valMiss.nRecusadas, 0);
  assert.equal(valMiss.xs.length, 8);

  // KV sem `x` nem literal e com todos os 9 motivos preservados.
  assert.equal(kv.mapa.size, 1);
  const kvVal = [...kv.mapa.values()][0].valor;
  assert.ok(!kvVal.includes("\"x\""));
  assert.ok(!kvVal.includes("Diretora B"));
  assert.equal(Object.keys(JSON.parse(kvVal).pecas[0].motivo).length, 9);

  // Segunda conta acerta o hit com 1 unica chamada ao LLM.
  const hit = await post("/v1/farol/compor", req9, { token: "tok-gosto-b", perfil: 2 });
  assert.deepEqual(hit.body, miss.body);
  assert.equal(chamadasLlm.length, 1);
});

test("R4-P1.4: chave canonicaliza ordem de campos em fatos e referencias de ancora sem confundir identidades nem ordem dos candidatos", async (t) => {
  // 1. Ordem das propriedades em fatos[] nao muda a chave e compartilha KV com 1 chamada ao LLM.
  const reqF1 = pedidoHomeBase({
    modo: "dica",
    fatos: [{ alvo: "c0", temporada: 2, episodio: 4, semanas: 5 }],
  });
  const reqF2 = pedidoHomeBase({
    modo: "dica",
    fatos: [{ semanas: 5, episodio: 4, temporada: 2, alvo: "c0" }],
  });
  assert.equal(await chaveCacheCompor(reqF1), await chaveCacheCompor(reqF2));

  {
    const { post, chamadasLlm } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [{
          t: "dica", id: "d1",
          l1: { m: "FT_DICA_PAROU_L1", a: ["c0", "2", "4"] },
          l2: { m: "FT_DICA_PAROU_L2", a: ["5"] },
          acao: { tipo: "abrir", alvo: "c0" },
        }],
      }),
    });
    const r1 = await post("/v1/farol/compor", reqF1, { token: "tok-gosto-a", perfil: 1 });
    const r2 = await post("/v1/farol/compor", reqF2, { token: "tok-gosto-b", perfil: 1 });
    assert.equal(r1.body.origem, "llm");
    assert.deepEqual(r2.body, r1.body);
    assert.equal(chamadasLlm.length, 1);
  }

  // 2. FMO_TERMINOU por indice de ancora (tipo:0, a:0) vs IMDb explicito (tipo:"FMO_TERMINOU", a:"tt11280740")
  //    compartilha a mesma chave e faz 1 chamada ao LLM.
  const candsBase = pedidoHomeBase().candidatos;
  const reqAncIdx = pedidoHomeBase({
    gosto: { ancoras: [{ id: "tt11280740", titulo: "Ruptura" }], nivel: 2 },
    candidatos: candsBase.map((c, i) => (i === 0 ? { ...c, motivos: [{ tipo: 0, a: 0, b: 0 }] } : c)),
  });
  const reqAncTt = pedidoHomeBase({
    gosto: { ancoras: [{ id: "tt11280740", titulo: "Ruptura" }], nivel: 2 },
    candidatos: candsBase.map((c, i) => (i === 0 ? { ...c, motivos: [{ tipo: "FMO_TERMINOU", a: "tt11280740", b: 0 }] } : c)),
  });
  assert.equal(await chaveCacheCompor(reqAncIdx), await chaveCacheCompor(reqAncTt));

  {
    const { post, chamadasLlm } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [{
          t: "fileira", id: "gosto1",
          titulo: { m: "FT_FIL_PORQUE_TERMINOU", a: ["tt11280740"] },
          forma: "cartaz",
          itens: ["c0", "c1", "c2", "c3", "c4", "c5"],
        }],
      }),
    });
    const r1 = await post("/v1/farol/compor", reqAncIdx, { token: "tok-gosto-a", perfil: 1 });
    const r2 = await post("/v1/farol/compor", reqAncTt, { token: "tok-gosto-b", perfil: 1 });
    assert.equal(r1.body.origem, "llm");
    assert.deepEqual(r2.body, r1.body);
    assert.equal(chamadasLlm.length, 1);
  }

  // 3. Nao confunde identidades diferentes nem reordena candidatos.
  const reqAncOutro = pedidoHomeBase({
    gosto: { ancoras: [{ id: "tt9999999", titulo: "Outra" }], nivel: 2 },
    candidatos: candsBase.map((c, i) => (i === 0 ? { ...c, motivos: [{ tipo: 0, a: 0, b: 0 }] } : c)),
  });
  const reqCandTrocado = pedidoHomeBase({
    gosto: { ancoras: [{ id: "tt11280740", titulo: "Ruptura" }], nivel: 2 },
    candidatos: [candsBase[1], candsBase[0], ...candsBase.slice(2)],
  });
  assert.notEqual(await chaveCacheCompor(reqAncIdx), await chaveCacheCompor(reqAncOutro));
  assert.notEqual(await chaveCacheCompor(reqAncIdx), await chaveCacheCompor(reqCandTrocado));
});

// =============================================================================
// 11. Regressoes dos 2 P1 da revisao FAROL40-r5
// =============================================================================

test("R5-P1.1: resolve identidade do fato uma vez, recusa campos conflitantes e recusa IMDb ambiguo preservando alias inequivoco", async (t) => {
  const baseCands = pedidoHomeBase().candidatos;

  // 1. Repro 1: c0 tem tt0114369, c1 tem tt0133093, mas fato entra com {alvo:"c1", id:"tt0114369", semanas:9}.
  //    Fato conflitante nao pode agregar evidencia em c0 nem autorizar l2=9.
  const reqConflito = pedidoHomeBase({
    modo: "dica",
    candidatos: baseCands.map((c, idx) =>
      idx === 0 ? { ...c, tipo: "series", titulo: "Série A" }
        : idx === 1 ? { ...c, tipo: "movie", titulo: "Obra B" }
        : c
    ),
    fatos: [
      { alvo: "c0", temporada: 2, episodio: 4, semanas: 5 },
      { alvo: "c1", id: "tt0114369", semanas: 9 },
    ],
  });
  for (const l1Ref of ["c0", "tt0114369"]) {
    const cenaCruzada = {
      v: 1, tela: "home", origem: "llm",
      pecas: [{
        t: "dica", id: "d_cruz",
        l1: { m: "FT_DICA_PAROU_L1", a: [l1Ref, "2", "4"] },
        l2: { m: "FT_DICA_PAROU_L2", a: ["9"] },
        acao: { tipo: "abrir", alvo: "c0" },
      }],
    };
    const { post, kv } = cenario(t, {}, { respostaLlm: () => cenaCruzada });
    const r = await post("/v1/farol/compor", reqConflito);
    assert.deepEqual(r.body, { origem: "regras" });
    assert.equal(kv.mapa.size, 0);

    // Hit KV semeado com a cena ruim tambem e recusado e recomposto com a dica valida de 5 semanas.
    const chave = await chaveCacheCompor(reqConflito);
    const hitEnv = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [{
          t: "dica", id: "d_ok",
          l1: { m: "FT_DICA_PAROU_L1", a: ["tt0114369", "2", "4"] },
          l2: { m: "FT_DICA_PAROU_L2", a: ["5"] },
          acao: { tipo: "abrir", alvo: "c0" },
        }],
      }),
    });
    await hitEnv.kv.put(chave, JSON.stringify({ ...cenaCruzada, feita: 1795000000, vale_s: 86400 }), { expirationTtl: 86400 });
    const rHit = await hitEnv.post("/v1/farol/compor", reqConflito);
    assert.equal(rHit.body.origem, "llm");
    assert.equal(rHit.body.pecas[0].l2.a[0], "5");
    assert.equal(hitEnv.chamadasLlm.length, 1);
  }

  // 2. Repro 2: c0 (series) e c1 (movie) compartilham o mesmo IMDb tt0114369.
  //    A forma IMDb fica ambigua: nao pode ligar l1/l2 a acao(c1), nem agregar fato por IMDb, nem autorizar acao(tt0114369).
  const reqAmb = pedidoHomeBase({
    modo: "dica",
    candidatos: baseCands.map((c, idx) =>
      idx === 0 ? { ...c, id: "tt0114369", tipo: "series", titulo: "Série A" }
        : idx === 1 ? { ...c, id: "tt0114369", tipo: "movie", titulo: "Obra B" }
        : c
    ),
    fatos: [
      { alvo: "c0", temporada: 2, episodio: 4, semanas: 5 },
      { alvo: "c1", semanas: 9 },
    ],
  });
  for (const cenaAmb of [
    {
      v: 1, tela: "home", origem: "llm",
      pecas: [{
        t: "dica", id: "d_amb1",
        l1: { m: "FT_DICA_PAROU_L1", a: ["tt0114369", "2", "4"] },
        l2: { m: "FT_DICA_PAROU_L2", a: ["9"] },
        acao: { tipo: "abrir", alvo: "c1" },
      }],
    },
    {
      v: 1, tela: "home", origem: "llm",
      pecas: [{
        t: "dica", id: "d_amb2",
        l1: { m: "FT_DICA_PAROU_L1", a: ["tt0114369", "2", "4"] },
        l2: { m: "FT_DICA_PAROU_L2", a: ["5"] },
        acao: { tipo: "abrir", alvo: "c0" },
      }],
    },
    {
      v: 1, tela: "home", origem: "llm",
      pecas: [{
        t: "dica", id: "d_amb3",
        l1: { m: "FT_DICA_PAROU_L1", a: ["tt0114369", "2", "4"] },
        l2: { m: "FT_DICA_PAROU_L2", a: ["5"] },
        acao: { tipo: "abrir", alvo: "tt0114369" },
      }],
    },
    {
      v: 1, tela: "home", origem: "llm",
      pecas: [{
        t: "dica", id: "d_amb4",
        l1: { m: "FT_DICA_SALVO_L1", a: ["5M"] },
        acao: { tipo: "abrir", alvo: "tt0114369" },
      }],
    },
  ]) {
    const { post, kv } = cenario(t, {}, { respostaLlm: () => cenaAmb });
    const r = await post("/v1/farol/compor", reqAmb);
    assert.deepEqual(r.body, { origem: "regras" }, JSON.stringify(cenaAmb));
    assert.equal(kv.mapa.size, 0);

    const chave = await chaveCacheCompor(reqAmb);
    const hitEnv = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [{
          t: "dica", id: "d_c0_ok",
          l1: { m: "FT_DICA_PAROU_L1", a: ["c0", "2", "4"] },
          l2: { m: "FT_DICA_PAROU_L2", a: ["5"] },
          acao: { tipo: "abrir", alvo: "c0" },
        }],
      }),
    });
    await hitEnv.kv.put(chave, JSON.stringify({ ...cenaAmb, feita: 1795000000, vale_s: 86400 }), { expirationTtl: 86400 });
    const rHit = await hitEnv.post("/v1/farol/compor", reqAmb);
    assert.equal(rHit.body.origem, "llm");
    assert.equal(rHit.body.pecas[0].acao.alvo, "c0");
    assert.equal(rHit.body.pecas[0].l2.a[0], "5");
    assert.equal(hitEnv.chamadasLlm.length, 1);
  }

  // Fato que usa apenas o IMDb ambiguo nao agrega evidencia em c0 nem em c1.
  {
    const reqFatoAmb = pedidoHomeBase({
      modo: "dica",
      candidatos: reqAmb.candidatos,
      fatos: [{ alvo: "tt0114369", temporada: 2, episodio: 4, semanas: 5, salvo: "5M" }],
    });
    const { post, kv } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [{
          t: "dica", id: "d_sem_ev",
          l1: { m: "FT_DICA_PAROU_L1", a: ["c0", "2", "4"] },
          l2: { m: "FT_DICA_PAROU_L2", a: ["5"] },
          acao: { tipo: "abrir", alvo: "c0" },
        }],
      }),
    });
    const r = await post("/v1/farol/compor", reqFatoAmb);
    assert.deepEqual(r.body, { origem: "regras" });
    assert.equal(kv.mapa.size, 0);
  }

  // 3. Controle positivo: IMDb inequivoco na linha (tt0114369) e c0 na acao passam no miss e no hit com 5 semanas.
  {
    const reqCtrl = pedidoHomeBase({
      modo: "dica",
      candidatos: baseCands.map((c, idx) =>
        idx === 0 ? { ...c, id: "tt0114369", tipo: "series", titulo: "Série A" }
          : idx === 1 ? { ...c, id: "tt0133093", tipo: "movie", titulo: "Obra B" }
          : c
      ),
      fatos: [
        { alvo: "c0", temporada: 2, episodio: 4, semanas: 5 },
        { alvo: "c1", semanas: 9 },
      ],
    });
    const { post, chamadasLlm } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [{
          t: "dica", id: "d_ctrl",
          l1: { m: "FT_DICA_PAROU_L1", a: ["tt0114369", "2", "4"] },
          l2: { m: "FT_DICA_PAROU_L2", a: ["5"] },
          acao: { tipo: "abrir", alvo: "c0" },
        }],
      }),
    });
    const miss = await post("/v1/farol/compor", reqCtrl, { token: "tok-gosto-a", perfil: 1 });
    assert.equal(miss.body.origem, "llm");
    assert.equal(miss.body.pecas[0].l1.a[0], "tt0114369");
    assert.equal(miss.body.pecas[0].l2.a[0], "5");
    assert.equal(miss.body.pecas[0].acao.alvo, "c0");
    const hit = await post("/v1/farol/compor", reqCtrl, { token: "tok-gosto-b", perfil: 2 });
    assert.deepEqual(hit.body, miss.body);
    assert.equal(chamadasLlm.length, 1);
  }
});

test("R5-P1.2: hash de cache concorda com precedencia da ordem de fatos em comparar e nome publico, preservando canonicalizacao de propriedades", async (t) => {
  // 1. Comparar sem c0.minutos: [80, 160, tempo_max:100] vs invertido geram chaves distintas e resultados coerentes em miss/hit.
  const candsSemMin = pedidoHomeBase().candidatos.map((c, idx) =>
    idx === 0 ? { id: c.id, tipo: "series", titulo: "Série A", generos: c.generos } : c
  );
  const reqCmp1 = pedidoHomeBase({
    modo: "comparar",
    candidatos: candsSemMin,
    fatos: [{ alvo: "c0", minutos: 80 }, { alvo: "c0", minutos: 160 }, { tempo_max: 100 }],
  });
  const reqCmp2 = pedidoHomeBase({
    modo: "comparar",
    candidatos: candsSemMin,
    fatos: [{ tempo_max: 100 }, { alvo: "c0", minutos: 160 }, { alvo: "c0", minutos: 80 }],
  });
  const reqCmp1Props = pedidoHomeBase({
    modo: "comparar",
    candidatos: candsSemMin,
    fatos: [{ minutos: 80, alvo: "c0" }, { minutos: 160, alvo: "c0" }, { tempo_max: 100 }],
  });
  assert.notEqual(await chaveCacheCompor(reqCmp1), await chaveCacheCompor(reqCmp2));
  assert.equal(await chaveCacheCompor(reqCmp1), await chaveCacheCompor(reqCmp1Props));

  {
    const { post, chamadasLlm } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "comparar", origem: "llm",
        pecas: [{
          t: "comparar",
          itens: ["c0", "c1"],
          eixos: ["duracao", "nota"],
          veredito: { m: "FT_CMP_VEREDITO_CABE", a: ["c0"] },
          escolha: "c0",
        }],
      }),
    });
    const miss1 = await post("/v1/farol/compor", reqCmp1, { token: "tok-gosto-a", perfil: 1 });
    assert.equal(miss1.body.origem, "llm");
    assert.equal(miss1.body.pecas[0].veredito.m, "FT_CMP_VEREDITO_CABE");
    const hit1 = await post("/v1/farol/compor", reqCmp1Props, { token: "tok-gosto-b", perfil: 1 });
    assert.deepEqual(hit1.body, miss1.body);
    assert.equal(chamadasLlm.length, 1);

    // Ordem invertida nao acerta o cache de reqCmp1; reavalia e devolve {origem:"regras"}.
    const r2 = await post("/v1/farol/compor", reqCmp2, { token: "tok-gosto-b", perfil: 2 });
    assert.deepEqual(r2.body, { origem: "regras" });
    assert.equal(chamadasLlm.length, 2);
  }

  // 2. Nome publico de pessoa em fatos[]: inverter ordem produz chaves distintas, e cada chave e consistente entre miss e hit.
  const candsPessoa = pedidoHomeBase().candidatos;
  const reqNomeA = pedidoHomeBase({
    gosto: { generos: [{ id: 53, nome: "Suspense", peso: 1.5 }], pessoas: [{ id: 137427, peso: 1.6 }], nivel: 2 },
    candidatos: candsPessoa,
    fatos: [{ pessoa: 137427, nome: "Nome Público A" }, { pessoa: 137427, nome: "Nome Público B" }],
  });
  const reqNomeB = pedidoHomeBase({
    gosto: { generos: [{ id: 53, nome: "Suspense", peso: 1.5 }], pessoas: [{ id: 137427, peso: 1.6 }], nivel: 2 },
    candidatos: candsPessoa,
    fatos: [{ pessoa: 137427, nome: "Nome Público B" }, { pessoa: 137427, nome: "Nome Público A" }],
  });
  const reqNomeAProps = pedidoHomeBase({
    gosto: { generos: [{ id: 53, nome: "Suspense", peso: 1.5 }], pessoas: [{ id: 137427, peso: 1.6 }], nivel: 2 },
    candidatos: candsPessoa,
    fatos: [{ nome: "Nome Público A", pessoa: 137427 }, { nome: "Nome Público B", pessoa: 137427 }],
  });
  assert.notEqual(await chaveCacheCompor(reqNomeA), await chaveCacheCompor(reqNomeB));
  assert.equal(await chaveCacheCompor(reqNomeA), await chaveCacheCompor(reqNomeAProps));

  {
    const { post, kv, chamadasLlm } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [{
          t: "fileira", id: "gosto1",
          titulo: { m: "FT_FIL_HORA" },
          forma: "cartaz",
          itens: ["c0", "c1", "c2", "c3", "c4", "c5"],
          motivo: { c4: { m: "FT_MOT_PESSOA", a: ["p137427"] } },
        }],
      }),
    });
    const missA = await post("/v1/farol/compor", reqNomeA, { token: "tok-gosto-a", perfil: 1 });
    assert.equal(missA.body.origem, "llm");
    assert.equal(missA.body.pecas[0].motivo.c4.x, "Com Nome Público A, que você acompanha.");
    const hitA = await post("/v1/farol/compor", reqNomeAProps, { token: "tok-gosto-b", perfil: 1 });
    assert.deepEqual(hitA.body, missA.body);
    assert.equal(chamadasLlm.length, 1);

    const missB = await post("/v1/farol/compor", reqNomeB, { token: "tok-gosto-a", perfil: 2 });
    assert.equal(missB.body.origem, "llm");
    assert.equal(missB.body.pecas[0].motivo.c4.x, "Com Nome Público B, que você acompanha.");
    const hitB = await post("/v1/farol/compor", reqNomeB, { token: "tok-gosto-b", perfil: 2 });
    assert.deepEqual(hitB.body, missB.body);
    assert.equal(chamadasLlm.length, 2);

    assert.equal(kv.mapa.size, 2);
    for (const { valor } of kv.mapa.values()) {
      assert.ok(!valor.includes("\"x\""));
      assert.ok(!valor.includes("Nome Público A"));
      assert.ok(!valor.includes("Nome Público B"));
    }
  }
});

// =============================================================================
// 12. Regressoes dos achados P1 e P2 da revisao FAROL40-r6
// =============================================================================

test("R6-P1: resolve tipos explicitos conjuntamente para candidato sem tipo entre aliases, recusa contradicao nas duas ordens e preserva parciais coerentes", async (t) => {
  const baseCands = pedidoHomeBase().candidatos;
  const candsSemTipo = baseCands.map((c, idx) =>
    idx === 0 ? { id: "tt2010001" } : c
  );

  // 1. Reproducao exata do P1 e ordem invertida: c0 sem tipo com fatos series (c0) e movie (tt2010001).
  const ordensContraditorias = [
    [
      { alvo: "c0", tipo: "series", temporada: 2, episodio: 4 },
      { id: "tt2010001", tipo: "movie", semanas: 9 },
    ],
    [
      { id: "tt2010001", tipo: "movie", semanas: 9 },
      { alvo: "c0", tipo: "series", temporada: 2, episodio: 4 },
    ],
    [
      { alvo: "c0", tipo: "series", temporada: 2, episodio: 4, semanas: 5 },
      { id: "tt2010001", tipo: "movie", semanas: 9 },
    ],
    [
      { id: "tt2010001", tipo: "movie", semanas: 9 },
      { alvo: "c0", tipo: "series", temporada: 2, episodio: 4, semanas: 5 },
    ],
  ];

  const cenaRuimBase = {
    v: 1, tela: "home", origem: "llm",
    pecas: [{
      t: "dica",
      l1: { m: "FT_DICA_PAROU_L1", a: ["c0", "2", "4"] },
      l2: { m: "FT_DICA_PAROU_L2", a: ["9"] },
      acao: { tipo: "abrir", alvo: "c0" },
    }],
  };

  for (const fatosContradit of ordensContraditorias) {
    const req = pedidoHomeBase({
      modo: "dica",
      candidatos: candsSemTipo,
      fatos: fatosContradit,
    });

    // Miss e repeticao caem para regras sem gravar no KV.
    for (const cenaTestada of [
      { ...cenaRuimBase, pecas: [{ ...cenaRuimBase.pecas[0], id: "d_r6_1" }] },
      {
        v: 1, tela: "home", origem: "llm",
        pecas: [{
          t: "dica", id: "d_r6_1_l1",
          l1: { m: "FT_DICA_PAROU_L1", a: ["tt2010001", "2", "4"] },
          acao: { tipo: "abrir", alvo: "c0" },
        }],
      },
    ]) {
      const { post, kv, chamadasLlm } = cenario(t, {}, { respostaLlm: () => cenaTestada });
      const r1 = await post("/v1/farol/compor", req, { token: "tok-gosto-a", perfil: 1 });
      const r2 = await post("/v1/farol/compor", req, { token: "tok-gosto-b", perfil: 1 });
      assert.deepEqual(r1.body, { origem: "regras" }, JSON.stringify(fatosContradit));
      assert.deepEqual(r2.body, { origem: "regras" }, JSON.stringify(fatosContradit));
      assert.equal(kv.mapa.size, 0);
      assert.equal(chamadasLlm.length, 2);
    }

    // KV semeado (legado e canonico) tambem recusa a cena contraditoria e recompoe com alternativa valida.
    const pecaCanon = { ...cenaRuimBase.pecas[0], id: dicaIdCanonico(cenaRuimBase.pecas[0]) };
    for (const idSemeado of ["d_legado_r6", pecaCanon.id]) {
      const chave = await chaveCacheCompor(req);
      const hitEnv = cenario(t, {}, {
        respostaLlm: () => ({
          v: 1, tela: "home", origem: "llm",
          pecas: [{
            t: "dica", id: "d_trakt",
            l1: { m: "FT_DICA_TRAKT" },
            acao: { tipo: "nada" },
          }],
        }),
      });
      await hitEnv.kv.put(chave, JSON.stringify({
        v: 1, tela: "home", feita: 1795000000, vale_s: 86400, origem: "llm",
        pecas: [{ ...cenaRuimBase.pecas[0], id: idSemeado }],
      }), { expirationTtl: 86400 });
      const rHit = await hitEnv.post("/v1/farol/compor", req, { token: "tok-gosto-b", perfil: 1 });
      assert.equal(rHit.body.origem, "llm");
      assert.equal(rHit.body.pecas[0].l1.m, "FT_DICA_TRAKT");
      assert.equal(hitEnv.chamadasLlm.length, 1);
    }
  }

  // 2. Contradicao entre alias numerico e ref tt no mesmo candidato sem tipo (nas duas ordens).
  const candsNumRef = baseCands.map((c, idx) =>
    idx === 0 ? { id: 4825, ref: "tt2010001" } : c
  );
  for (const fatosNumRef of [
    [{ id: 4825, tipo: "series", temporada: 2, episodio: 4 }, { ref: "tt2010001", tipo: "movie", semanas: 9 }],
    [{ ref: "tt2010001", tipo: "movie", semanas: 9 }, { id: 4825, tipo: "series", temporada: 2, episodio: 4 }],
  ]) {
    const reqNumRef = pedidoHomeBase({ modo: "dica", candidatos: candsNumRef, fatos: fatosNumRef });
    const { post, kv } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [{
          t: "dica", id: "d_num_ref",
          l1: { m: "FT_DICA_PAROU_L1", a: ["c0", "2", "4"] },
          l2: { m: "FT_DICA_PAROU_L2", a: ["9"] },
          acao: { tipo: "abrir", alvo: "c0" },
        }],
      }),
    });
    const r = await post("/v1/farol/compor", reqNumRef);
    assert.deepEqual(r.body, { origem: "regras" });
    assert.equal(kv.mapa.size, 0);
  }

  // 3. Controles positivos coerentes com candidato sem tipo: fato unico e fatos parciais combinados entre aliases.
  for (const fatosCoerentes of [
    [{ alvo: "c0", tipo: "series", temporada: 2, episodio: 4, semanas: 5 }],
    [{ alvo: "c0", tipo: "series", temporada: 2, episodio: 4 }, { id: "tt2010001", tipo: "series", semanas: 5 }],
    [{ alvo: "c0", tipo: "series", temporada: 2, episodio: 4 }, { id: "tt2010001", semanas: 5 }],
    [{ alvo: "c0", temporada: 2, episodio: 4 }, { id: "tt2010001", tipo: "series", semanas: 5 }],
  ]) {
    const reqOk = pedidoHomeBase({
      modo: "dica",
      candidatos: candsSemTipo,
      fatos: fatosCoerentes,
    });
    const { post, chamadasLlm } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [{
          t: "dica", id: "d_ok",
          l1: { m: "FT_DICA_PAROU_L1", a: ["c0", "2", "4"] },
          l2: { m: "FT_DICA_PAROU_L2", a: ["5"] },
          acao: { tipo: "abrir", alvo: "c0" },
        }],
      }),
    });
    const miss = await post("/v1/farol/compor", reqOk, { token: "tok-gosto-a", perfil: 1 });
    assert.equal(miss.body.origem, "llm", JSON.stringify(fatosCoerentes));
    assert.equal(miss.body.pecas[0].l1.a[0], "c0");
    assert.equal(miss.body.pecas[0].l2.a[0], "5");
    assert.equal(miss.body.pecas[0].acao.alvo, "c0");
    const hit = await post("/v1/farol/compor", reqOk, { token: "tok-gosto-b", perfil: 1 });
    assert.deepEqual(hit.body, miss.body);
    assert.equal(chamadasLlm.length, 1);
  }

  // 4. Quando o candidato declara tipo:"series", fato invalido movie (semanas:9) e descartado mantendo fato series (semanas:5).
  {
    const candsTipado = baseCands.map((c, idx) => (idx === 0 ? { id: "tt2010001", tipo: "series" } : c));
    const reqTipado = pedidoHomeBase({
      modo: "dica",
      candidatos: candsTipado,
      fatos: [
        { alvo: "c0", tipo: "series", temporada: 2, episodio: 4, semanas: 5 },
        { id: "tt2010001", tipo: "movie", semanas: 9 },
      ],
    });
    const { post: post9 } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [{
          t: "dica", id: "d_9",
          l1: { m: "FT_DICA_PAROU_L1", a: ["c0", "2", "4"] },
          l2: { m: "FT_DICA_PAROU_L2", a: ["9"] },
          acao: { tipo: "abrir", alvo: "c0" },
        }],
      }),
    });
    assert.deepEqual((await post9("/v1/farol/compor", reqTipado)).body, { origem: "regras" });

    const { post: post5 } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [{
          t: "dica", id: "d_5",
          l1: { m: "FT_DICA_PAROU_L1", a: ["c0", "2", "4"] },
          l2: { m: "FT_DICA_PAROU_L2", a: ["5"] },
          acao: { tipo: "abrir", alvo: "c0" },
        }],
      }),
    });
    const r5 = await post5("/v1/farol/compor", reqTipado);
    assert.equal(r5.body.origem, "llm");
    assert.equal(r5.body.pecas[0].l2.a[0], "5");
  }
});

test("R6-P2: IMDb unico permanece resolvivel quando nome publico da ancora difere do titulo do candidato e preserva hit/chave", async (t) => {
  const baseCands = pedidoHomeBase().candidatos;
  const cands = baseCands.map((c, idx) =>
    idx === 0
      ? { id: "tt2010002", tipo: "movie", titulo: "Obra 1", ano: 2021, generos: [53], minutos: 100, motivos: [{ tipo: "FMO_TERMINOU", a: "tt2010002", b: 0 }] }
      : c
  );
  const reqIgual = pedidoHomeBase({
    gosto: { ancoras: [{ id: "tt2010002", titulo: "Obra 1" }], nivel: 2 },
    candidatos: cands,
  });
  const reqVariante = pedidoHomeBase({
    gosto: { ancoras: [{ id: "tt2010002", titulo: "Obra 1 (versão restaurada)" }], nivel: 2 },
    candidatos: cands,
  });

  // 1. Chave coincide quando id estabelece identidade e apenas o rotulo da ancora muda;
  //    quando id e ref estao ausentes, o rotulo continua distinguindo a chave.
  assert.equal(await chaveCacheCompor(reqIgual), await chaveCacheCompor(reqVariante));
  const reqSemIdA = pedidoHomeBase({ gosto: { ancoras: [{ titulo: "Obra 1" }], nivel: 2 }, candidatos: cands });
  const reqSemIdB = pedidoHomeBase({ gosto: { ancoras: [{ titulo: "Obra 1 (versão restaurada)" }], nivel: 2 }, candidatos: cands });
  assert.notEqual(await chaveCacheCompor(reqSemIdA), await chaveCacheCompor(reqSemIdB));

  const cenaAncora = {
    v: 1, tela: "home", origem: "llm",
    pecas: [{
      t: "fileira", id: "gosto1",
      titulo: { m: "FT_FIL_PORQUE_TERMINOU", a: ["tt2010002"] },
      forma: "cartaz",
      itens: ["c0", "c1", "c2", "c3", "c4", "c5"],
    }],
  };

  // 2. Cold miss + hit direto com nome variante da ancora passam com 1 chamada e KV sem literais.
  {
    const { post, kv, chamadasLlm } = cenario(t, {}, { respostaLlm: () => cenaAncora });
    const miss = await post("/v1/farol/compor", reqVariante, { token: "tok-gosto-a", perfil: 1 });
    assert.equal(miss.body.origem, "llm");
    assert.deepEqual(miss.body.pecas[0].titulo, { m: "FT_FIL_PORQUE_TERMINOU", a: ["tt2010002"] });
    const hit = await post("/v1/farol/compor", reqVariante, { token: "tok-gosto-b", perfil: 1 });
    assert.deepEqual(hit.body, miss.body);
    assert.equal(chamadasLlm.length, 1);
    assert.equal(kv.mapa.size, 1);
    const kvVal = [...kv.mapa.values()][0].valor;
    assert.ok(!kvVal.includes("\"x\""));
    assert.ok(!kvVal.includes("Obra 1"));
    assert.ok(!kvVal.includes("versão restaurada"));
  }

  // 3. Miss com titulo igual seguido de hit trocando somente o rotulo da ancora preserva hit (1 chamada LLM) e mesma cena.
  {
    const { post, chamadasLlm } = cenario(t, {}, { respostaLlm: () => cenaAncora });
    const missIgual = await post("/v1/farol/compor", reqIgual, { token: "tok-gosto-a", perfil: 1 });
    assert.equal(missIgual.body.origem, "llm");
    const hitVariante = await post("/v1/farol/compor", reqVariante, { token: "tok-gosto-b", perfil: 2 });
    assert.deepEqual(hitVariante.body, missIgual.body);
    assert.equal(chamadasLlm.length, 1);
  }

  // 4. Ambiguidade genuina (dois candidatos com o mesmo tt2010002) continua bloqueada no miss e no KV.
  {
    const candsAmb = cands.map((c, idx) =>
      idx === 1 ? { ...c, id: "tt2010002", tipo: "movie", titulo: "Obra 1 (versão restaurada)" } : c
    );
    const reqAmb = pedidoHomeBase({
      gosto: { ancoras: [{ id: "tt2010002", titulo: "Obra 1 (versão restaurada)" }], nivel: 2 },
      candidatos: candsAmb,
    });
    const { post, kv } = cenario(t, {}, { respostaLlm: () => cenaAncora });
    const r = await post("/v1/farol/compor", reqAmb);
    assert.deepEqual(r.body, { origem: "regras" });
    assert.equal(kv.mapa.size, 0);
  }
});

// =============================================================================
// 13. Regressoes dos achados P1 e P2 da revisao FAROL40-r7
// =============================================================================

test("R7-P1: integra candidatos[].eixos na resolucao conjunta de identidade e tipo antes da agregacao (dica e comparar)", async (t) => {
  const baseCands = pedidoHomeBase().candidatos;
  const candsSemTipo = (eixos0, extra0 = {}) => baseCands.map((c, idx) =>
    idx === 0 ? { id: "tt2010001", ...extra0, ...(eixos0 ? { eixos: eixos0 } : {}) } : c
  );

  // 1. Reproducao exata do P1 em dica: c0 sem tipo com eixos movie (semanas:9) + fato series (T2E4).
  //    Tambem cobre alias por id/ref, id numerico+ref e eixos com campos de identidade invalidos/ambiguos.
  const casosContraditDica = [
    {
      cands: candsSemTipo({ tipo: "movie", semanas: 9 }),
      fatos: [{ alvo: "c0", tipo: "series", temporada: 2, episodio: 4 }],
    },
    {
      cands: candsSemTipo({ tipo: "movie", semanas: 9 }),
      fatos: [{ id: "tt2010001", tipo: "series", temporada: 2, episodio: 4 }],
    },
    {
      cands: baseCands.map((c, idx) =>
        idx === 0 ? { id: 4825, ref: "tt2010001", eixos: { tipo: "movie", semanas: 9 } } : c
      ),
      fatos: [
        { id: 4825, temporada: 2, episodio: 4 },
        { ref: "tt2010001", tipo: "series" },
      ],
    },
    {
      cands: candsSemTipo({ alvo: "c1", semanas: 9 }),
      fatos: [{ alvo: "c0", tipo: "series", temporada: 2, episodio: 4 }],
    },
    {
      cands: candsSemTipo({ id: "tt9999999", semanas: 9 }),
      fatos: [{ alvo: "c0", tipo: "series", temporada: 2, episodio: 4 }],
    },
    {
      cands: baseCands.map((c, idx) =>
        idx === 0 ? { id: "tt2010001", eixos: { id: "tt2010001", semanas: 9 } }
          : idx === 1 ? { ...c, id: "tt2010001" }
          : c
      ),
      fatos: [{ alvo: "c0", tipo: "series", temporada: 2, episodio: 4 }],
    },
  ];

  const cenaDica9 = {
    v: 1, tela: "home", origem: "llm",
    pecas: [{
      t: "dica", id: "d_r7_p1",
      l1: { m: "FT_DICA_PAROU_L1", a: ["c0", "2", "4"] },
      l2: { m: "FT_DICA_PAROU_L2", a: ["9"] },
      acao: { tipo: "abrir", alvo: "c0" },
    }],
  };

  for (const caso of casosContraditDica) {
    const req = pedidoHomeBase({ modo: "dica", candidatos: caso.cands, fatos: caso.fatos });
    const { post, kv, chamadasLlm } = cenario(t, {}, { respostaLlm: () => cenaDica9 });
    const r1 = await post("/v1/farol/compor", req, { token: "tok-gosto-a", perfil: 1 });
    const r2 = await post("/v1/farol/compor", req, { token: "tok-gosto-b", perfil: 1 });
    assert.deepEqual(r1.body, { origem: "regras" }, JSON.stringify(caso.fatos));
    assert.deepEqual(r2.body, { origem: "regras" }, JSON.stringify(caso.fatos));
    assert.equal(kv.mapa.size, 0);
    assert.equal(chamadasLlm.length, 2);

    // Hit KV semeado (canonico e legado) tambem e recusado e recomposto.
    const chave = await chaveCacheCompor(req);
    const idCanon = farolGostoMod.dicaIdCanonico(cenaDica9.pecas[0]);
    for (const idSemeado of [idCanon, "d_legado_r7_dica"]) {
      const hitEnv = cenario(t, {}, {
        respostaLlm: () => ({
          v: 1, tela: "home", origem: "llm",
          pecas: [{ t: "dica", id: "d_ok", l1: { m: "FT_DICA_TRAKT" }, acao: { tipo: "nada" } }],
        }),
      });
      await hitEnv.kv.put(chave, JSON.stringify({
        v: 1, tela: "home", feita: 1795000000, vale_s: 86400, origem: "llm",
        pecas: [{ ...cenaDica9.pecas[0], id: idSemeado }],
      }), { expirationTtl: 86400 });
      const rHit = await hitEnv.post("/v1/farol/compor", req, { token: "tok-gosto-b", perfil: 2 });
      assert.equal(rHit.body.origem, "llm");
      assert.equal(rHit.body.pecas[0].l1.m, "FT_DICA_TRAKT");
      assert.equal(hitEnv.chamadasLlm.length, 1);
    }
  }

  // 2. Reproducao exata do P1 em comparar: c0 sem tipo com eixos movie (minutos:80) + fato series (minutos:160) + tempo_max:100.
  for (const fatosCmp of [
    [{ c: 0, tipo: "series", minutos: 160 }, { tempo_max: 100 }],
    [{ id: "tt2010001", tipo: "series", minutos: 160 }, { tempo_max: 100 }],
  ]) {
    const reqCmpRuim = pedidoHomeBase({
      modo: "comparar",
      candidatos: candsSemTipo({ tipo: "movie", minutos: 80 }),
      fatos: fatosCmp,
    });
    const cenaCmpRuim = {
      v: 1, tela: "comparar", origem: "llm",
      pecas: [{
        t: "comparar",
        itens: ["c0", "c1"],
        eixos: ["duracao", "nota"],
        veredito: { m: "FT_CMP_VEREDITO_CABE", a: ["c0"] },
        escolha: "c0",
      }],
    };
    const { post, kv, chamadasLlm } = cenario(t, {}, { respostaLlm: () => cenaCmpRuim });
    const r1 = await post("/v1/farol/compor", reqCmpRuim, { token: "tok-gosto-a", perfil: 1 });
    const r2 = await post("/v1/farol/compor", reqCmpRuim, { token: "tok-gosto-b", perfil: 1 });
    assert.deepEqual(r1.body, { origem: "regras" });
    assert.deepEqual(r2.body, { origem: "regras" });
    assert.equal(kv.mapa.size, 0);
    assert.equal(chamadasLlm.length, 2);

    const chave = await chaveCacheCompor(reqCmpRuim);
    const hitEnv = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "comparar", origem: "llm",
        pecas: [{ t: "comparar", itens: ["c0", "c1"], eixos: ["duracao", "nota"], escolha: "c1" }],
      }),
    });
    await hitEnv.kv.put(chave, JSON.stringify({
      ...cenaCmpRuim, feita: 1795000000, vale_s: 86400,
    }), { expirationTtl: 86400 });
    const rHit = await hitEnv.post("/v1/farol/compor", reqCmpRuim, { token: "tok-gosto-b", perfil: 2 });
    assert.equal(rHit.body.origem, "llm");
    assert.equal(rHit.body.pecas[0].veredito, undefined);
    assert.equal(rHit.body.pecas[0].escolha, "c1");
    assert.equal(hitEnv.chamadasLlm.length, 1);
  }

  // 3. Candidato com tipo explicito ("series"): eixos movie e descartado sem contaminar fatos series.
  {
    const reqTipadoDica = pedidoHomeBase({
      modo: "dica",
      candidatos: candsSemTipo({ tipo: "movie", semanas: 9 }, { tipo: "series" }),
      fatos: [{ alvo: "c0", tipo: "series", temporada: 2, episodio: 4, semanas: 5 }],
    });
    const { post: post9 } = cenario(t, {}, { respostaLlm: () => cenaDica9 });
    assert.deepEqual((await post9("/v1/farol/compor", reqTipadoDica)).body, { origem: "regras" });

    const { post: post5 } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [{
          t: "dica", id: "d_5",
          l1: { m: "FT_DICA_PAROU_L1", a: ["c0", "2", "4"] },
          l2: { m: "FT_DICA_PAROU_L2", a: ["5"] },
          acao: { tipo: "abrir", alvo: "c0" },
        }],
      }),
    });
    const r5 = await post5("/v1/farol/compor", reqTipadoDica);
    assert.equal(r5.body.origem, "llm");
    assert.equal(r5.body.pecas[0].l2.a[0], "5");

    const reqTipadoCmp = pedidoHomeBase({
      modo: "comparar",
      candidatos: candsSemTipo({ tipo: "movie", minutos: 80 }, { tipo: "series" }),
      fatos: [{ c: 0, tipo: "series", minutos: 160 }, { tempo_max: 100 }],
    });
    const { post: postCmp160 } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "comparar", origem: "llm",
        pecas: [{
          t: "comparar", itens: ["c0", "c1"], eixos: ["duracao"],
          veredito: { m: "FT_CMP_VEREDITO_CABE", a: ["c0"] }, escolha: "c0",
        }],
      }),
    });
    assert.deepEqual((await postCmp160("/v1/farol/compor", reqTipadoCmp)).body, { origem: "regras" });
  }

  // 4. Controles positivos concordantes misturando eixos e fatos (tipado/nao-tipado, aliases, campos legitimos de eixos e cache).
  for (const [eixosOk, fatosOk] of [
    [{ tipo: "serie", semanas: 5, onde: "Netflix", duracao: "45 min" }, [{ alvo: "c0", temporada: 2, episodio: 4 }]],
    [{ semanas: 5, onde: "Netflix", duracao: "45 min" }, [{ id: "tt2010001", tipo: "series", temporada: 2, episodio: 4 }]],
    [{ id: "tt2010001", tipo: "series", semanas: 5, onde: "Netflix" }, [{ alvo: "c0", tipo: "serie", temporada: 2, episodio: 4 }]],
  ]) {
    const reqOk = pedidoHomeBase({
      modo: "dica",
      candidatos: candsSemTipo(eixosOk),
      fatos: fatosOk,
    });
    const { post, chamadasLlm } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [{
          t: "dica", id: "d_ok",
          l1: { m: "FT_DICA_PAROU_L1", a: ["c0", "2", "4"] },
          l2: { m: "FT_DICA_PAROU_L2", a: ["5"] },
          acao: { tipo: "abrir", alvo: "c0" },
        }],
      }),
    });
    const miss = await post("/v1/farol/compor", reqOk, { token: "tok-gosto-a", perfil: 1 });
    assert.equal(miss.body.origem, "llm", JSON.stringify({ eixosOk, fatosOk }));
    assert.equal(miss.body.pecas[0].l2.a[0], "5");
    const hit = await post("/v1/farol/compor", reqOk, { token: "tok-gosto-b", perfil: 2 });
    assert.deepEqual(hit.body, miss.body);
    assert.equal(chamadasLlm.length, 1);
  }

  // Controle positivo comparar misto (eixos + fato concordante + precedencia de cache).
  {
    const reqCmpOkA = pedidoHomeBase({
      modo: "comparar",
      candidatos: candsSemTipo({ tipo: "serie", minutos: 80, onde: "Netflix", nota: "8.2" }),
      fatos: [{ c: 0, tipo: "series", minutos: 160 }, { tempo_max: 100 }],
    });
    const reqCmpOkB = pedidoHomeBase({
      modo: "comparar",
      candidatos: candsSemTipo({ nota: "8.2", onde: "Netflix", minutos: 80, tipo: "serie" }),
      fatos: [{ c: 0, tipo: "series", minutos: 160 }, { tempo_max: 100 }],
    });
    const reqCmpDiff = pedidoHomeBase({
      modo: "comparar",
      candidatos: candsSemTipo({ tipo: "movie", minutos: 80, onde: "Netflix", nota: "8.2" }),
      fatos: [{ c: 0, tipo: "series", minutos: 160 }, { tempo_max: 100 }],
    });
    assert.equal(await chaveCacheCompor(reqCmpOkA), await chaveCacheCompor(reqCmpOkB));
    assert.notEqual(await chaveCacheCompor(reqCmpOkA), await chaveCacheCompor(reqCmpDiff));

    const { post, chamadasLlm } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "comparar", origem: "llm",
        pecas: [{
          t: "comparar", itens: ["c0", "c1"], eixos: ["duracao", "onde", "nota"],
          veredito: { m: "FT_CMP_VEREDITO_CABE", a: ["c0"] }, escolha: "c0",
        }],
      }),
    });
    const miss = await post("/v1/farol/compor", reqCmpOkA, { token: "tok-gosto-a", perfil: 1 });
    assert.equal(miss.body.origem, "llm");
    assert.equal(miss.body.pecas[0].veredito.m, "FT_CMP_VEREDITO_CABE");
    const hit = await post("/v1/farol/compor", reqCmpOkB, { token: "tok-gosto-b", perfil: 1 });
    assert.deepEqual(hit.body, miss.body);
    assert.equal(chamadasLlm.length, 1);
  }
});

test("R7-P2: FT_DICA_CURTO respeita duracao conhecida do fato com precedencia de comparacao e controles de falta/conflito", async (t) => {
  const baseCands = pedidoHomeBase().candidatos;
  const candsSemMin = (extra0 = {}) => baseCands.map((c, idx) =>
    idx === 0 ? { id: "tt2010001", ...extra0 } : c
  );

  const cenaCurto = {
    v: 1, tela: "home", origem: "llm",
    pecas: [{
      t: "dica", id: "d_curto_r7",
      l1: { m: "FT_DICA_CURTO", a: ["c0"] },
      acao: { tipo: "abrir", alvo: "c0" },
    }],
  };

  // 1. Reproducao exata do P2 (160 > 100 no fato, c0 sem minutos; tambem limite global, eixos e c0.minutos=160).
  for (const [cands, fatos] of [
    [candsSemMin(), [{ c: 0, tipo: "series", minutos: 160, tempo_max: 100 }]],
    [candsSemMin(), [{ id: "tt2010001", tipo: "series", minutos: 160 }, { tempo_max: 100 }]],
    [candsSemMin({ eixos: { minutos: 160 } }), [{ c: 0, tipo: "series", tempo_max: 100 }]],
    [candsSemMin({ minutos: 160 }), [{ c: 0, tipo: "series", minutos: 80, tempo_max: 100 }]],
  ]) {
    const reqRuim = pedidoHomeBase({ modo: "dica", candidatos: cands, fatos });
    const { post, kv, chamadasLlm } = cenario(t, {}, { respostaLlm: () => cenaCurto });
    const r1 = await post("/v1/farol/compor", reqRuim, { token: "tok-gosto-a", perfil: 1 });
    const r2 = await post("/v1/farol/compor", reqRuim, { token: "tok-gosto-b", perfil: 1 });
    assert.deepEqual(r1.body, { origem: "regras" }, JSON.stringify(fatos));
    assert.deepEqual(r2.body, { origem: "regras" }, JSON.stringify(fatos));
    assert.equal(kv.mapa.size, 0);
    assert.equal(chamadasLlm.length, 2);

    // Hit KV semeado (canonico e legado) tambem e recusado e recomposto.
    const chave = await chaveCacheCompor(reqRuim);
    const idCanon = farolGostoMod.dicaIdCanonico(cenaCurto.pecas[0]);
    for (const idSemeado of [idCanon, "d_legado_r7_curto"]) {
      const hitEnv = cenario(t, {}, {
        respostaLlm: () => ({
          v: 1, tela: "home", origem: "llm",
          pecas: [{ t: "dica", id: "d_ok", l1: { m: "FT_DICA_TRAKT" }, acao: { tipo: "nada" } }],
        }),
      });
      await hitEnv.kv.put(chave, JSON.stringify({
        v: 1, tela: "home", feita: 1795000000, vale_s: 86400, origem: "llm",
        pecas: [{ ...cenaCurto.pecas[0], id: idSemeado }],
      }), { expirationTtl: 86400 });
      const rHit = await hitEnv.post("/v1/farol/compor", reqRuim, { token: "tok-gosto-b", perfil: 2 });
      assert.equal(rHit.body.origem, "llm");
      assert.equal(rHit.body.pecas[0].l1.m, "FT_DICA_TRAKT");
      assert.equal(hitEnv.chamadasLlm.length, 1);
    }
  }

  // 2. Controles positivos dentro do limite (incluindo precedencia de c0.minutos=80 e ordem de fatos).
  for (const [cands, fatos] of [
    [candsSemMin(), [{ c: 0, tipo: "series", minutos: 80, tempo_max: 100 }]],
    [candsSemMin(), [{ c: 0, tipo: "series", minutos: 100, tempo_max: 100 }]],
    [candsSemMin(), [{ id: "tt2010001", tipo: "series", minutos: 80 }, { tempo_max: 100 }]],
    [candsSemMin({ minutos: 80 }), [{ c: 0, tipo: "series", minutos: 160, tempo_max: 100 }]],
    [candsSemMin(), [{ c: 0, minutos: 80 }, { c: 0, minutos: 160, tempo_max: 100 }]],
  ]) {
    const reqOk = pedidoHomeBase({ modo: "dica", candidatos: cands, fatos });
    const { post, chamadasLlm } = cenario(t, {}, { respostaLlm: () => cenaCurto });
    const miss = await post("/v1/farol/compor", reqOk, { token: "tok-gosto-a", perfil: 1 });
    assert.equal(miss.body.origem, "llm", JSON.stringify(fatos));
    assert.equal(miss.body.pecas[0].l1.m, "FT_DICA_CURTO");
    const hit = await post("/v1/farol/compor", reqOk, { token: "tok-gosto-b", perfil: 2 });
    assert.deepEqual(hit.body, miss.body);
    assert.equal(chamadasLlm.length, 1);
  }

  // Ordem invertida dos fatos sem c0.minutos muda chave e recusa (160 primeiro > 100).
  const reqOrdOk = pedidoHomeBase({
    modo: "dica",
    candidatos: candsSemMin(),
    fatos: [{ c: 0, minutos: 80 }, { c: 0, minutos: 160, tempo_max: 100 }],
  });
  const reqOrdRuim = pedidoHomeBase({
    modo: "dica",
    candidatos: candsSemMin(),
    fatos: [{ c: 0, minutos: 160, tempo_max: 100 }, { c: 0, minutos: 80 }],
  });
  assert.notEqual(await chaveCacheCompor(reqOrdOk), await chaveCacheCompor(reqOrdRuim));
  {
    const { post, kv } = cenario(t, {}, { respostaLlm: () => cenaCurto });
    assert.deepEqual((await post("/v1/farol/compor", reqOrdRuim)).body, { origem: "regras" });
    assert.equal(kv.mapa.size, 0);
  }

  // 3. Controles proporcionais de evidencia ausente e conflitante.
  for (const [cands, fatos] of [
    // Limite presente sem duracao conhecida (ausente / 0)
    [candsSemMin(), [{ c: 0, tipo: "series", tempo_max: 100 }]],
    [candsSemMin(), [{ tempo_max: 100 }]],
    [candsSemMin({ minutos: 0 }), [{ c: 0, tempo_max: 100 }]],
    // Limites conflitantes onde 80 viola o menor limite (60)
    [candsSemMin(), [{ c: 0, minutos: 80, tempo_max: 100 }, { c: 0, tempo_max: 60 }]],
    // FMO_CABE_HOJE conflitante com minutos do candidato
    [candsSemMin({ minutos: 80, motivos: [{ tipo: "FMO_CABE_HOJE", a: 160 }] }), [{ c: 0, tempo_max: 100 }]],
    // Tipos conflitantes no mesmo candidato sem tipo
    [candsSemMin(), [{ c: 0, tipo: "series", minutos: 80, tempo_max: 100 }, { id: "tt2010001", tipo: "movie" }]],
  ]) {
    const reqCtrl = pedidoHomeBase({ modo: "dica", candidatos: cands, fatos });
    const { post, kv } = cenario(t, {}, { respostaLlm: () => cenaCurto });
    const r = await post("/v1/farol/compor", reqCtrl);
    assert.deepEqual(r.body, { origem: "regras" }, JSON.stringify(fatos));
    assert.equal(kv.mapa.size, 0);
  }
});

// =============================================================================
// 14. Regressoes dos achados P1, P2 e dicas aterradas da revisao FAROL40-r8
// =============================================================================

test("R8-P1: FT_DICA_CURTO exige limite positivo pertinente obedecido ou FMO_CABE_HOJE nativo coerente", async (t) => {
  const baseCands = pedidoHomeBase().candidatos;
  const filme240 = { id: "tt2010001", tipo: "filme", titulo: "Filme A", ano: 2004, generos: [18], minutos: 240 };
  const candsCom = (c0) => baseCands.map((c, idx) => (idx === 0 ? c0 : c));

  const cenaCurto = {
    v: 1, tela: "home", origem: "llm",
    pecas: [{
      t: "dica", id: "d_r8_curto",
      l1: { m: "FT_DICA_CURTO", a: ["c0"] },
      acao: { tipo: "abrir", alvo: "c0" },
    }],
  };

  // 1. Reproducao exata do P1 (home sem fatos e filme 240; dica com fatos:[], tempo_max:0, duracao ausente,
  //    duracao 80 sem limite/motivo, FT_MOT_CABE_HOJE nao-nativo sem limite, FMO_CABE_HOJE incoerente).
  const reqHomeSemFatos = pedidoHomeBase({ modo: "home", candidatos: candsCom(filme240) });
  delete reqHomeSemFatos.fatos;

  const casosSemElegibilidade = [
    reqHomeSemFatos,
    pedidoHomeBase({ modo: "dica", candidatos: candsCom(filme240), fatos: [] }),
    pedidoHomeBase({ modo: "dica", candidatos: candsCom(filme240), fatos: [{ c: 0, tempo_max: 0 }] }),
    pedidoHomeBase({ modo: "dica", candidatos: candsCom(filme240), fatos: [{ tempo_max: 0 }] }),
    pedidoHomeBase({ modo: "dica", candidatos: candsCom({ id: "tt2010001", tipo: "series", titulo: "Série A" }), fatos: [] }),
    pedidoHomeBase({ modo: "dica", candidatos: candsCom({ id: "tt2010001", tipo: "series", titulo: "Série A", minutos: 80 }), fatos: [] }),
    pedidoHomeBase({ modo: "dica", candidatos: candsCom({ id: "tt2010001", tipo: "series", titulo: "Série A", minutos: 80, motivos: [{ m: "FT_MOT_CABE_HOJE", a: ["80"] }] }), fatos: [] }),
    pedidoHomeBase({ modo: "dica", candidatos: candsCom({ id: "tt2010001", tipo: "series", titulo: "Série A", minutos: 80, motivos: [{ tipo: 6, a: 120, b: 0 }] }), fatos: [] }),
    pedidoHomeBase({ modo: "dica", candidatos: candsCom({ id: "tt2010001", tipo: "series", titulo: "Série A", minutos: 80, motivos: [{ tipo: 6, a: 0, b: 0 }] }), fatos: [] }),
    pedidoHomeBase({ modo: "dica", candidatos: candsCom({ id: "tt2010001", tipo: "series", titulo: "Série A", motivos: [{ tipo: 6, a: 80, b: 0 }] }), fatos: [{ c: 0, minutos: 120 }] }),
  ];

  for (const reqRuim of casosSemElegibilidade) {
    const { post, kv, chamadasLlm } = cenario(t, {}, { respostaLlm: () => cenaCurto });
    const r1 = await post("/v1/farol/compor", reqRuim, { token: "tok-gosto-a", perfil: 1 });
    const r2 = await post("/v1/farol/compor", reqRuim, { token: "tok-gosto-b", perfil: 1 });
    assert.deepEqual(r1.body, { origem: "regras" }, JSON.stringify(reqRuim));
    assert.deepEqual(r2.body, { origem: "regras" }, JSON.stringify(reqRuim));
    assert.equal(kv.mapa.size, 0);
    assert.equal(chamadasLlm.length, 2);

    const chave = await chaveCacheCompor(reqRuim);
    const idCanon = dicaIdCanonico(cenaCurto.pecas[0]);
    for (const idSemeado of [idCanon, "d_legado_r8_p1"]) {
      const hitEnv = cenario(t, {}, {
        respostaLlm: () => ({
          v: 1, tela: "home", origem: "llm",
          pecas: [{ t: "dica", id: "d_ok", l1: { m: "FT_DICA_TRAKT" }, acao: { tipo: "nada" } }],
        }),
      });
      await hitEnv.kv.put(chave, JSON.stringify({
        v: 1, tela: "home", feita: 1795000000, vale_s: 86400, origem: "llm",
        pecas: [{ ...cenaCurto.pecas[0], id: idSemeado }],
      }), { expirationTtl: 86400 });
      const rHit = await hitEnv.post("/v1/farol/compor", reqRuim, { token: "tok-gosto-b", perfil: 2 });
      assert.equal(rHit.body.origem, "llm");
      assert.equal(rHit.body.pecas[0].l1.m, "FT_DICA_TRAKT");
      assert.equal(hitEnv.chamadasLlm.length, 1);
    }
  }

  // 2. Controles positivos: limite positivo obedecido (abaixo/igual), precedencia item > global,
  //    e FMO_CABE_HOJE nativo coerente sem limite fornecido (e com limite igual).
  for (const [modo, c0, fatos] of [
    ["home", { ...filme240, minutos: 80 }, [{ c: 0, tempo_max: 100 }]],
    ["dica", { ...filme240, minutos: 100 }, [{ c: 0, tempo_max: 100 }]],
    ["dica", { ...filme240, minutos: 80 }, [{ tempo_max: 100 }]],
    ["dica", { ...filme240, minutos: 80 }, [{ c: 0, tempo_max: 100 }, { tempo_max: 60 }]],
    ["dica", { ...filme240, minutos: 80 }, [{ c: 0, tempo_max: 0 }, { tempo_max: 100 }]],
    ["dica", { id: "tt2010001", tipo: "series", titulo: "Série A", minutos: 80, motivos: [{ tipo: 6, a: 80, b: 0 }] }, []],
    ["home", { id: "tt2010001", tipo: "series", titulo: "Série A", minutos: 80, motivos: [{ tipo: "FMO_CABE_HOJE", a: 80, b: 0 }] }, [{ c: 0, tempo_max: 80 }]],
  ]) {
    const reqOk = pedidoHomeBase({ modo, candidatos: candsCom(c0), fatos });
    const { post, chamadasLlm } = cenario(t, {}, { respostaLlm: () => cenaCurto });
    const miss = await post("/v1/farol/compor", reqOk, { token: "tok-gosto-a", perfil: 1 });
    assert.equal(miss.body.origem, "llm", JSON.stringify({ modo, c0, fatos }));
    assert.equal(miss.body.pecas[0].l1.m, "FT_DICA_CURTO");
    const hit = await post("/v1/farol/compor", reqOk, { token: "tok-gosto-b", perfil: 2 });
    assert.deepEqual(hit.body, miss.body);
    assert.equal(chamadasLlm.length, 1);
  }

  // Comparar continua exigindo limite positivo explicito mesmo com FMO_CABE_HOJE nativo coerente,
  // e aceita controle de limite igual.
  {
    const candNativo80 = { id: "tt2010001", tipo: "series", titulo: "Série A", minutos: 80, motivos: [{ tipo: 6, a: 80, b: 0 }] };
    const cenaCmp = {
      v: 1, tela: "comparar", origem: "llm",
      pecas: [{ t: "comparar", itens: ["c0", "c1"], eixos: ["duracao", "nota"], veredito: { m: "FT_CMP_VEREDITO_CABE", a: ["c0"] }, escolha: "c0" }],
    };
    const { post: postSemLim } = cenario(t, {}, { respostaLlm: () => cenaCmp });
    assert.deepEqual((await postSemLim("/v1/farol/compor", pedidoHomeBase({ modo: "comparar", candidatos: candsCom(candNativo80), fatos: [] }))).body, { origem: "regras" });

    const { post: postLimIgual } = cenario(t, {}, { respostaLlm: () => cenaCmp });
    const rOk = await postLimIgual("/v1/farol/compor", pedidoHomeBase({ modo: "comparar", candidatos: candsCom(candNativo80), fatos: [{ c: 0, tempo_max: 80 }] }));
    assert.equal(rOk.body.origem, "llm");
    assert.equal(rOk.body.pecas[0].veredito.m, "FT_CMP_VEREDITO_CABE");
  }
});

test("R8-P2: FT_MOT_CABE_HOJE respeita limite pertinente mesmo com FMO_CABE_HOJE nativo", async (t) => {
  const baseCands = pedidoHomeBase().candidatos;
  const serie120 = { id: "tt2010001", tipo: "series", titulo: "Série A", minutos: 120, motivos: [{ tipo: 6, a: 120, b: 0 }] };
  const cands = baseCands.map((c, idx) => (idx === 0 ? serie120 : c));

  const cenaFileira = {
    v: 1, tela: "home", origem: "llm",
    pecas: [{
      t: "fileira", id: "gosto1", titulo: { m: "FT_FIL_HORA" }, forma: "cartaz",
      itens: ["c0", "c1", "c2", "c3", "c4", "c5"],
      motivo: { c0: { m: "FT_MOT_CABE_HOJE", a: ["120"] } },
    }],
  };

  // 1. Limite por item 60, global 60, item menor que global (60 vs 150) e item 0 caindo no global 60 recusam
  //    tanto na fileira quanto em achados e dica (miss, repeticao e KV semeado).
  for (const fatosTight of [
    [{ c: 0, tempo_max: 60 }],
    [{ tempo_max: 60 }],
    [{ c: 0, tempo_max: 60 }, { tempo_max: 150 }],
    [{ c: 0, tempo_max: 0 }, { tempo_max: 60 }],
  ]) {
    const reqTight = pedidoHomeBase({ modo: "home", candidatos: cands, fatos: fatosTight });
    const { post, kv, chamadasLlm } = cenario(t, {}, { respostaLlm: () => cenaFileira });
    const r1 = await post("/v1/farol/compor", reqTight, { token: "tok-gosto-a", perfil: 1 });
    const r2 = await post("/v1/farol/compor", reqTight, { token: "tok-gosto-b", perfil: 1 });
    assert.deepEqual(r1.body, { origem: "regras" }, JSON.stringify(fatosTight));
    assert.deepEqual(r2.body, { origem: "regras" }, JSON.stringify(fatosTight));
    assert.equal(kv.mapa.size, 0);
    assert.equal(chamadasLlm.length, 2);

    const chave = await chaveCacheCompor(reqTight);
    const hitEnv = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [{ t: "fileira", id: "gosto1", titulo: { m: "FT_FIL_HORA" }, forma: "cartaz", itens: ["c0", "c1", "c2", "c3", "c4", "c5"] }],
      }),
    });
    await hitEnv.kv.put(chave, JSON.stringify({ ...cenaFileira, feita: 1795000000, vale_s: 86400 }), { expirationTtl: 86400 });
    const rHit = await hitEnv.post("/v1/farol/compor", reqTight, { token: "tok-gosto-b", perfil: 2 });
    assert.equal(rHit.body.origem, "llm");
    assert.equal(rHit.body.pecas[0].motivo, undefined);
    assert.equal(hitEnv.chamadasLlm.length, 1);

    // Mesma recusa em FT_DICA_CURTO quando limite pertinente e menor que FMO_CABE_HOJE=120.
    const reqDicaTight = pedidoHomeBase({ modo: "dica", candidatos: cands, fatos: fatosTight });
    const { post: postDica, kv: kvDica } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [{ t: "dica", id: "d_curto", l1: { m: "FT_DICA_CURTO", a: ["c0"] }, acao: { tipo: "abrir", alvo: "c0" } }],
      }),
    });
    assert.deepEqual((await postDica("/v1/farol/compor", reqDicaTight)).body, { origem: "regras" });
    assert.equal(kvDica.mapa.size, 0);
  }

  // `porque` com alvo externo `tt:` e `cabe_hoje: true` tambem obedece limite pertinente.
  {
    const reqExtTight = pedidoHomeBase({
      modo: "retomada",
      fatos: [{ alvo: "tt0944947", cabe_hoje: true, minutos: 120 }, { tempo_max: 60 }],
    });
    const { post, kv } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "detalhe", origem: "llm",
        pecas: [{ t: "porque", alvo: "tt0944947", confianca: 2, linhas: [{ m: "FT_MOT_CABE_HOJE", a: ["120"] }] }],
      }),
    });
    assert.deepEqual((await post("/v1/farol/compor", reqExtTight)).body, { origem: "regras" });
    assert.equal(kv.mapa.size, 0);
  }

  // 2. Controles positivos: limite igual (120), item 120 vencendo global 60, limite de c1 ignorado e sem limite.
  for (const fatosOk of [
    [{ c: 0, tempo_max: 120 }],
    [{ c: 0, tempo_max: 120 }, { tempo_max: 60 }],
    [{ c: 1, tempo_max: 60 }],
    [],
  ]) {
    const reqOk = pedidoHomeBase({ modo: "home", candidatos: cands, fatos: fatosOk });
    const { post, chamadasLlm } = cenario(t, {}, { respostaLlm: () => cenaFileira });
    const miss = await post("/v1/farol/compor", reqOk, { token: "tok-gosto-a", perfil: 1 });
    assert.equal(miss.body.origem, "llm", JSON.stringify(fatosOk));
    assert.equal(miss.body.pecas[0].motivo.c0.m, "FT_MOT_CABE_HOJE");
    assert.deepEqual(miss.body.pecas[0].motivo.c0.a, ["120"]);
    const hit = await post("/v1/farol/compor", reqOk, { token: "tok-gosto-b", perfil: 2 });
    assert.deepEqual(hit.body, miss.body);
    assert.equal(chamadasLlm.length, 1);
  }
});

test("R8-Dicas: FT_DICA_TEMPORADA exige fato de nova temporada e filme declarado recusa retomada episodica", async (t) => {
  const baseCands = pedidoHomeBase().candidatos;
  const serie = { id: "tt2010001", tipo: "series", titulo: "Série A" };
  const filme = { id: "tt2010001", tipo: "filme", titulo: "Filme A", ano: 2004, generos: [18], minutos: 240 };
  const candsCom = (c0) => baseCands.map((c, idx) => (idx === 0 ? c0 : c));

  const cenaTemp = {
    v: 1, tela: "home", origem: "llm",
    pecas: [{
      t: "dica", id: "d_temp_r8",
      l1: { m: "FT_DICA_TEMPORADA", a: ["c0"] },
      acao: { tipo: "abrir", alvo: "c0" },
    }],
  };
  const cenaParou = {
    v: 1, tela: "home", origem: "llm",
    pecas: [{
      t: "dica", id: "d_parou_r8",
      l1: { m: "FT_DICA_PAROU_L1", a: ["c0", "2", "4"] },
      l2: { m: "FT_DICA_PAROU_L2", a: ["9"] },
      acao: { tipo: "abrir", alvo: "c0" },
    }],
  };

  // 1. Serie ordinaria sem fato de nova temporada (nem mesmo progresso/estreia de pessoa/flag false) nao afirma FT_DICA_TEMPORADA;
  //    filme declarado tambem nao aceita FT_DICA_TEMPORADA nem com fato de temporada.
  for (const [c0, fatos] of [
    [serie, []],
    [serie, [{ c: 0, temporada: 2, episodio: 4 }]],
    [serie, [{ c: 0, tempo_max: 100 }]],
    [serie, [{ c: 0, pessoa: 137427, estreia: true }]],
    [serie, [{ c: 0, nova_temporada: false }]],
    [filme, [{ c: 0, nova_temporada: true }]],
  ]) {
    const reqRuim = pedidoHomeBase({ modo: "dica", candidatos: candsCom(c0), fatos });
    const { post, kv, chamadasLlm } = cenario(t, {}, { respostaLlm: () => cenaTemp });
    const r1 = await post("/v1/farol/compor", reqRuim, { token: "tok-gosto-a", perfil: 1 });
    const r2 = await post("/v1/farol/compor", reqRuim, { token: "tok-gosto-b", perfil: 1 });
    assert.deepEqual(r1.body, { origem: "regras" }, JSON.stringify({ c0, fatos }));
    assert.deepEqual(r2.body, { origem: "regras" }, JSON.stringify({ c0, fatos }));
    assert.equal(kv.mapa.size, 0);
    assert.equal(chamadasLlm.length, 2);

    const chave = await chaveCacheCompor(reqRuim);
    const hitEnv = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [{ t: "dica", id: "d_ok", l1: { m: "FT_DICA_TRAKT" }, acao: { tipo: "nada" } }],
      }),
    });
    await hitEnv.kv.put(chave, JSON.stringify({
      v: 1, tela: "home", feita: 1795000000, vale_s: 86400, origem: "llm",
      pecas: [{ ...cenaTemp.pecas[0], id: dicaIdCanonico(cenaTemp.pecas[0]) }],
    }), { expirationTtl: 86400 });
    const rHit = await hitEnv.post("/v1/farol/compor", reqRuim, { token: "tok-gosto-b", perfil: 2 });
    assert.equal(rHit.body.origem, "llm");
    assert.equal(rHit.body.pecas[0].l1.m, "FT_DICA_TRAKT");
    assert.equal(hitEnv.chamadasLlm.length, 1);
  }

  // Controles positivos legitimos de FT_DICA_TEMPORADA.
  for (const fatosTempOk of [
    [{ c: 0, nova_temporada: true }],
    [{ c: 0, temporada_nova: 2 }],
    [{ c: 0, estreia_temporada: true }],
    [{ c: 0, tipo: "series", estreia: true }],
    [{ c: 0, temporada: 3, chegou: true }],
  ]) {
    const reqOk = pedidoHomeBase({ modo: "dica", candidatos: candsCom(serie), fatos: fatosTempOk });
    const { post, chamadasLlm } = cenario(t, {}, { respostaLlm: () => cenaTemp });
    const miss = await post("/v1/farol/compor", reqOk, { token: "tok-gosto-a", perfil: 1 });
    assert.equal(miss.body.origem, "llm", JSON.stringify(fatosTempOk));
    assert.equal(miss.body.pecas[0].l1.m, "FT_DICA_TEMPORADA");
    const hit = await post("/v1/farol/compor", reqOk, { token: "tok-gosto-b", perfil: 2 });
    assert.deepEqual(hit.body, miss.body);
    assert.equal(chamadasLlm.length, 1);
  }

  // 2. Filme declarado nao produz retomada episodica a partir de evidencia sem tipo (dica parou, falta e info retomada/sobe).
  for (const [c0, fatos, cena] of [
    [{ ...filme, eixos: { semanas: 9 } }, [{ c: 0, temporada: 2, episodio: 4 }], cenaParou],
    [filme, [{ c: 0, temporada: 2, episodio: 4 }], { v: 1, tela: "home", origem: "llm", pecas: [{ t: "dica", id: "d_p1", l1: { m: "FT_DICA_PAROU_L1", a: ["c0", "2", "4"] }, acao: { tipo: "abrir", alvo: "c0" } }] }],
    [{ id: "tt2010001", eixos: { semanas: 9 } }, [{ c: 0, tipo: "filme" }, { c: 0, temporada: 2, episodio: 4 }], cenaParou],
    [filme, [{ c: 0, faltam: 3, temporada: 2 }], { v: 1, tela: "home", origem: "llm", pecas: [{ t: "dica", id: "d_f1", l1: { m: "FT_DICA_FALTA_L1", a: ["3", "2", "c0"] }, acao: { tipo: "abrir", alvo: "c0" } }] }],
  ]) {
    const reqRuim = pedidoHomeBase({ modo: "dica", candidatos: candsCom(c0), fatos });
    const { post, kv, chamadasLlm } = cenario(t, {}, { respostaLlm: () => cena });
    const r1 = await post("/v1/farol/compor", reqRuim, { token: "tok-gosto-a", perfil: 1 });
    const r2 = await post("/v1/farol/compor", reqRuim, { token: "tok-gosto-b", perfil: 1 });
    assert.deepEqual(r1.body, { origem: "regras" });
    assert.deepEqual(r2.body, { origem: "regras" });
    assert.equal(kv.mapa.size, 0);
    assert.equal(chamadasLlm.length, 2);

    const chave = await chaveCacheCompor(reqRuim);
    const hitEnv = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [{ t: "dica", id: "d_ok", l1: { m: "FT_DICA_TRAKT" }, acao: { tipo: "nada" } }],
      }),
    });
    await hitEnv.kv.put(chave, JSON.stringify({
      v: 1, tela: "home", feita: 1795000000, vale_s: 86400, origem: "llm",
      pecas: [{ ...cena.pecas[0], id: dicaIdCanonico(cena.pecas[0]) }],
    }), { expirationTtl: 86400 });
    const rHit = await hitEnv.post("/v1/farol/compor", reqRuim, { token: "tok-gosto-b", perfil: 2 });
    assert.equal(rHit.body.origem, "llm");
    assert.equal(rHit.body.pecas[0].l1.m, "FT_DICA_TRAKT");
    assert.equal(hitEnv.chamadasLlm.length, 1);
  }

  // Filme declarado em modo retomada tambem recusa info episodica.
  {
    const reqInfoFilme = pedidoHomeBase({
      modo: "retomada",
      candidatos: candsCom(filme),
      fatos: [{ c: 0, temporada: 2, episodio: 4, retomada: "7w" }],
    });
    const { post, kv } = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "detalhe", origem: "llm",
        pecas: [{ t: "info", alvo: "c0", itens: [{ k: "retomada", l: { m: "FT_INFO_RETOMADA", a: ["2", "4", "7w"] } }] }],
      }),
    });
    assert.deepEqual((await post("/v1/farol/compor", reqInfoFilme)).body, { origem: "regras" });
    assert.equal(kv.mapa.size, 0);
  }

  // Controles positivos legitimos: serie (ou candidato sem tipo) retoma normalmente com eixos/fatos coerentes.
  for (const [c0, fatos] of [
    [{ ...serie, eixos: { semanas: 9 } }, [{ c: 0, temporada: 2, episodio: 4 }]],
    [{ id: "tt2010001", eixos: { semanas: 9 } }, [{ c: 0, temporada: 2, episodio: 4 }]],
  ]) {
    const reqOk = pedidoHomeBase({ modo: "dica", candidatos: candsCom(c0), fatos });
    const { post, chamadasLlm } = cenario(t, {}, { respostaLlm: () => cenaParou });
    const miss = await post("/v1/farol/compor", reqOk, { token: "tok-gosto-a", perfil: 1 });
    assert.equal(miss.body.origem, "llm");
    assert.equal(miss.body.pecas[0].l1.m, "FT_DICA_PAROU_L1");
    assert.equal(miss.body.pecas[0].l2.a[0], "9");
    const hit = await post("/v1/farol/compor", reqOk, { token: "tok-gosto-b", perfil: 2 });
    assert.deepEqual(hit.body, miss.body);
    assert.equal(chamadasLlm.length, 1);
  }
});

// =============================================================================
// 15. Regressoes dos achados P1 e P2 da revisao FAROL40-r9
// =============================================================================

test("R9-P1: FT_DICA_TEMPORADA veta contradicao entre fato positivo e negacao explicita na mesma obra", async (t) => {
  const baseCands = pedidoHomeBase().candidatos;
  const serie = { id: "tt2010001", tipo: "series", titulo: "Série A" };
  const candsCom = (c0) => baseCands.map((c, idx) => (idx === 0 ? c0 : c));
  const cenaTemp = {
    v: 1, tela: "home", origem: "llm",
    pecas: [{
      t: "dica", id: "d_r9_temp",
      l1: { m: "FT_DICA_TEMPORADA", a: ["c0"] },
      acao: { tipo: "abrir", alvo: "c0" },
    }],
  };

  // 1. Reproducao exata do P1 e variantes por aliases, eixos e ordem invertida.
  for (const [c0, fatos] of [
    [serie, [{ c: 0, tipo: "series", nova_temporada: true }, { id: "tt2010001", tipo: "series", nova_temporada: false }]],
    [serie, [{ id: "tt2010001", tipo: "series", nova_temporada: false }, { c: 0, tipo: "series", nova_temporada: true }]],
    [serie, [{ c: 0, temporada_nova: 3 }, { ref: "tt2010001", estreia_temporada: 0 }]],
    [serie, [{ ref: "tt2010001", temporada: 3, nova: true }, { alvo: "c0", chegou: false }]],
    [serie, [{ c: 0, tipo: "series", estreia: true }, { id: "tt2010001", estreia: false }]],
    [{ ...serie, eixos: { nova_temporada: false } }, [{ c: 0, tipo: "series", nova_temporada: true }]],
    [{ ...serie, eixos: { nova_temporada: true } }, [{ id: "tt2010001", nova_temporada: false }]],
    [serie, [{ c: 0, nova_temporada: true, chegou: 0 }]],
  ]) {
    const reqRuim = pedidoHomeBase({ modo: "home", candidatos: candsCom(c0), fatos });
    const { post, kv, chamadasLlm } = cenario(t, {}, { respostaLlm: () => cenaTemp });
    const r1 = await post("/v1/farol/compor", reqRuim, { token: "tok-gosto-a", perfil: 1 });
    const r2 = await post("/v1/farol/compor", reqRuim, { token: "tok-gosto-b", perfil: 1 });
    assert.deepEqual(r1.body, { origem: "regras" }, JSON.stringify({ c0, fatos }));
    assert.deepEqual(r2.body, { origem: "regras" }, JSON.stringify({ c0, fatos }));
    assert.equal(kv.mapa.size, 0);
    assert.equal(chamadasLlm.length, 2);

    const chave = await chaveCacheCompor(reqRuim);
    const idCanon = farolGostoMod.dicaIdCanonico(cenaTemp.pecas[0]);
    for (const idSemeado of [idCanon, "d_legado_r9_temp"]) {
      const hitEnv = cenario(t, {}, {
        respostaLlm: () => ({
          v: 1, tela: "home", origem: "llm",
          pecas: [{ t: "dica", id: "d_ok", l1: { m: "FT_DICA_TRAKT" }, acao: { tipo: "nada" } }],
        }),
      });
      await hitEnv.kv.put(chave, JSON.stringify({
        v: 1, tela: "home", feita: 1795000000, vale_s: 86400, origem: "llm",
        pecas: [{ ...cenaTemp.pecas[0], id: idSemeado }],
      }), { expirationTtl: 86400 });
      const rHit = await hitEnv.post("/v1/farol/compor", reqRuim, { token: "tok-gosto-b", perfil: 2 });
      assert.equal(rHit.body.origem, "llm");
      assert.equal(rHit.body.pecas[0].l1.m, "FT_DICA_TRAKT");
      assert.equal(hitEnv.chamadasLlm.length, 1);
    }
  }

  // 2. Controles positivos legitimos: negacao em outra obra, fato global ou estreia de pessoa nao veta c0;
  //    eixo cruzado com alias de outra obra tambem nao veta nem contamina.
  for (const [c0, fatos] of [
    [serie, [{ c: 0, tipo: "series", nova_temporada: true }, { c: 1, tipo: "series", nova_temporada: false }]],
    [serie, [{ c: 0, tipo: "series", nova_temporada: true }, { nova_temporada: false }]],
    [serie, [{ c: 0, tipo: "series", nova_temporada: true }, { c: 0, pessoa: 137427, estreia: false }]],
    [{ ...serie, eixos: { ref: "tt0133093", nova_temporada: false } }, [{ c: 0, tipo: "series", nova_temporada: true }]],
    [serie, [{ c: 0, tipo: "series", nova_temporada: true }, { c: 0, temporada: 2, episodio: 4 }]],
  ]) {
    const reqOk = pedidoHomeBase({ modo: "home", candidatos: candsCom(c0), fatos });
    const { post, chamadasLlm } = cenario(t, {}, { respostaLlm: () => cenaTemp });
    const miss = await post("/v1/farol/compor", reqOk, { token: "tok-gosto-a", perfil: 1 });
    assert.equal(miss.body.origem, "llm", JSON.stringify({ c0, fatos }));
    assert.equal(miss.body.pecas[0].l1.m, "FT_DICA_TEMPORADA");
    const hit = await post("/v1/farol/compor", reqOk, { token: "tok-gosto-b", perfil: 2 });
    assert.deepEqual(hit.body, miss.body);
    assert.equal(chamadasLlm.length, 1);
  }
});

test("R9-P2: elegibilidade canonica por obra recusa cabe_hoje:false e duracoes contraditorias em candidato e IMDb externo", async (t) => {
  const baseCands = pedidoHomeBase().candidatos;
  const serie80 = { id: "tt2010001", tipo: "series", titulo: "Série A", minutos: 80, motivos: [{ tipo: 6, a: 80, b: 0 }] };
  const serieSemMin = { id: "tt2010001", tipo: "series", titulo: "Série A" };
  const candsCom = (c0) => baseCands.map((c, idx) => (idx === 0 ? c0 : c));

  const cenaDica = {
    v: 1, tela: "home", origem: "llm",
    pecas: [{ t: "dica", id: "d_r9_curto", l1: { m: "FT_DICA_CURTO", a: ["c0"] }, acao: { tipo: "abrir", alvo: "c0" } }],
  };
  const cenaFileira = {
    v: 1, tela: "home", origem: "llm",
    pecas: [{
      t: "fileira", id: "gosto1", titulo: { m: "FT_FIL_HORA" }, forma: "cartaz",
      itens: ["c0", "c1", "c2", "c3", "c4", "c5"],
      motivo: { c0: { m: "FT_MOT_CABE_HOJE", a: ["80"] } },
    }],
  };
  const cenaAchados = {
    v: 1, tela: "home", origem: "llm",
    pecas: [{
      t: "achados", l1: { m: "FT_ACHA_NAO_VIU" }, itens: ["c0"],
      motivo: { c0: { m: "FT_MOT_CABE_HOJE", a: ["80"] } },
    }],
  };
  const cenaPorque = (alvo = "c0", min = "80") => ({
    v: 1, tela: "detalhe", origem: "llm",
    pecas: [{ t: "porque", alvo, confianca: 2, linhas: [{ m: "FT_MOT_CABE_HOJE", a: [String(min)] }] }],
  });
  const cenaCmp = {
    v: 1, tela: "comparar", origem: "llm",
    pecas: [{ t: "comparar", itens: ["c0", "c1"], eixos: ["duracao", "nota"], veredito: { m: "FT_CMP_VEREDITO_CABE", a: ["c0"] }, escolha: "c0" }],
  };
  const telaPorModo = { dica: "home", home: "home", retomada: "detalhe", comparar: "comparar" };

  // 1. Reproducao exata do P2 (candidato com cabe_hoje:false por indice, alias IMDb ou eixos, com e sem tempo_max:100)
  //    e duracao afirmativa contraditoria caem para regras em dica, fileira, achados, porque e comparar.
  for (const [c0, fatos] of [
    [serie80, [{ c: 0, cabe_hoje: false, minutos: 80 }]],
    [serie80, [{ c: 0, cabe_hoje: false, minutos: 80 }, { tempo_max: 100 }]],
    [serie80, [{ id: "tt2010001", cabe_hoje: false }, { tempo_max: 100 }]],
    [serie80, [{ ref: "tt2010001", cabe_hoje: 0 }, { tempo_max: 100 }]],
    [{ ...serie80, eixos: { cabe_hoje: false } }, [{ tempo_max: 100 }]],
    [{ ...serieSemMin, eixos: { cabe_hoje: true, minutos: 80 } }, [{ id: "tt2010001", minutos: 120 }, { tempo_max: 150 }]],
    [serieSemMin, [{ c: 0, cabe_hoje: true, minutos: 80 }, { id: "tt2010001", cabe_hoje: true, minutos: 120 }, { tempo_max: 150 }]],
    [serieSemMin, [{ id: "tt2010001", cabe_hoje: true, minutos: 120 }, { c: 0, cabe_hoje: true, minutos: 80 }, { tempo_max: 150 }]],
  ]) {
    for (const [modo, cena] of [
      ["dica", cenaDica],
      ["home", cenaFileira],
      ["home", cenaAchados],
      ["retomada", cenaPorque("c0", "80")],
      ["comparar", cenaCmp],
    ]) {
      const reqRuim = pedidoHomeBase({ modo, candidatos: candsCom(c0), fatos });
      const { post, kv, chamadasLlm } = cenario(t, {}, { respostaLlm: () => cena });
      const r1 = await post("/v1/farol/compor", reqRuim, { token: "tok-gosto-a", perfil: 1 });
      const r2 = await post("/v1/farol/compor", reqRuim, { token: "tok-gosto-b", perfil: 1 });
      assert.deepEqual(r1.body, { origem: "regras" }, `${modo} ${JSON.stringify(fatos)}`);
      assert.deepEqual(r2.body, { origem: "regras" }, `${modo} ${JSON.stringify(fatos)}`);
      assert.equal(kv.mapa.size, 0);
      assert.equal(chamadasLlm.length, 2);

      const chave = await chaveCacheCompor(reqRuim);
      const tela = telaPorModo[modo];
      const hitEnv = cenario(t, {}, {
        respostaLlm: () => ({
          v: 1, tela, origem: "llm",
          pecas: [
            modo === "comparar"
              ? { t: "comparar", itens: ["c0", "c1"], eixos: ["duracao"], escolha: "c0" }
              : modo === "retomada"
                ? { t: "porque", alvo: "c1", confianca: 1, linhas: [{ m: "FT_MOT_GENERO", a: ["g878"] }] }
                : { t: "dica", id: "d_ok", l1: { m: "FT_DICA_TRAKT" }, acao: { tipo: "nada" } },
          ],
        }),
      });
      const pecaSemeada = cena.pecas[0].t === "dica"
        ? { ...cena.pecas[0], id: farolGostoMod.dicaIdCanonico(cena.pecas[0]) }
        : cena.pecas[0];
      await hitEnv.kv.put(chave, JSON.stringify({
        v: 1, tela, feita: 1795000000, vale_s: 86400, origem: "llm",
        pecas: [pecaSemeada],
      }), { expirationTtl: 86400 });
      const rHit = await hitEnv.post("/v1/farol/compor", reqRuim, { token: "tok-gosto-b", perfil: 2 });
      assert.equal(rHit.body.origem, "llm");
      assert.equal(hitEnv.chamadasLlm.length, 1);
      if (modo === "comparar") assert.equal(rHit.body.pecas[0].veredito, undefined);
      else if (modo === "retomada") assert.equal(rHit.body.pecas[0].linhas[0].m, "FT_MOT_GENERO");
      else assert.equal(rHit.body.pecas[0].l1.m, "FT_DICA_TRAKT");
    }
  }

  // 2. IMDb externo (tt0944947): contradicao em qualquer ordem e cabe_hoje:false sao recusados (inclusive em KV semeado);
  //    controle positivo com limite igual (120) e limite frouxo (150) passam.
  const ext = "tt0944947";
  for (const fatosExtRuins of [
    [{ alvo: ext, cabe_hoje: true, minutos: 120 }, { alvo: ext, cabe_hoje: true, minutos: 80 }, { tempo_max: 150 }],
    [{ alvo: ext, cabe_hoje: true, minutos: 80 }, { alvo: ext, cabe_hoje: true, minutos: 120 }, { tempo_max: 150 }],
    [{ alvo: ext, cabe_hoje: true, minutos: 120 }, { id: ext, minutos: 80 }, { tempo_max: 150 }],
    [{ alvo: ext, minutos: 80 }, { id: ext, cabe_hoje: true, minutos: 120 }, { tempo_max: 150 }],
    [{ alvo: ext, cabe_hoje: true, minutos: 120 }, { id: ext, cabe_hoje: false }, { tempo_max: 150 }],
    [{ id: ext, cabe_hoje: false }, { alvo: ext, cabe_hoje: true, minutos: 120 }, { tempo_max: 150 }],
  ]) {
    const reqExtRuim = pedidoHomeBase({ modo: "retomada", fatos: fatosExtRuins });
    const cenaExt = cenaPorque(ext, "120");
    const { post, kv, chamadasLlm } = cenario(t, {}, { respostaLlm: () => cenaExt });
    assert.deepEqual((await post("/v1/farol/compor", reqExtRuim, { token: "tok-gosto-a", perfil: 1 })).body,
      { origem: "regras" }, JSON.stringify(fatosExtRuins));
    assert.deepEqual((await post("/v1/farol/compor", reqExtRuim, { token: "tok-gosto-b", perfil: 1 })).body,
      { origem: "regras" }, JSON.stringify(fatosExtRuins));
    assert.equal(kv.mapa.size, 0);
    assert.equal(chamadasLlm.length, 2);

    const chave = await chaveCacheCompor(reqExtRuim);
    const hitEnv = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "detalhe", origem: "llm",
        pecas: [{ t: "porque", alvo: "c1", confianca: 1, linhas: [{ m: "FT_MOT_GENERO", a: ["g878"] }] }],
      }),
    });
    await hitEnv.kv.put(chave, JSON.stringify({
      v: 1, tela: "detalhe", feita: 1795000000, vale_s: 86400, origem: "llm",
      pecas: cenaExt.pecas,
    }), { expirationTtl: 86400 });
    const rHit = await hitEnv.post("/v1/farol/compor", reqExtRuim, { token: "tok-gosto-b", perfil: 2 });
    assert.equal(rHit.body.origem, "llm");
    assert.equal(rHit.body.pecas[0].linhas[0].m, "FT_MOT_GENERO");
    assert.equal(hitEnv.chamadasLlm.length, 1);
  }

  // Fronteira de alias em eixos: eixos com ref de c1 nao pode vazar cabe_hoje:false para c1 nem conceder cabe_hoje para c0.
  {
    const candsAlias = candsCom({ ...serie80, eixos: { ref: "tt0133093", cabe_hoje: false } }).map((c, idx) =>
      idx === 1 ? { ...c, minutos: 80, motivos: [{ tipo: 6, a: 80, b: 0 }] } : c
    );
    const reqC1 = pedidoHomeBase({ modo: "dica", candidatos: candsAlias, fatos: [{ tempo_max: 100 }] });
    const cenaDicaC1 = {
      v: 1, tela: "home", origem: "llm",
      pecas: [{ t: "dica", id: "d_c1", l1: { m: "FT_DICA_CURTO", a: ["c1"] }, acao: { tipo: "abrir", alvo: "c1" } }],
    };
    const { post, chamadasLlm } = cenario(t, {}, { respostaLlm: () => cenaDicaC1 });
    const miss = await post("/v1/farol/compor", reqC1, { token: "tok-gosto-a", perfil: 1 });
    assert.equal(miss.body.origem, "llm");
    const hit = await post("/v1/farol/compor", reqC1, { token: "tok-gosto-b", perfil: 2 });
    assert.deepEqual(hit.body, miss.body);
    assert.equal(chamadasLlm.length, 1);
  }

  // Preserva semantica de ordem r5 para duracoes brutas sem afirmacao explicita de cabe_hoje.
  {
    const reqRawOk = pedidoHomeBase({
      modo: "dica",
      candidatos: candsCom(serieSemMin),
      fatos: [{ c: 0, minutos: 80 }, { id: "tt2010001", minutos: 160 }, { tempo_max: 100 }],
    });
    const reqRawRuim = pedidoHomeBase({
      modo: "dica",
      candidatos: candsCom(serieSemMin),
      fatos: [{ id: "tt2010001", minutos: 160 }, { c: 0, minutos: 80 }, { tempo_max: 100 }],
    });
    const { post: postOk } = cenario(t, {}, { respostaLlm: () => cenaDica });
    assert.equal((await postOk("/v1/farol/compor", reqRawOk)).body.origem, "llm");
    const { post: postRuim } = cenario(t, {}, { respostaLlm: () => cenaDica });
    assert.deepEqual((await postRuim("/v1/farol/compor", reqRawRuim)).body, { origem: "regras" });
  }

  // Controles positivos: negacao em c1 nao afeta c0, e IMDb externo unico com limite igual/frouxo passa.
  {
    const reqC0Ok = pedidoHomeBase({
      modo: "home",
      candidatos: candsCom(serie80),
      fatos: [{ c: 1, cabe_hoje: false, minutos: 80 }, { tempo_max: 100 }],
    });
    const { post, chamadasLlm } = cenario(t, {}, { respostaLlm: () => cenaFileira });
    const miss = await post("/v1/farol/compor", reqC0Ok, { token: "tok-gosto-a", perfil: 1 });
    assert.equal(miss.body.origem, "llm");
    const hit = await post("/v1/farol/compor", reqC0Ok, { token: "tok-gosto-b", perfil: 2 });
    assert.deepEqual(hit.body, miss.body);
    assert.equal(chamadasLlm.length, 1);
  }
  {
    const reqExtOk = pedidoHomeBase({
      modo: "retomada",
      fatos: [{ alvo: ext, cabe_hoje: true, minutos: 120 }, { tempo_max: 120 }],
    });
    const { post, chamadasLlm } = cenario(t, {}, { respostaLlm: () => cenaPorque(ext, "120") });
    const miss = await post("/v1/farol/compor", reqExtOk, { token: "tok-gosto-a", perfil: 1 });
    assert.equal(miss.body.origem, "llm");
    const hit = await post("/v1/farol/compor", reqExtOk, { token: "tok-gosto-b", perfil: 2 });
    assert.deepEqual(hit.body, miss.body);
    assert.equal(chamadasLlm.length, 1);
  }
});

test("R9-Achados: FT_ACHA_CURTOS_L1 nao permite fato agregado sobrepor item exibido inelegivel", async (t) => {
  const baseCands = pedidoHomeBase().candidatos;
  const cenaAchados = {
    v: 1, tela: "home", origem: "llm",
    pecas: [{
      t: "achados",
      l1: { m: "FT_ACHA_CURTOS_L1", a: ["2", "100"] },
      itens: ["c0", "c1"],
    }],
  };
  const mkCands = (c0Mut, c1Mut) => baseCands.map((c, idx) =>
    idx === 0 ? c0Mut(c) : idx === 1 ? c1Mut(c) : c
  );

  // 1. Negativos: c0=240 com c1 sem duracao e fato agregado; c0 sem minutos com fato 240;
  //    c0=80 com contradicao de duracao; agregado com curtos_qtd divergente; itens duplicados por alias.
  for (const [cands, fatos, cena] of [
    [
      mkCands((c) => ({ ...c, minutos: 240 }), ({ minutos: _m, ...rest }) => rest),
      [{ curtos_max_min: 100, curtos_qtd: 2 }],
      cenaAchados,
    ],
    [
      mkCands(({ minutos: _m, ...rest }) => rest, ({ minutos: _m, ...rest }) => rest),
      [{ c: 0, minutos: 240 }, { curtos_max_min: 100, curtos_qtd: 2 }],
      cenaAchados,
    ],
    [
      mkCands((c) => ({ ...c, minutos: 80 }), ({ minutos: _m, ...rest }) => rest),
      [{ c: 0, cabe_hoje: true, minutos: 240 }, { curtos_max_min: 100, curtos_qtd: 2 }],
      cenaAchados,
    ],
    [
      mkCands((c) => ({ ...c, minutos: 80 }), ({ minutos: _m, ...rest }) => rest),
      [{ curtos_max_min: 100, curtos_qtd: 1 }],
      cenaAchados,
    ],
    [
      mkCands((c) => ({ ...c, minutos: 80 }), ({ minutos: _m, ...rest }) => rest),
      [{ curtos_max_min: 100, curtos_qtd: 0 }],
      cenaAchados,
    ],
    [
      mkCands((c) => ({ ...c, minutos: 80 }), (c) => ({ ...c, minutos: 75 })),
      [{ curtos_max_min: 100, curtos_qtd: 2 }],
      {
        v: 1, tela: "home", origem: "llm",
        pecas: [{ t: "achados", l1: { m: "FT_ACHA_CURTOS_L1", a: ["2", "100"] }, itens: ["c0", baseCands[0].id] }],
      },
    ],
  ]) {
    const reqRuim = pedidoHomeBase({ modo: "home", candidatos: cands, fatos });
    const { post, kv, chamadasLlm } = cenario(t, {}, { respostaLlm: () => cena });
    const r1 = await post("/v1/farol/compor", reqRuim, { token: "tok-gosto-a", perfil: 1 });
    const r2 = await post("/v1/farol/compor", reqRuim, { token: "tok-gosto-b", perfil: 1 });
    assert.deepEqual(r1.body, { origem: "regras" }, JSON.stringify(fatos));
    assert.deepEqual(r2.body, { origem: "regras" }, JSON.stringify(fatos));
    assert.equal(kv.mapa.size, 0);
    assert.equal(chamadasLlm.length, 2);

    const chave = await chaveCacheCompor(reqRuim);
    const hitEnv = cenario(t, {}, {
      respostaLlm: () => ({
        v: 1, tela: "home", origem: "llm",
        pecas: [{ t: "dica", id: "d_ok", l1: { m: "FT_DICA_TRAKT" }, acao: { tipo: "nada" } }],
      }),
    });
    await hitEnv.kv.put(chave, JSON.stringify({
      v: 1, tela: "home", feita: 1795000000, vale_s: 86400, origem: "llm",
      pecas: cena.pecas,
    }), { expirationTtl: 86400 });
    const rHit = await hitEnv.post("/v1/farol/compor", reqRuim, { token: "tok-gosto-b", perfil: 2 });
    assert.equal(rHit.body.origem, "llm");
    assert.equal(rHit.body.pecas[0].l1.m, "FT_DICA_TRAKT");
  }

  // 2. Controles positivos: ambos curtos conhecidos, ou um curto conhecido (80) + c1 realmente desconhecido com agregado coerente.
  for (const [cands, fatos] of [
    [mkCands((c) => ({ ...c, minutos: 80 }), (c) => ({ ...c, minutos: 75 })), [{ curtos_max_min: 100, curtos_qtd: 2 }]],
    [mkCands((c) => ({ ...c, minutos: 80 }), ({ minutos: _m, ...rest }) => rest), [{ curtos_max_min: 100, curtos_qtd: 2 }]],
    [mkCands(({ minutos: _m, ...rest }) => rest, ({ minutos: _m, ...rest }) => rest), [{ curtos_max_min: 100, curtos_qtd: 2 }]],
  ]) {
    const reqOk = pedidoHomeBase({ modo: "home", candidatos: cands, fatos });
    const { post, chamadasLlm } = cenario(t, {}, { respostaLlm: () => cenaAchados });
    const miss = await post("/v1/farol/compor", reqOk, { token: "tok-gosto-a", perfil: 1 });
    assert.equal(miss.body.origem, "llm", JSON.stringify(fatos));
    assert.equal(miss.body.pecas[0].l1.m, "FT_ACHA_CURTOS_L1");
    const hit = await post("/v1/farol/compor", reqOk, { token: "tok-gosto-b", perfil: 2 });
    assert.deepEqual(hit.body, miss.body);
    assert.equal(chamadasLlm.length, 1);
  }
});
