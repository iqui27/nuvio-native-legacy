// Focused route tests against real SQLite, without Wrangler, network or a
// shared D1 database. Requires Node 22+; run:
//   node --test servidor/recomendacoes/teste-social-semantica.mjs
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { DatabaseSync } from "node:sqlite";
import test from "node:test";
import { rotaAmigo, rotaEvento, rotaFeed, rotaRecResposta } from "./src/social.js";

const NOW = 1791054000;
const ME = "nuvio:reader", FRIEND = "nuvio:friend";
const h = {
  agora: () => NOW,
  limpar: (s, max) => String(s ?? "").slice(0, max),
  json: (body, status = 200) => new Response(JSON.stringify(body), { status }),
  erro: (message, status) => new Response(message, { status }),
};

function fixture(t) {
  const sqlite = new DatabaseSync(":memory:");
  t.after(() => sqlite.close());
  for (const name of ["schema.sql", "migracao-005-amigos.sql", "migracao-006-social.sql",
                     "migracao-007-resposta.sql"])
    sqlite.exec(readFileSync(new URL(name, import.meta.url), "utf8"));
  sqlite.prepare("INSERT INTO pessoa (id, nome, criado, visto, alcance) VALUES (?, ?, ?, ?, 1)")
    .run(ME, "Reader", NOW, NOW);
  sqlite.prepare("INSERT INTO pessoa (id, nome, criado, visto, alcance) VALUES (?, ?, ?, ?, 1)")
    .run(FRIEND, "Friend", NOW, NOW);
  sqlite.prepare("INSERT INTO contato (a, b, criado, via) VALUES (?, ?, ?, 'codigo')")
    .run(ME, FRIEND, NOW - 86400);

  // D1's prepared-statement API, executing the production SQL in SQLite.
  const DB = {
    prepare(sql) {
      const statement = sqlite.prepare(sql);
      return { bind(...values) {
        return {
          first: async () => statement.get(...values) || null,
          all: async () => ({ results: statement.all(...values) }),
          run: async () => statement.run(...values),
        };
      } };
    },
    batch: async (commands) => Promise.all(commands.map((command) => command.run())),
  };
  const env = { DB };
  const friend = async () => {
    const response = await rotaAmigo(env, { id: ME }, new URL(
      `https://example.test/v1/amigo?id=${FRIEND}`), h, async () => null);
    assert.equal(response.status, 200);
    return response.json();
  };
  const event = async (body) => {
    const response = await rotaEvento(env, { id: FRIEND }, body, h, async () => true);
    assert.equal(response.status, 200);
    return response.json();
  };
  const reply = async (body, who = FRIEND) => {
    const clean = (x) => String(x || "").toLowerCase().replace(/[^a-z0-9 ]+/g, " ").replace(/\s+/g, " ").trim().slice(0, 60);
    const response = await rotaRecResposta(env, { id: who }, body, h, async () => true, clean);
    return { status: response.status, body: response.status === 200 ? await response.json() : null };
  };
  const feed = async () => {
    const response = await rotaFeed(env, { id: ME }, new URL("https://example.test/v1/feed"),
      new Request("https://example.test/v1/feed"), h, async () => null);
    assert.equal(response.status, 200);
    return (await response.json()).itens;
  };
  return { sqlite, friend, event, reply, feed };
}

test("recommendation completion remains independent of reactions and opening", async (t) => {
  const { sqlite, friend, event } = fixture(t);
  const id = Number(sqlite.prepare(
    "INSERT INTO rec (de, para, criado, imdb, tipo, titulo) VALUES (?, ?, ?, 'tt1', 'movie', 'Movie')"
  ).run(ME, FRIEND, NOW).lastInsertRowid);
  const rec = async () => (await friend()).recs[0];
  assert.equal((await rec()).terminou, 0, "delivery is not completion");
  sqlite.prepare("UPDATE rec SET visto = 1, aberto = 1 WHERE id = ?").run(id);
  assert.equal((await rec()).terminou, 0, "opening is not completion");
  await event({ ev: "inicio", imdb: "tt1", rec: id });
  assert.equal((await rec()).terminou, 0, "starting is not completion");
  await event({ ev: "reacao", imdb: "tt1", rec: id, reacao: 1 });
  assert.equal((await rec()).estado, "reacao");
  assert.equal((await rec()).terminou, 0, "a positive reaction alone is not completion");
  await event({ ev: "fim", imdb: "tt1", rec: id, pct: 100 });
  const completed = await rec();
  assert.equal(completed.estado, "reacao", "the existing reaction display is preserved");
  assert.equal(completed.reacao, 1);
  assert.equal(completed.terminou, 1, "a reaction does not hide known completion");
});

