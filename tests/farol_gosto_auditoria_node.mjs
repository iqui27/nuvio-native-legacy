import assert from "node:assert/strict";
import { rotaFarolCompor, rotaFarolEscolha } from "../servidor/recomendacoes/src/farolgosto.js";

const CAN_PERFIL = "CANARIO_PERFIL_901";
const CAN_TITULO = "CANARIO_TITULO_902";
const CAN_FRASE = "filme curto CANARIO_FRASE_903";
const CAN_TOKEN = "CANARIO_TOKEN_904";
const CAN_URL = "https://canario905.example/segredo.mkv";
const CONTADORES_LOCAIS = [
  "77101", "77102", "77103", "77104", "77105", "77106", "77107",
  "cenasTv", "cenasServidor", "expVistas", "expAbertas", "normAbertas",
];

function makeHelpers(t = 1795000000) {
  return {
    json(data, status = 200) {
      return { status, body: data };
    },
    erro(msg, status = 400) {
      return { status, body: { erro: msg } };
    },
    agora() {
      return t;
    },
  };
}

const origLog = console.log;
const origWarn = console.warn;
const origError = console.error;
const logs = [];
console.log = (...a) => logs.push(a.map(String).join(" "));
console.warn = (...a) => logs.push(a.map(String).join(" "));
console.error = (...a) => logs.push(a.map(String).join(" "));

const env = {
  FAROL_LLM: "1",
  FAROL_LLM_KEY: CAN_TOKEN,
  FAROL_LLM_PROVEDOR: "anthropic",
  FAROL_SAL: "sal_teste_auditoria",
  DB: {
    prepare() {
      return {
        bind() {
          return {
            async first() {
              return { infantil: 0 };
            },
          };
        },
      };
    },
  },
};
const quem = { id: "conta-canario-1" };
const limitar = async () => true;

const llmPayloads = [];
const fetchLlmCompor = async (_url, init) => {
  llmPayloads.push(String(init?.body || ""));
  return {
    ok: true,
    status: 200,
    async json() {
      return {
        content: [
          {
            type: "text",
            text: JSON.stringify({
              pecas: [
                {
                  t: "fileira",
                  id: "gosto1",
                  titulo: { m: "FT_FIL_RECOMENDADOS" },
                  forma: "poster",
                  itens: ["c0", "c1", "c2", "c3", "c4", "c5"],
                },
              ],
            }),
          },
        ],
      };
    },
  };
};

// 1) Bloqueios estritos: consentiu:false / infantil:true nao chamam fetchFn.
{
  let chamadas = 0;
  const fetchProibido = async () => {
    chamadas++;
    throw new Error("nao deveria chamar LLM");
  };
  const baseCompor = {
    v: 1,
    modo: "home",
    consentiu: false,
    idioma: "pt-BR",
    gosto: { generos: [{ id: 18, nome: "Drama", peso: 3 }] },
    candidatos: [
      { id: "tt0001", tipo: "filme", minutos: 100 },
      { id: "tt0002", tipo: "filme", minutos: 95 },
      { id: "tt0003", tipo: "filme", minutos: 90 },
      { id: "tt0004", tipo: "filme", minutos: 105 },
      { id: "tt0005", tipo: "filme", minutos: 110 },
      { id: "tt0006", tipo: "filme", minutos: 115 },
    ],
  };
  const r1 = await rotaFarolCompor(env, quem, baseCompor, makeHelpers(), limitar, fetchProibido);
  assert.equal(r1.body.origem, "regras");
  assert.equal(chamadas, 0);

  const r2 = await rotaFarolCompor(
    env,
    quem,
    { ...baseCompor, consentiu: true, infantil: true },
    makeHelpers(),
    limitar,
    fetchProibido
  );
  assert.equal(r2.body.origem, "regras");
  assert.equal(chamadas, 0);
}

