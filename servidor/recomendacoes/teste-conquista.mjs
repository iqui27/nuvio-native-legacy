import { filmesDosCreditos, classe, rotaCaca, conquistasDe } from "./src/conquista.js";
import worker from "./src/index.js";
import { DatabaseSync } from "node:sqlite";
import { readFileSync } from "node:fs";
import assert from "node:assert";
const c = { crew: [{id:1,title:"A",job:"Director",release_date:"2010-01-01",genre_ids:[18]},{id:1,title:"A",job:"Writer",release_date:"2010-01-01"},{id:2,title:"F",job:"Director",release_date:"2999-01-01"},{id:3,title:"T",job:"Director",release_date:"2005-01-01",genre_ids:[10770]}],
  cast: [{id:9,title:"Doc",character:"Himself",release_date:"2001-01-01"},{id:8,title:"X",character:"Bob",release_date:"2001-01-01"}] };
const d = filmesDosCreditos(c, "dir", 20261006);
assert.deepEqual(d.map(f=>f.tmdb), [3,1]); assert.equal(d[0].classe, 3);
assert.deepEqual(filmesDosCreditos(c,"ator",20261006).map(f=>f.tmdb),[8]);
assert.equal(classe([99],0),1); assert.equal(classe([18],30),2);
// Import the actual entry point: duplicate imports must fail module loading.
assert.equal(typeof worker.fetch, "function");
const db = new DatabaseSync(":memory:");
try {
  db.exec(readFileSync(new URL("migracao-013-conquista.sql", import.meta.url), "utf8"));
  db.exec("CREATE TABLE pessoa (id TEXT PRIMARY KEY, alcance INTEGER); INSERT INTO pessoa VALUES ('A', 1)");
  const stmt = (sql, values) => {
    const s = db.prepare(sql);
    return {
      first: async () => s.get(...values),
      all: async () => ({ results: s.all(...values) }),
      run: async () => s.run(...values),
    };
  };
  const env = { DB: { prepare: (sql) => ({ ...stmt(sql, []), bind: (...v) => stmt(sql, v) }) } };
  const h = { agora: () => 100, limpar: (s, n) => String(s || "").slice(0, n),
    json: (b) => Response.json(b), erro: (e, status) => Response.json({ erro: e }, { status }) };
  const body = { chave: "dir:42", nivel: 4, visivel: 1, nome: "Completed Hunt", vistos: 2, total: 2 };
  const post = (visivel) => rotaCaca("/v1/conquista", "POST", env, { id: "A" },
    { ...body, visivel }, new URL("https://worker.test/v1/conquista"), h);
  assert.equal((await post(1)).status, 200);
  assert.equal((await conquistasDe(env, "A")).length, 1);
  // Only me after completion: update server visibility without another unlock.
  assert.equal((await post(0)).status, 200);
  assert.equal(db.prepare("SELECT visivel FROM conquista").get().visivel, 0);
  assert.equal((await conquistasDe(env, "A")).length, 0);
  assert.equal(db.prepare("SELECT nivel, quando FROM conquista").get().nivel, 4);
} finally {
  db.close();
}
console.log("conquista.js: parser, Worker load and server privacy ok");