test("Recently liked uses the latest reaction and retains the episode identity", async (t) => {
  const { friend, event } = fixture(t);
  const reaction = (imdb, reacao, extra = {}) => event({ ev: "reacao", imdb, reacao, ...extra });
  await reaction("tt1", 1);
  await reaction("tt1", -1);
  await reaction("tt2", 1);
  await reaction("tt2", 0);
  await reaction("tt3", 1);
  await reaction("tt3", -1, { midia: "series", temporada: 1, episodio: 1 });
  await reaction("tt4", 1, { midia: "series", temporada: 1, episodio: 1 });
  await reaction("tt4", 1, { midia: "series", temporada: 1, episodio: 2 });
  await reaction("tt4", -1, { midia: "series", temporada: 1, episodio: 1 });
  const liked = (await friend()).gostou;
  assert.ok(!liked.some((x) => x.imdb === "tt1"), "a newer dislike removes an older like");
  assert.ok(!liked.some((x) => x.imdb === "tt2"), "a newer neutral reaction removes an older like");
  assert.deepEqual(liked.map((x) => [x.imdb, x.midia, x.temporada, x.episodio]), [
    ["tt4", "series", 1, 2], ["tt3", "movie", 0, 0],
  ], "episodes and media kinds cannot overwrite one another's reactions");
});

test("reaction dedupe preserves a changed mind inside the retry window", async (t) => {
  const { sqlite, friend, event } = fixture(t);
  const body = { ev: "reacao", imdb: "tt1", midia: "series", temporada: 1, episodio: 3 };
  assert.equal((await event({ ...body, reacao: 1 })).evento, 1);
  assert.equal((await event({ ...body, reacao: 1 })).evento, 0, "the same latest reaction is a retry");
  assert.equal((await event({ ...body, reacao: -1 })).evento, 1);
  assert.equal((await event({ ...body, reacao: 1 })).evento, 1, "liking again is a new choice");
  assert.equal((await event({ ...body, reacao: 1 })).evento, 0);
  assert.equal(sqlite.prepare("SELECT COUNT(*) AS n FROM evento").get().n, 3);
  assert.equal((await friend()).gostou.length, 1);
  assert.equal((await event({ ...body, midia: "movie", reacao: 1 })).evento, 1,
    "a different media kind is not a retry of the episode");
});

test("private friend activity stays hidden after the reaction query changes", async (t) => {
  const { sqlite, friend, event } = fixture(t);
  await event({ ev: "reacao", imdb: "tt1", reacao: 1 });
  sqlite.prepare("UPDATE pessoa SET alcance = 0 WHERE id = ?").run(FRIEND);
  const result = await friend();
  assert.equal(result.compartilha, 0);
  assert.deepEqual(result.gostou, []);
  assert.equal(result.gosto, null);
});

test("a direct reply marks the recommendation watched even with activity sharing off", async (t) => {
  const { sqlite, friend, reply } = fixture(t);
  const id = Number(sqlite.prepare(
    "INSERT INTO rec (de, para, criado, imdb, tipo, titulo) VALUES (?, ?, ?, 'tt9', 'movie', 'Movie')"
  ).run(ME, FRIEND, NOW).lastInsertRowid);
  sqlite.prepare("UPDATE pessoa SET alcance = 0 WHERE id = ?").run(FRIEND);
  assert.equal((await reply({ id })).body.n, 1, "watched without reaction or message");
  let rec = (await friend()).recs[0];
  assert.equal(rec.terminou, 1);
  assert.equal(rec.reacao, null, "no reaction was given");
  assert.equal(rec.respondido, NOW);
  assert.equal((await reply({ id, reacao: 1, texto: "Valeu, AMEI!!" })).body.n, 1);
  rec = (await friend()).recs[0];
  assert.equal(rec.reacao, 1);
  assert.equal(rec.resposta, "valeu amei");
  await reply({ id, texto: "" });
  rec = (await friend()).recs[0];
  assert.equal(rec.reacao, 1, "a later reply without reaction keeps the reaction");
  assert.equal(rec.resposta, "valeu amei", "an empty message keeps the message");
  assert.equal((await reply({ id, reacao: -1 }, ME)).body.n, 0, "the sender cannot answer their own rec");
  assert.equal((await reply({ id: 0 })).status, 400);
});