// 2) Compor real com campo estranho/contador local/perfil deve rejeitar (400).
{
  const rExtra = await rotaFarolCompor(
    env,
    quem,
    {
      v: 1,
      modo: "home",
      consentiu: true,
      perfil: CAN_PERFIL,
      vistas: 77101,
      gosto: { generos: [{ id: 18, peso: 3 }] },
      candidatos: [
        { id: "tt0001", tipo: "filme" },
        { id: "tt0002", tipo: "filme" },
        { id: "tt0003", tipo: "filme" },
        { id: "tt0004", tipo: "filme" },
        { id: "tt0005", tipo: "filme" },
        { id: "tt0006", tipo: "filme" },
      ],
    },
    makeHelpers(),
    limitar,
    fetchLlmCompor
  );
  assert.equal(rExtra.status, 400);
}

// 3) Compor real consentido chama fetchLlmCompor sem vazar perfil/token/url/contadores.
{
  const rOk = await rotaFarolCompor(
    env,
    quem,
    {
      v: 1,
      modo: "home",
      consentiu: true,
      idioma: "pt-BR",
      gosto: { generos: [{ id: 18, nome: "Drama", peso: 3 }] },
      candidatos: [
        { id: "tt1001", tipo: "filme", minutos: 100 },
        { id: "tt1002", tipo: "filme", minutos: 95 },
        { id: "tt1003", tipo: "filme", minutos: 90 },
        { id: "tt1004", tipo: "filme", minutos: 105 },
        { id: "tt1005", tipo: "filme", minutos: 110 },
        { id: "tt1006", tipo: "filme", minutos: 115 },
      ],
    },
    makeHelpers(),
    limitar,
    fetchLlmCompor
  );
  assert.equal(rOk.status, 200);
  assert.equal(llmPayloads.length, 1);
  for (const segredo of [CAN_PERFIL, CAN_TITULO, CAN_FRASE, CAN_TOKEN, CAN_URL, ...CONTADORES_LOCAIS]) {
    assert.ok(!llmPayloads[0].includes(segredo), "vazou em compor LLM: " + segredo);
  }
}

// 4) Escolha real com URL na frase e barrada ou normalizada, e sem vazar perfil/token/contadores.
{
  const fetchLlmEscolha = async (_url, init) => {
    llmPayloads.push(String(init?.body || ""));
    return {
      ok: true,
      status: 200,
      async json() {
        return {
          content: [
            {
              type: "text",
              text: JSON.stringify({
                respostas: { tempo: "t45" },
                falta: "humor",
              }),
            },
          ],
        };
      },
    };
  };
  const rEsc = await rotaFarolEscolha(
    env,
    quem,
    {
      v: 1,
      consentiu: true,
      idioma: "pt-BR",
      frase: CAN_FRASE,
      perguntas: ["tempo", "humor"],
      gosto: { generos: [{ id: 18, peso: 3 }] },
    },
    makeHelpers(),
    limitar,
    fetchLlmEscolha
  );
  assert.equal(rEsc.status, 200);
  assert.equal(rEsc.body.origem, "llm");
  const rEscUrl = await rotaFarolEscolha(
    env,
    quem,
    {
      v: 1,
      consentiu: true,
      idioma: "pt-BR",
      frase: CAN_URL,
      perguntas: ["tempo"],
      gosto: { generos: [{ id: 18, peso: 3 }] },
    },
    makeHelpers(),
    limitar,
    fetchLlmEscolha
  );
  assert.equal(rEscUrl.status, 400);
  const escBody = llmPayloads[llmPayloads.length - 1];
  for (const segredo of [CAN_PERFIL, CAN_TITULO, CAN_TOKEN, CAN_URL, ...CONTADORES_LOCAIS]) {
    assert.ok(!escBody.includes(segredo), "vazou em escolha LLM: " + segredo);
  }
}

console.log = origLog;
console.warn = origWarn;
console.error = origError;

const logJoined = logs.join("\n");
for (const segredo of [CAN_PERFIL, CAN_TITULO, CAN_FRASE, CAN_TOKEN, CAN_URL, ...CONTADORES_LOCAIS]) {
  assert.ok(!logJoined.includes(segredo), "vazou em log Node: " + segredo);
}
console.log("auditoria-node: tudo ok");
