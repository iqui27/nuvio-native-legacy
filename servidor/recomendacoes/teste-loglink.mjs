// Loglink (Matis94 adaptado): upload da copia mascarada do registro -> link de
// 72 h. Worker inteiro com KV em memoria; o D1 do worker inteiro entra pelo
// mesmo cenario de teste-arranque.mjs mas aqui basta o KV.
//   node --test servidor/recomendacoes/teste-loglink.mjs
import assert from "node:assert/strict";
import test from "node:test";
import worker from "./src/index.js";

function cenario() {
  const kv = new Map();
  const env = {
    DIAG_TOKEN: "tok-teste",
    LOGS: {
      put: async (k, v) => kv.set(k, v),
      get: async (k, t) => kv.get(k) ?? null,
    },
  };
  const post = (corpo, hdrs = {}) =>
    worker.fetch(new Request("https://w.test/v1/loglink", {
      method: "POST",
      headers: { "x-nuvio-auth": "diagnostico", ...hdrs },
      body: corpo,
    }), env);
  return { kv, env, post };
}

test("post autenticado grava gzip no KV e devolve link que baixa", async () => {
  const { kv, env, post } = cenario();
  const gz = new Uint8Array([0x1f, 0x8b, 0x08, 1, 2, 3, 0x00]);
  const r = await post(gz, { authorization: "Bearer tok-teste" });
  assert.equal(r.status, 200);
  const body = await r.json();
  assert.equal(body.ok, 1);
  assert.match(body.url, /^https:\/\/w\.test\/v1\/loglink\/[0-9a-f]{32}$/);
  const id = body.url.split("/").pop();
  assert.deepEqual([...new Uint8Array(kv.get("log:" + id))], [...gz]);

  const g = await worker.fetch(new Request(body.url), env);
  assert.equal(g.status, 200);
  assert.equal(g.headers.get("content-type"), "application/gzip");
  assert.equal(g.headers.get("cache-control"), "no-store");
  assert.deepEqual([...new Uint8Array(await g.arrayBuffer())], [...gz]);

  const h = await worker.fetch(new Request(body.url, { method: "HEAD" }), env);
  assert.equal(h.status, 200);
  assert.equal(h.headers.get("content-length"), String(gz.length));
});

test("sem auth, sem binding, corpo vazio e token ruim", async () => {
  const { env, post } = cenario();
  assert.equal((await post(new Uint8Array([1]))).status, 401);
  assert.equal((await post(new Uint8Array([1]), { authorization: "Bearer errado" })).status, 401);
  assert.equal((await post(new Uint8Array(0), { authorization: "Bearer tok-teste" })).status, 400);

  const semKv = { DIAG_TOKEN: "tok-teste" };
  const r = await worker.fetch(new Request("https://w.test/v1/loglink", {
    method: "POST", headers: { "x-nuvio-auth": "diagnostico", authorization: "Bearer tok-teste" },
    body: new Uint8Array([1, 2]),
  }), semKv);
  assert.equal(r.status, 503);

  // GET nunca pede auth (o link viaja por fora) mas o token nao e adivinhavel.
  assert.equal((await worker.fetch(new Request("https://w.test/v1/loglink/qualquer-coisa"), env)).status, 404);
  assert.equal((await worker.fetch(new Request("https://w.test/v1/loglink/" + "0".repeat(32)), env)).status, 404);
  assert.equal((await worker.fetch(new Request("https://w.test/v1/loglink/" + "Z".repeat(32)), env)).status, 404);

  const grande = await worker.fetch(new Request("https://w.test/v1/loglink", {
    method: "POST", headers: { "x-nuvio-auth": "diagnostico", authorization: "Bearer tok-teste" },
    body: new Uint8Array(2 * 1024 * 1024 + 1),
  }), env);
  assert.equal(grande.status, 413);
});

test("outros metodos e rotas vizinhas nao vazam para o loglink", async () => {
  const { env } = cenario();
  const d = await worker.fetch(new Request("https://w.test/v1/loglink/" + "0".repeat(32), { method: "DELETE" }), env);
  assert.notEqual(d.status, 200); // DELETE nao e rota do loglink
});
