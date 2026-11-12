// Loglink (Matis94 adaptado): upload da copia mascarada do registro -> link de
// 72 h. Worker inteiro com KV e o contador `limite` do D1 em memoria. Valores
// de segredo aqui sao sinteticos.
//   node --test servidor/recomendacoes/teste-loglink.mjs
import assert from "node:assert/strict";
import test from "node:test";
import worker from "./src/index.js";

const UP = "sintetico-envio", DIAG = "sintetico-diag";

function cenario(extra = {}) {
  const kv = new Map(), lim = new Map();
  const DB = {
    prepare: (sql) => ({
      bind: (chave, j, expira) => ({
        first: async () => {
          if (!sql.startsWith("INSERT INTO limite")) throw new Error("sql inesperado: " + sql);
          const v = lim.get(chave);
          const n = v && v.j === j ? v.n + 1 : 1;
          lim.set(chave, { j, n, expira });
          return { n };
        },
      }),
    }),
  };
  const env = {
    LOGLINK_TOKEN: UP, DIAG_TOKEN: DIAG, DB,
    LOGS: {
      put: async (k, v, opts) => kv.set(k, { value: v, metadata: opts.metadata, ttl: opts.expirationTtl }),
      getWithMetadata: async (k) => kv.get(k) ?? { value: null, metadata: null },
    },
    ...extra,
  };
  const post = (corpo, hdrs = {}, via = "loglink") =>
    worker.fetch(new Request("https://w.test/v1/loglink", {
      method: "POST",
      headers: { "x-nuvio-auth": via, "cf-connecting-ip": "203.0.113.9", ...hdrs },
      body: corpo,
    }), env);
  return { kv, lim, env, post };
}

test("post com o token de envio grava gzip no KV e devolve link que baixa", async () => {
  const { kv, env, post } = cenario();
  const gz = new Uint8Array([0x1f, 0x8b, 0x08, 1, 2, 3, 0x00]);
  const r = await post(gz, { authorization: "Bearer " + UP });
  assert.equal(r.status, 200);
  const body = await r.json();
  assert.equal(body.ok, 1);
  assert.equal(body.expira_s, 259200);
  assert.match(body.url, /^https:\/\/w\.test\/v1\/loglink\/[0-9a-f]{32}$/);
  const id = body.url.split("/").pop();
  assert.deepEqual([...new Uint8Array(kv.get("log:" + id).value)], [...gz]);
  assert.equal(kv.get("log:" + id).ttl, 259200);

  const g = await worker.fetch(new Request(body.url), env);
  assert.equal(g.status, 200);
  assert.equal(g.headers.get("content-type"), "application/gzip");
  assert.equal(g.headers.get("cache-control"), "no-store");
  assert.deepEqual([...new Uint8Array(await g.arrayBuffer())], [...gz]);
  const h = await worker.fetch(new Request(body.url, { method: "HEAD" }), env);
  assert.equal(h.status, 200);
  assert.equal(h.headers.get("content-length"), String(gz.length));
});

test("expiracao de 72 h e dura no GET, mesmo com o KV ainda devolvendo bytes", async () => {
  const { kv, env, post } = cenario();
  const body = await (await post(new Uint8Array([1, 2]), { authorization: "Bearer " + UP })).json();
  const id = body.url.split("/").pop();
  const exp = kv.get("log:" + id).metadata.expires;
  assert.ok(Math.abs(exp - (Date.now() + 72 * 3600 * 1000)) < 5000);
  kv.get("log:" + id).metadata.expires = Date.now() - 1;
  assert.equal((await worker.fetch(new Request(body.url), env)).status, 404);
  assert.equal((await worker.fetch(new Request(body.url, { method: "HEAD" }), env)).status, 404);
  delete kv.get("log:" + id).metadata.expires;
  assert.equal((await worker.fetch(new Request(body.url), env)).status, 404);
});

test("escopo: DIAG_TOKEN nao cria link e o token de envio nao grava recibo", async () => {
  const { post, env, kv } = cenario();
  assert.equal((await post(new Uint8Array([1]), { authorization: "Bearer " + DIAG })).status, 401);
  assert.equal((await post(new Uint8Array([1]), { authorization: "Bearer " + DIAG }, "diagnostico")).status, 401);
  assert.equal((await post(new Uint8Array([1]), { authorization: "Bearer " + UP }, "diagnostico")).status, 401);
  assert.equal((await post(new Uint8Array([1]))).status, 401);
  assert.equal((await post(new Uint8Array([1]), { authorization: "Bearer errado" })).status, 401);
  assert.equal(kv.size, 0);
  // /v1/registro (recibo) continua so com DIAG_TOKEN: o token de envio e 401.
  const reg = await worker.fetch(new Request("https://w.test/v1/registro", {
    method: "POST", headers: { "x-nuvio-auth": "diagnostico", authorization: "Bearer " + UP },
    body: JSON.stringify({ tv: "x", texto: "y" }),
  }), env);
  assert.equal(reg.status, 401);
});

test("configuracao errada nunca alarga: segredos iguais ou ausentes dao 503", async () => {
  const igual = cenario({ LOGLINK_TOKEN: DIAG });
  assert.equal((await igual.post(new Uint8Array([1]), { authorization: "Bearer " + DIAG })).status, 503);
  const sem = cenario({ LOGLINK_TOKEN: undefined });
  assert.equal((await sem.post(new Uint8Array([1]), { authorization: "Bearer " + DIAG })).status, 503);
  const semKv = cenario({ LOGS: undefined });
  assert.equal((await semKv.post(new Uint8Array([1]), { authorization: "Bearer " + UP })).status, 503);
  const semDb = cenario({ DB: undefined });
  assert.equal((await semDb.post(new Uint8Array([1]), { authorization: "Bearer " + UP })).status, 503);
});

test("corpo vazio, grande, teto por IP e teto diario", async () => {
  const { post, lim } = cenario();
  const a = { authorization: "Bearer " + UP };
  assert.equal((await post(new Uint8Array(0), a)).status, 400);
  assert.equal((await post(new Uint8Array(2 * 1024 * 1024 + 1), a)).status, 413);
  lim.clear();
  for (let i = 0; i < 6; i++) assert.equal((await post(new Uint8Array([i + 1]), a)).status, 200);
  assert.equal((await post(new Uint8Array([9]), a)).status, 429);
  assert.equal((await post(new Uint8Array([9]), { ...a, "cf-connecting-ip": "198.51.100.1" })).status, 200);
  const dia = [...lim.entries()].find(([k]) => k === "loglink:dia")[1];
  dia.n = 100;
  assert.equal((await post(new Uint8Array([9]), { ...a, "cf-connecting-ip": "198.51.100.2" })).status, 429);
});

test("token adivinhado e rotas vizinhas dao 404", async () => {
  const { env } = cenario();
  for (const t of ["qualquer-coisa", "0".repeat(32), "Z".repeat(32), "0".repeat(31), "0".repeat(33)])
    assert.equal((await worker.fetch(new Request("https://w.test/v1/loglink/" + t), env)).status, 404);
  const d = await worker.fetch(new Request("https://w.test/v1/loglink/" + "0".repeat(32), { method: "DELETE" }), env);
  assert.notEqual(d.status, 200);
});
