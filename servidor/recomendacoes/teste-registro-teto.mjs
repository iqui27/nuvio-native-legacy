// #203: teto, corte cabeca+cauda, dedupe e retencao do registro. Node 22+:
//   node --test servidor/recomendacoes/teste-registro-teto.mjs
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { DatabaseSync } from "node:sqlite";
import test from "node:test";
import worker, { cortarRegistro, limparRegistro } from "./src/index.js";

function cenario(t) {
  const sqlite = new DatabaseSync(":memory:");
  t.after(() => sqlite.close());
  for (const m of ["schema.sql", "migracao-003-registro.sql"])
    for (const cmd of readFileSync(new URL(m, import.meta.url), "utf8").split(/;\s*\n/)) {
      const c = cmd.replace(/^\s*--.*$/gm, "").trim();
      if (c) sqlite.exec(c);
    }
  const stmt = (sql, v) => {
    const st = sqlite.prepare(sql);
    return {
      first: async () => st.get(...v) ?? null,
      run: async () => { const r = st.run(...v);
        return { meta: { changes: Number(r.changes), last_row_id: Number(r.lastInsertRowid) } }; },
    };
  };
  const env = { DIAG_TOKEN: "tok", DB: { prepare: (sql) => ({ ...stmt(sql, []), bind: (...v) => stmt(sql, v) }) } };
  const post = async (corpo, tv = "tv1") => {
    const res = await worker.fetch(new Request("https://w.test/v1/registro", {
      method: "POST",
      headers: { "x-nuvio-auth": "diagnostico", authorization: "Bearer tok" },
      body: JSON.stringify({ versao: "2.0", plataforma: "webos", tv, ...corpo }) }), env);
    return { status: res.status, body: await res.json() };
  };
  return { sqlite, env, post };
}
const linhas = (n, c = "x") => Array.from({ length: n }, (_, i) => `${c}${i} ${"y".repeat(60)}`).join("\n");

test("cortarRegistro: cabeca + cauda com marcador, dentro do teto", () => {
  const txt = linhas(5000);
  const r = cortarRegistro(txt, 64 * 1024);
  assert.ok(r.length <= 64 * 1024);
  assert.match(r, /\.\.\. \d+ bytes omitidos \.\.\./);
  assert.ok(r.startsWith("x0 "));
  assert.ok(r.endsWith(txt.slice(-100)));
  assert.equal(cortarRegistro("curto", 100), "curto");
});

test("auto e anterior: 64 KB; manual: 256 KB", async (t) => {
  const { sqlite, post } = cenario(t);
  const grande = linhas(12000);   // ~800 KB
  await post({ quando: "2026-10-07 10:00 (anterior)", texto: grande }, "a");
  await post({ quando: "2026-10-07 10:00", texto: grande }, "b");
  const n = (p) => sqlite.prepare("SELECT length(texto) AS n FROM registro WHERE pessoa = ?").get(p).n;
  assert.ok(n("diag:a") <= 64 * 1024);
  assert.ok(n("diag:b") <= 256 * 1024 && n("diag:b") > 200 * 1024);
});

test("corpo acima de 2 MB: 413", async (t) => {
  const { post } = cenario(t);
  assert.equal((await post({ quando: "x", texto: "z".repeat(2.1 * 1024 * 1024) })).status, 413);
});

test("texto identico da mesma pessoa nao cria linha e devolve o mesmo recibo", async (t) => {
  const { sqlite, post } = cenario(t);
  const a = await post({ quando: "2026-10-07 10:00 (anterior)", texto: "log igual" });
  const b = await post({ quando: "2026-10-07 11:00", texto: "log igual" });
  const c = await post({ quando: "2026-10-07 11:00", texto: "log igual" }, "outra");
  assert.equal(b.body.registro_id, a.body.registro_id);
  assert.notEqual(c.body.registro_id, a.body.registro_id);
  assert.equal(sqlite.prepare("SELECT COUNT(*) AS n FROM registro").get().n, 2);
});

test("(auto) seguido, texto diferente, na janela de 15 min: substitui", async (t) => {
  const { sqlite, post } = cenario(t);
  await post({ quando: "2026-10-07 10:00 (auto)", texto: "um" });
  await post({ quando: "2026-10-07 10:05 (auto)", texto: "dois" });
  const l = sqlite.prepare("SELECT texto FROM registro").all();
  assert.deepEqual(l.map((x) => x.texto), ["dois"]);
});

test("retencao: 4 dias, arranque 30, em lotes", async (t) => {
  const { sqlite, env } = cenario(t);
  const t0 = Math.floor(Date.now() / 1000), dia = 86400;
  const ins = sqlite.prepare("INSERT INTO registro (pessoa, criado, texto) VALUES (?, ?, '')");
  for (let i = 0; i < 450; i++) ins.run("p" + i, t0 - 5 * dia);      // velhos: saem
  for (let i = 0; i < 10; i++) ins.run("p" + i, t0 - 3 * dia);       // novos: ficam
  for (let i = 0; i < 5; i++) ins.run("arranque:x", t0 - 10 * dia);  // arranque 10 dias: fica
  for (let i = 0; i < 5; i++) ins.run("arranque:x", t0 - 31 * dia);  // arranque 31 dias: sai
  await limparRegistro(env, t0);
  const q = (w) => sqlite.prepare(`SELECT COUNT(*) AS n FROM registro WHERE ${w}`).get().n;
  assert.equal(q("pessoa NOT LIKE 'arranque:%' AND criado < " + (t0 - 4 * dia)), 0);
  assert.equal(q("pessoa NOT LIKE 'arranque:%'"), 10);
  assert.equal(q("pessoa LIKE 'arranque:%'"), 5);
});