test("GET /v1/rec returns watched/reply state of every rec, old ids included, and the ETag follows it", async (t) => {
  const { sqlite, reply } = fixture(t);
  const { rotaReceber } = await import("./src/index.js");
  const ins = (de, para, imdb) => Number(sqlite.prepare(
    "INSERT INTO rec (de, para, criado, imdb, tipo, titulo) VALUES (?, ?, ?, ?, 'movie', 'Movie')"
  ).run(de, para, NOW, imdb).lastInsertRowid);
  const old = ins(ME, FRIEND, "tt1"), fresh = ins(ME, FRIEND, "tt2"), mine = ins(FRIEND, ME, "tt3");
  const DB = {
    prepare(sql) {
      const st = sqlite.prepare(sql);
      return { bind: (...v) => ({ all: async () => ({ results: st.all(...v) }) }) };
    },
  };
  const get = (desde, etag) => rotaReceber({ DB }, { id: FRIEND },
    new URL(`https://example.test/v1/rec?desde=${desde}`),
    new Request("https://example.test/v1/rec", { headers: etag ? { "if-none-match": etag } : {} }));
  let res = await get(0);
  assert.equal(res.status, 200);
  const first = await res.json();
  const etag0 = res.headers.get("etag");
  assert.deepEqual(first.respostas, [], "nothing answered yet");
  assert.ok(first.itens.every((x) => x.terminou === 0 && x.respondido === 0 && x.reacao === null));
  assert.equal((await get(0, etag0)).status, 304, "unchanged state is a 304");

  // answered on ANOTHER device: the old id is behind the client's cursor
  assert.equal((await reply({ id: old, reacao: -1, texto: "Nao curti" })).body.n, 1);
  res = await get(fresh);                               // cursor already past `old`
  assert.equal(res.status, 200, "the reply invalidates the ETag even with no new rec");
  const body = await res.json();
  assert.equal(body.itens.length, 0);
  assert.deepEqual(body.respostas, [
    { id: old, terminou: 1, reacao: -1, resposta: "nao curti", respondido: NOW },
  ]);
  assert.ok(!body.respostas.some((x) => x.id === mine), "only recs sent TO the caller");
  // the same fields ride on the items themselves
  const all = await (await get(0)).json();
  const item = all.itens.find((x) => x.id === old);
  assert.equal(item.terminou, 1);
  assert.equal(item.resposta, "nao curti");
  assert.equal(item.reacao, -1);
  // a later change moves the ETag again
  const etag1 = res.headers.get("etag");
  await reply({ id: fresh });
  assert.notEqual((await get(fresh)).headers.get("etag"), etag1);
});

test("the feed shows only meaningful events, one line per person and title", async (t) => {
  const { sqlite, event, feed } = fixture(t);
  const raw = (ev, imdb, midia, temporada, episodio, criado, reacao = 0) => sqlite.prepare(
    "INSERT INTO evento (pessoa, ev, imdb, midia, titulo, poster, temporada, episodio, pct, reacao, rec, criado) " +
    "VALUES (?, ?, ?, ?, 'T', '', ?, ?, 0, ?, 0, ?)").run(FRIEND, ev, imdb, midia, temporada, episodio, reacao, criado);
  // legacy noise already stored: exits, saves, a started movie
  raw("abandono", "tt10", "movie", 0, 0, NOW - 900);
  raw("salvo", "tt11", "movie", 0, 0, NOW - 800);
  raw("inicio", "tt12", "movie", 0, 0, NOW - 700);
  // a series: started, three episodes finished, the last one liked
  raw("inicio", "tt20", "series", 1, 1, NOW - 600);
  raw("fim", "tt20", "series", 1, 1, NOW - 500);
  raw("inicio", "tt20", "series", 1, 2, NOW - 450);
  raw("fim", "tt20", "series", 1, 2, NOW - 400);
  raw("fim", "tt20", "series", 1, 3, NOW - 300);
  raw("reacao", "tt20", "series", 1, 3, NOW - 290, 1);
  // a brand-new series, only started
  raw("inicio", "tt30", "series", 2, 1, NOW - 200);
  // a movie: disliked, changed mind to "meh", no completion seen
  raw("reacao", "tt40", "movie", 0, 0, NOW - 100, -1);
  raw("reacao", "tt40", "movie", 0, 0, NOW - 90, 0);
  const itens = await feed();
  assert.deepEqual(itens.map((x) => [x.imdb, x.ev]), [
    ["tt40", "reacao"], ["tt30", "inicio"], ["tt20", "fim"],
  ], "no exits, saves or started movies; one line per title, newest first");
  const serie = itens.find((x) => x.imdb === "tt20");
  assert.equal(serie.eps, 3, "three distinct episodes finished");
  assert.equal(serie.episodio, 3, "the newest finished episode");
  assert.equal(serie.reac, 1, "the reaction rides on the finished line");
  assert.equal(itens.find((x) => x.imdb === "tt40").reacao, 0, "the latest reaction wins");
  assert.ok(!("eps" in itens.find((x) => x.imdb === "tt30")));
});

test("leaving the player closes 'watching now' and is never stored as an event", async (t) => {
  const { sqlite, event } = fixture(t);
  const agora = () => sqlite.prepare("SELECT COUNT(*) AS n FROM agora").get().n;
  await event({ ev: "inicio", imdb: "tt1", midia: "series", temporada: 1, episodio: 1 });
  assert.equal(agora(), 1);
  await event({ ev: "progresso", imdb: "tt1", midia: "series", temporada: 1, episodio: 1, pct: 10, seg: 60, parou: 1 });
  assert.equal(agora(), 0, "the new client's stop signal");
  await event({ ev: "inicio", imdb: "tt2", midia: "movie" });
  await event({ ev: "abandono", imdb: "tt2", midia: "movie", pct: 5, seg: 30 });
  assert.equal(agora(), 0, "an old client's abandono still closes it");
  assert.equal(sqlite.prepare("SELECT COUNT(*) AS n FROM evento WHERE ev IN ('abandono', 'progresso')").get().n, 0);
  assert.equal(sqlite.prepare("SELECT seg FROM agregado").get().seg, 90, "the watched time still counts");
});
