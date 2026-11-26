// Vai tocar nesta TV (4.2): POST /v1/vaitocar pelo Worker inteiro, com SQLite
// de verdade para a tabela (migracao 022) e para a cota. Node 22+:
//   node --test servidor/recomendacoes/teste-vaitocar.mjs
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { DatabaseSync } from "node:sqlite";
import test from "node:test";
import worker from "./src/index.js";
import { firmwareTecnico, limparVaiTocar, modeloTecnico, vaitocarZerarCota, validarVaiTocar,
  VAITOCAR_GLOBAL_HORA, VAITOCAR_RETENCAO_DIAS } from "./src/vtreg.js";

// O MESMO texto que tests/vtreg.c exige de vtreg_json: se o app mudar o
// formato, os dois testes tem de mudar juntos.
const DO_APP = '{"v":1,"plataforma":"webos","modelo":"OLED55C9PSA","firmware":"05.30.25",' +
  '"container":"mkv","video":"hevc","audio":"eac3","hdr":"dv","mbps":40,' +
  '"altura":2160,"resultado":"tocou","trocou":0}';

function ambiente(t) {
  vaitocarZerarCota();
  const sqlite = new DatabaseSync(":memory:");
  t.after(() => sqlite.close());
  sqlite.exec("CREATE TABLE limite (chave TEXT PRIMARY KEY, janela INTEGER NOT NULL, n INTEGER NOT NULL, expira INTEGER NOT NULL)");
  sqlite.exec(readFileSync(new URL("migracao-022-vaitocar.sql", import.meta.url), "utf8"));
  const stmt = (sql, v) => {
    const st = sqlite.prepare(sql);
    return { first: async () => st.get(...v) ?? null, all: async () => ({ results: st.all(...v) }),
      run: async () => { const r = st.run(...v); return { meta: { changes: Number(r.changes) } }; } };
  };
  const DB = { prepare: (sql) => ({ ...stmt(sql, []), bind: (...v) => stmt(sql, v) }) };
  return { sqlite, env: { DB } };
}
const pedir = (env, corpo, ip = "192.0.2.7") => worker.fetch(new Request("https://x/v1/vaitocar", {
  method: "POST", headers: { "cf-connecting-ip": ip, "content-type": "application/json" },
  body: typeof corpo === "string" ? corpo : JSON.stringify(corpo),
}), env, { waitUntil() {} });

test("aceita o registro do app e guarda so contagem", async (t) => {
  const { sqlite, env } = ambiente(t);
  for (let i = 0; i < 3; i++) assert.equal((await pedir(env, DO_APP)).status, 200);
  const linhas = sqlite.prepare("SELECT * FROM vaitocar").all();
  assert.equal(linhas.length, 1);
  assert.equal(linhas[0].n, 3);
  assert.deepEqual(Object.keys(linhas[0]).sort(), ["altura", "audio", "container", "dia", "firmware", "hdr",
    "mbps", "modelo", "n", "plataforma", "resultado", "trocou", "video"]);
  // O IP nao esta em lugar nenhum do D1, nem em hash: a unica chave de cota
  // gravada e a global, fixa.
  const tudo = JSON.stringify(linhas) + JSON.stringify(sqlite.prepare("SELECT * FROM limite").all());
  assert.ok(!tudo.includes("192.0.2.7"));
  assert.deepEqual(sqlite.prepare("SELECT chave FROM limite").all().map((l) => l.chave), ["vaitocar:global"]);
  // Outro resultado e outra linha.
  assert.equal((await pedir(env, { ...JSON.parse(DO_APP), resultado: "nao_abriu", trocou: 1 })).status, 200);
  assert.equal(sqlite.prepare("SELECT COUNT(*) c FROM vaitocar").get().c, 2);
});

