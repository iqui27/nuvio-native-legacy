// #266: registro de arranque sem conta (vigia do APK Android). Worker inteiro
// com SQLite em memoria, como teste-homonimos.mjs. Node 22+:
//   node --test servidor/recomendacoes/teste-arranque.mjs
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { DatabaseSync } from "node:sqlite";
import test from "node:test";
import worker from "./src/index.js";

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
      all: async () => ({ results: st.all(...v) }),
      run: async () => { const r = st.run(...v);
        return { meta: { changes: Number(r.changes), last_row_id: Number(r.lastInsertRowid) } }; },
    };
  };
  const env = { DB: { prepare: (sql) => ({ ...stmt(sql, []), bind: (...v) => stmt(sql, v) }) } };
  const post = async (corpo) => {
    const res = await worker.fetch(new Request("https://w.test/v1/registro/arranque", {
      method: "POST", body: typeof corpo === "string" ? corpo : JSON.stringify(corpo) }), env);
    return { status: res.status, body: await res.json() };
  };
  return { sqlite, post };
}

test("arranque sem conta grava sob arranque:<tv>, corta o texto e da codigo", async (t) => {
  const { sqlite, post } = cenario(t);
  const r = await post({ versao: "2.0.1", plataforma: "android", tv: "TCL 43P745!", texto: "x".repeat(70000) });
  assert.equal(r.status, 200);
  assert.equal(r.body.codigo.length, 6);
  const l = sqlite.prepare("SELECT pessoa, length(texto) AS n FROM registro").get();
  assert.equal(l.pessoa, "arranque:TCL43P745");
  assert.equal(l.n, 64 * 1024);
});

test("arranque: corpo enorme e teto diario", async (t) => {
  const { sqlite, post } = cenario(t);
  assert.equal((await post("y".repeat(140 * 1024))).status, 413);
  const ins = sqlite.prepare("INSERT INTO registro (pessoa, criado) VALUES ('arranque:a', ?)");
  for (let i = 0; i < 300; i++) ins.run(Math.floor(Date.now() / 1000));
  assert.equal((await post({ tv: "b", texto: "z" })).status, 429);
});
