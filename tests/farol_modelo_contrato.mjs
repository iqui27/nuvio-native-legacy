// Contrato dirigido farol-8 (r3): o aparelho.modelo que o cliente emite tem
// de passar EXATAMENTE no modeloOk do servidor publicado — fora dela o
// registro inteiro volta 422 (a regressao da r2: a ponta emitia
// "50NANO80ASA" e o servidor recusava o pacote da LG NANO por inteiro).
//
// Para cada modelo do corpus a sonda C (coletor + redator de producao)
// devolve o FarolJson; o aparelho.modelo emitido e conferido contra o valor
// esperado, contra o modeloOk REAL importado de src/farol.js e contra a
// rotaFarolRegistro local (SQLite em memoria, fixture oficial C) — 200 e
// registro gravado em TODOS os casos.
//
//   node tests/farol_modelo_contrato.mjs <binario-sonda> [raiz-do-servidor]
//
// O terceiro argumento permite apontar os fontes do servidor (farol.js,
// codigo.js, fixture) para outra arvore — usado na prova do pai, onde a
// sonda compilada do commit reprovado emite o modelo que a rota recusa.
import assert from "node:assert/strict";
import { execFileSync } from "node:child_process";
import { readFileSync } from "node:fs";
import { DatabaseSync } from "node:sqlite";
import { dirname, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const propria = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const sonda = process.argv[2];
const RAIZ = process.argv[3] ? resolve(process.argv[3]) : propria;
assert(sonda, "uso: node tests/farol_modelo_contrato.mjs <binario-sonda> [raiz]");

const dir = "servidor/recomendacoes";
const { modeloOk, rotaFarolRegistro } =
  await import(pathToFileURL(resolve(RAIZ, dir, "src/farol.js")));
const { codigoRegistro } =
  await import(pathToFileURL(resolve(RAIZ, dir, "src/codigo.js")));
const fixture = JSON.parse(
  readFileSync(resolve(RAIZ, dir, "teste-farol-fixture-c.json"), "utf8"));

// [entrada do receptor, aparelho.modelo que o JSON tem de emitir].
// A gramatica publicada so aceita "x" ou prefixo da lista + 2 digitos de
// tela + serie/numero/sufixo nos tetos. LG com digito a frente (NANO, LM,
// QNED, UQ) e fabricante fora da lista viram "x" — como no pai.
const CORPUS = [
  ["OLED55C9PSA", "OLED55C9PSA"],
  ["OLED65G4PSA", "OLED65G4PSA"],
  ["50NANO80ASA", "x"],
  ["43LM6300PSB", "x"],
  ["65QNED85AQA", "x"],
  ["50UQ7500PSF", "x"],
  ["QN55Q80BAGXZD", "QN55Q80BAGXZD"],
  ["UN50TU8000GXZD", "UN50TU8000GXZD"],
  ["UE55AU7700", "UE55AU7700"],
  ["QA55LS03B", "x"],
  ["KD-55X90J", "KD-55X90J"],
  ["XR-65A80J", "XR-65A80J"],
  ["TCL55C745", "TCL55C745"],
  ["TCL 55C745", "TCL55C745"],
  ["Sony BRAVIA 4K VH2", "x"],
  ["Google Chromecast", "x"],
  ["AFTMM", "x"],
  ["123456", "x"],
  ["https://example.invalid/tv", "x"],
  ["teste@example.invalid", "x"],
  ["TV_192.168.0.1", "x"],
];

const sqlite = new DatabaseSync(":memory:");
sqlite.exec(`
  CREATE TABLE farol_dedup (
    pessoa_h TEXT, fp TEXT, criado INTEGER, registro INTEGER, confirmado INTEGER,
    toques INTEGER, PRIMARY KEY (pessoa_h, fp)
  );
  CREATE TABLE farol_registro (
    id INTEGER PRIMARY KEY, criado INTEGER, janela INTEGER, pessoa_h TEXT, versao TEXT,
    plataforma TEXT, so_maior INTEGER, codigo TEXT, fp TEXT, fingerprint TEXT,
    vezes INTEGER, json TEXT
  );
  INSERT INTO farol_registro(id) VALUES (536870911);
`);
const DB = {
  prepare(sql) {
    return {
      bind(...valores) {
        return {
          first: async () => sqlite.prepare(sql).get(...valores),
          all: async () => ({ results: sqlite.prepare(sql).all(...valores) }),
          run: async () => sqlite.prepare(sql).run(...valores),
        };
      },
    };
  },
};
const h = {
  agora: () => 1784000000,
  json: (body) => ({ status: 200, body }),
  erro: (motivo, status) => ({ status, motivo }),
};

const modeloEmitido = (entrada) => {
  const saida = execFileSync(sonda, [entrada], { encoding: "utf8" });
  const linha = saida.split("\n").find((texto) => texto.startsWith("{"));
  assert(linha, `sonda sem JSON para ${JSON.stringify(entrada)}`);
  return JSON.parse(linha).aparelho.modelo;
};

const temModeloOk = typeof modeloOk === "function";
if (!temModeloOk)
  console.log("aviso: farol.js desta arvore nao exporta modeloOk; validacao so pela rota");

for (const [entrada, esperado] of CORPUS) {
  const modelo = modeloEmitido(entrada);
  const ok = temModeloOk ? modeloOk(modelo) : null;
  sqlite.exec("DELETE FROM farol_dedup; DELETE FROM farol_registro WHERE json IS NOT NULL;");
  const corpo = { ...fixture, aparelho: { ...fixture.aparelho, modelo } };
  const resposta = await rotaFarolRegistro(
    { DB, FAROL_SAL: "sal-local-sintetico" }, { id: "nuvio:sonda-contrato" },
    corpo, h, async () => true, codigoRegistro);
  const gravado = sqlite.prepare(
    "SELECT json FROM farol_registro WHERE json IS NOT NULL").get();
  console.log(`${JSON.stringify(entrada)} -> modelo=${JSON.stringify(modelo)}` +
    ` / modeloOk=${ok === null ? "?" : Number(ok)} / HTTP ${resposta.status}` +
    ` / gravado=${Boolean(gravado)}`);
  assert.equal(modelo, esperado,
    `${JSON.stringify(entrada)}: emitido ${JSON.stringify(modelo)}`);
  assert.ok(ok !== false, `modeloOk recusou ${JSON.stringify(modelo)}`);
  assert.equal(resposta.status, 200,
    `${JSON.stringify(entrada)}: rota devolveu ${resposta.status} ${resposta.motivo ?? ""}`);
  assert.ok(gravado, `${JSON.stringify(entrada)}: registro nao gravado`);
}
sqlite.close();
console.log(`contrato: ${CORPUS.length} modelos aceitos pela rota`);