test("recusa titulo, hash, endereco e qualquer chave a mais", async (t) => {
  const { sqlite, env } = ambiente(t);
  const base = JSON.parse(DO_APP);
  const ruins = [
    { ...base, titulo: "Filme" }, { ...base, url: "http://a/b.mkv" }, { ...base, hash: "abc" },
    { ...base, modelo: "x".repeat(65) }, { ...base, firmware: 11 }, { ...base, video: "Filme.2024.HEVC" }, { ...base, container: "mkv " },
    { ...base, hdr: "DolbyVision" }, { ...base, resultado: "ok" }, { ...base, mbps: 23 }, { ...base, altura: 2161 },
    { ...base, trocou: 2 }, { ...base, v: 2 }, { ...base, modelo: 5 }, [base], "x", null,
    (() => { const c = { ...base }; delete c.audio; return c; })(),
  ];
  for (const c of ruins) assert.equal((await pedir(env, c)).status, 400, JSON.stringify(c));
  assert.equal((await pedir(env, "{")).status, 400);
  assert.equal((await pedir(env, JSON.stringify({ ...base, modelo: "a" }) + " ".repeat(2000))).status, 413);
  assert.equal(sqlite.prepare("SELECT COUNT(*) c FROM vaitocar").get().c, 0);
  assert.equal(validarVaiTocar({ ...base, modelo: "?", firmware: "03.21.30.1" })?.length, 11);
});

test("modelo e firmware: so valor tecnico; texto livre vira ? e nao entra no agregado", async (t) => {
  const { sqlite, env } = ambiente(t);
  const base = JSON.parse(DO_APP);
  const livres = [
    { modelo: "http://servidor/filme.mkv" }, { modelo: "0123456789abcdef0123456789abcdef01234567" },
    { modelo: "192.168.100.24" }, { firmware: "joao@exemplo.com" }, { modelo: "Filme (2024)" },
    { modelo: "Joao Silva" }, { modelo: "Sala" }, { modelo: "Joao-Silva" }, { modelo: "TV-da-Ana-2" },
    { modelo: "x".repeat(48) }, { modelo: "tel11987654321" }, { firmware: "versao nova" },
    { firmware: "192.168.100.24" }, { firmware: "10.0.0.1" }, { firmware: "6.18.44-generic" },
    { firmware: "1234" }, { modelo: "", firmware: "" },
  ];
  for (const l of livres) assert.equal((await pedir(env, { ...base, ...l })).status, 200, JSON.stringify(l));
  const guardado = sqlite.prepare("SELECT DISTINCT modelo, firmware FROM vaitocar").all();
  for (const g of guardado) {
    assert.ok(g.modelo === "?" || g.modelo === base.modelo, JSON.stringify(g));
    assert.ok(g.firmware === "?" || g.firmware === base.firmware, JSON.stringify(g));
  }
  assert.ok(!JSON.stringify(guardado).match(/joao|Joao|Ana|Sala|Filme|http|192\.168|10\.0|abcdef|generic|nova/));
  // O que e tecnico passa como veio, conferido pela plataforma.
  const ok = [["webos", "OLED55C9PSA"], ["webos", "50NANO80ASA"], ["webos", "43UK6300PSB"], ["tizen", "QN65Q80BAGXZD"],
    ["tizen", "UN55TU7000"], ["android", "TCL-55C735"], ["android", "Sony-BRAVIA-4K-VH2"], ["android", "Google-Chromecast"],
    ["android", "NVIDIA-SHIELD-Android-TV"], ["android", "Xiaomi-MiTV-MOOQ1"], ["android", "Amazon-AFTMM"], ["android", "Chromecast"], ["linux", "x86_64"], ["macos", "arm64"]];
  for (const [p, m] of ok) assert.equal(modeloTecnico(m, p), m, p + " " + m);
  const ruim = [["webos", "Joao-Silva"], ["webos", "TV-da-Ana-2"], ["webos", "TCL-55C735"], ["tizen", "OLED55C9PSA"],
    ["android", "Joao-Silva"], ["android", "Chromecast-da-Ana"], ["android", "OLED55C9PSA"], ["android", "Sony"],
    ["linux", "meu-pc"], ["outro", "OLED55C9PSA"], ["android", "Sony-12345678"], ["android", "Sony-TV-da-Ana"], ["android", "Amazon-Sala"]];
  for (const [p, m] of ruim) assert.equal(modeloTecnico(m, p), "?", p + " " + m);
  for (const f of ["05.30.25", "11", "6.18.44"]) assert.equal(firmwareTecnico(f), f);
  for (const f of ["6.", "1.2.3.4", "03.21.30.1", "10.0.0.1", "255.255.255.0", "v1.2", "1234"]) assert.equal(firmwareTecnico(f), "?");
});

