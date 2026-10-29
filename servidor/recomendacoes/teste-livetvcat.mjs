// Jev HTTP and KV doubles; real SQLite for the shared limiter and cleanup.
import assert from "node:assert/strict";
import { DatabaseSync } from "node:sqlite";
import test from "node:test";
import worker from "./src/index.js";
import { limitar, limpezaAmigos } from "./src/amigos.js";
import { rotaLiveTvCategorias, nomeBase, NOMES_MAX, CONFIANCA_MIN, CACHE_SEGUNDOS, SECOES } from "./src/livetvcat.js";

const request = (nomes) => new Request("https://x/livetv/categorias", {
  method: "POST", headers: { "cf-connecting-ip": "192.0.2.1" }, body: JSON.stringify({ nomes }),
});
const calls = [], writes = [];
const values = new Map();
const tab = { "canal foo": ["Esportes", 1.9], "canal bar": ["Filmes", 0.4], "canal baz": ["Outros", 2],
  "boundary": ["Notícias", 1.2], "invalid": ["constructor", 2], "missing": ["Filmes", undefined],
  "too high": ["Filmes", 3], "negative": ["Filmes", -1] };
const env = {
  TYPESAFE_KEY: "test-secret",
  CATS: { get: async (k) => values.get(k) ?? null,
    put: async (k, v, opts) => { values.set(k, v); writes.push(opts); } },
};
const originalFetch = globalThis.fetch;
const jev = async (url, options) => {
  assert.equal(url, "https://api.typesafe.ai/v1/systemone");
  assert.equal(options.method, "POST");
  assert.equal(options.headers.authorization, "Bearer test-secret");
  assert.equal(options.headers["content-type"], "application/json");
  const input = JSON.parse(options.body);
  calls.push(input);
  assert.equal(input.model, "jev-latest");
  assert.ok(Object.keys(input.questions).length <= 64);
  const names = JSON.parse(input.state), answers = {};
  for (const [key, name] of Object.entries(names)) {
    const choice = input.questions[key], score = input.questions["confianca" + key];
    assert.equal(choice.type, "choice");
    assert.deepEqual(choice.criteria, SECOES);
    assert.ok(choice.instructions.includes(key));
    assert.equal(score.type, "score");
    assert.deepEqual(score.criteria, ["A guess", "Probable", "Clear, a well-known channel"]);
    assert.ok(score.instructions.includes(key));
    const [category, confidence] = tab[name] ?? ["Outros", 2];
    answers[key] = { choice: category };
    answers["confianca" + key] = { score: confidence };
  }
  return Response.json({ answers });
};
globalThis.fetch = jev;

await test("P1: requested Jev HTTP contract works without an AI binding", async () => {
  assert.equal(nomeBase("[FHD] Canal-Foo 4K"), "canal foo");
  assert.equal(nomeBase("HDMI Canal"), "hdmi canal");
  const r = await worker.fetch(request(["Canal Foo HD", "Canal Bar", "Canal Baz", "canal foo", "boundary", "invalid", "missing", "too high", "negative"]),
    { ...env, DB: { prepare: () => ({ bind: () => ({ first: async () => ({ n: 1 }) }) }) } });
  assert.equal(r.status, 200);
  const cats = (await r.json()).cats;
  assert.equal(calls.length, 1);
  assert.equal(Object.keys(JSON.parse(calls[0].state)).length, 8);
  assert.equal(CONFIANCA_MIN, 1.2);
  assert.equal(cats["canal foo"], "Esportes");
  assert.equal(cats["canal bar"], null);
  assert.equal(cats["canal baz"], null);
  assert.equal(cats.boundary, "Notícias");
  assert.equal(cats.invalid, null);
  for (const key of ["missing", "too high", "negative"]) assert.ok(!(key in cats));
  assert.equal(writes.length, 5);
  assert.ok(writes.every((o) => o.expirationTtl === CACHE_SEGUNDOS && CACHE_SEGUNDOS === 2592000));
});

await test("P2: real limiter rejects request 13, resets after 600 seconds, and cleanup removes expired IPs", async () => {
  const db = new DatabaseSync(":memory:");
  db.exec("CREATE TABLE limite (chave TEXT PRIMARY KEY, janela INTEGER, n INTEGER, expira INTEGER)");
  const DB = { prepare: (sql) => ({ bind: (...args) => ({
    first: async () => db.prepare(sql).get(...args),
    run: async () => db.prepare(sql).run(...args),
  }) }) };
  // Keep an AI double here solely so this regression reaches the old limiter.
  const e = { ...env, DB, AI: { run: async () => ({ answers: {} }) } };
  const now = 1800000000000;
  try {
    for (let i = 0; i < 12; i++) assert.equal((await rotaLiveTvCategorias(request([]), e, limitar, now)).status, 200);
    assert.equal((await rotaLiveTvCategorias(request([]), e, limitar, now)).status, 429);
    const row = db.prepare("SELECT * FROM limite").get();
    assert.equal(row.chave, "livetvcat:192.0.2.1");
    assert.equal(row.expira, now / 1000 + 600);
    assert.equal((await rotaLiveTvCategorias(request([]), e, limitar, now + 600000)).status, 200);
    assert.equal(db.prepare("SELECT n FROM limite").get().n, 1);
    await limpezaAmigos(e, now / 1000 + 1201).at(-1).run();
    assert.equal(db.prepare("SELECT count(*) AS n FROM limite").get().n, 0);
  } finally { db.close(); }
});

await test("cache, batching, unavailable upstream, validation and rate limits", async () => {
  const route = (names, e = env, l) => rotaLiveTvCategorias(request(names), e, l);
  calls.length = 0;
  let r = await route(["Canal Foo FHD", "Canal Bar", "Canal Baz"]);
  let cats = (await r.json()).cats;
  assert.equal(calls.length, 0);
  assert.equal(cats["canal foo"], "Esportes");
  assert.equal(cats["canal bar"], null);
  await route(["Canal Foo", "new channel"]);
  assert.equal(calls.length, 1);
  assert.equal(Object.values(JSON.parse(calls[0].state)).join(), "new channel");
  calls.length = 0;
  r = await route(Array.from({ length: 200 }, (_, i) => "Channel " + i));
  assert.equal(Object.keys((await r.json()).cats).length, 200);
  assert.equal(calls.length, 7);
  for (const fetcher of [async () => { throw new Error("offline"); }, async () => new Response("upstream failed", { status: 503 }), async () => Response.json({ answers: {} })]) {
    globalThis.fetch = fetcher;
    r = await route(["offline channel"]);
    assert.equal(r.status, 200);
    assert.ok(!("offline channel" in (await r.json()).cats));
    assert.ok(!values.has("c:offline channel"));
  }
  globalThis.fetch = jev;
  for (const bad of [Array(NOMES_MAX + 1).fill("a"), [1], ["x".repeat(201)], ["https://host/secret"], ["//host/path"], ["www.host.tv/path"], ["bad\nname"]])
    assert.equal((await route(bad)).status, 400);
  assert.equal((await rotaLiveTvCategorias(new Request("https://x", { method: "POST", body: "{" }), env)).status, 400);
  assert.equal((await route(["a"], { CATS: env.CATS })).status, 503);
  assert.equal((await route(["a"], { TYPESAFE_KEY: env.TYPESAFE_KEY })).status, 503);
  assert.equal((await route(["a"], env, async () => false)).status, 429);
});
globalThis.fetch = originalFetch;