test("teto global por hora no D1: condicional, sem chave de IP, e 400 nao gasta", async (t) => {
  const { sqlite, env } = ambiente(t);
  const janela = Math.floor(Date.now() / 1000 / 3600);
  const global = () => sqlite.prepare("SELECT n FROM limite WHERE chave = 'vaitocar:global'").get()?.n ?? 0;
  // Pedidos invalidos (400) nao debitam o teto global.
  assert.equal((await pedir(env, "{", "198.51.100.1")).status, 400);
  assert.equal((await pedir(env, { ...JSON.parse(DO_APP), titulo: "x" }, "198.51.100.1")).status, 400);
  assert.equal(global(), 0);
  assert.equal((await pedir(env, DO_APP, "198.51.100.1")).status, 200);
  assert.equal(global(), 1);
  // No teto: recusa, e o contador NAO passa do teto (incremento condicional).
  sqlite.prepare("UPDATE limite SET n = ? WHERE chave = 'vaitocar:global'").run(VAITOCAR_GLOBAL_HORA);
  assert.equal((await pedir(env, DO_APP, "198.51.100.9")).status, 429);
  assert.equal((await pedir(env, DO_APP, "198.51.100.10")).status, 429);
  assert.equal(global(), VAITOCAR_GLOBAL_HORA);
  assert.equal(sqlite.prepare("SELECT SUM(n) s FROM vaitocar").get().s, 1);
  // Janela seguinte: o UPSERT zera e volta a aceitar.
  sqlite.prepare("UPDATE limite SET janela = ? WHERE chave = 'vaitocar:global'").run(janela - 1);
  assert.equal((await pedir(env, DO_APP, "198.51.100.11")).status, 200);
  assert.equal(global(), 1);
  assert.deepEqual(sqlite.prepare("SELECT chave FROM limite").all().map((l) => l.chave), ["vaitocar:global"]);
});

test("cota por IP (so em memoria) e retencao", async (t) => {
  const { sqlite, env } = ambiente(t);
  let ultimo = 0;
  for (let i = 0; i < 61; i++) ultimo = (await pedir(env, DO_APP)).status;
  assert.equal(ultimo, 429);
  assert.equal((await pedir(env, DO_APP, "192.0.2.8")).status, 200);
  assert.equal(sqlite.prepare("SELECT SUM(n) s FROM vaitocar").get().s, 61);
  const hoje = Math.floor(Date.now() / 1000);
  await limparVaiTocar(env, hoje + (VAITOCAR_RETENCAO_DIAS - 1) * 86400);
  assert.equal(sqlite.prepare("SELECT COUNT(*) c FROM vaitocar").get().c, 1);
  await limparVaiTocar(env, hoje + (VAITOCAR_RETENCAO_DIAS + 2) * 86400);
  assert.equal(sqlite.prepare("SELECT COUNT(*) c FROM vaitocar").get().c, 0);
  assert.equal((await worker.fetch(new Request("https://x/v1/vaitocar"), env, { waitUntil() {} })).status !== 200, true);
});

// Ponta a ponta (tests/vtreg.sh): os corpos que src/vtregenvio.c mandaria.
test("o que o app manda entra", { skip: !process.env.VTREG_CORPOS }, async (t) => {
  const { sqlite, env } = ambiente(t);
  const corpos = readFileSync(process.env.VTREG_CORPOS, "utf8").split("\n").filter(Boolean);
  assert.ok(corpos.length >= 3);
  for (const c of corpos) assert.equal((await pedir(env, c)).status, 200, c);
  assert.equal(sqlite.prepare("SELECT SUM(n) s FROM vaitocar").get().s, corpos.length);
});
