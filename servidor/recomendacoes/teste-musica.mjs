// Teste de /musica/v1 e /m/ (src/musica.js) sem rede: MusicBrainz e iTunes sao dobles.
//   node servidor/recomendacoes/teste-musica.mjs
import assert from "node:assert/strict";
import { rotaMusica, rotaPaginaMusica, escolher, CACHE_ACHOU, CACHE_VAZIO } from "./src/musica.js";

const ok = (nome, cond) => { assert.ok(cond, nome); console.log("ok", nome); };
const U = (s) => new URL("https://rec.exemplo.dev" + s);
const pedidos = [];
const J = (o) => new Response(JSON.stringify(o), { status: 200 });
const fonte = (grupos) => async (alvo, opts) => {
  pedidos.push({ alvo, opts });
  if (alvo.includes("v3-cinemeta.strem.io/meta/")) return J({ meta: { name: "Inception", year: "2010" } });
  if (alvo.includes("/release-group?")) return J({ "release-groups": grupos });
  if (alvo.includes("/release?")) return J({ releases: [{ media: [{ tracks: [
    { title: "Time", length: 275000, "artist-credit": [{ name: "Hans Zimmer" }] },
    { title: "Mind Heist", length: 192000, "artist-credit": [{ name: "Hans Zimmer" }] }] }] }] });
  if (alvo.includes("itunes.apple.com")) return J({ results: [{ collectionName: "Inception (Music from the Motion Picture)",
    artworkUrl100: "https://is1.mzstatic.com/a/100x100bb.jpg", collectionViewUrl: "https://music.apple.com/us/album/x" }] });
  throw new Error("alvo inesperado " + alvo);
};
const OST = { id: "g1", title: "Inception (Music from the Motion Picture)", score: 100, "first-release-date": "2010-07-13", "artist-credit": [{ name: "Hans Zimmer" }] };
const kv = () => { const m = new Map(), ttl = []; return { m, ttl, get: async (k) => m.get(k) ?? null, put: async (k, v, o) => { m.set(k, v); ttl.push(o.expirationTtl); } }; };

let env = { MUS: kv() };
let r = await rotaMusica(U("/musica/v1/tt1375666?t=Inception&y=2010"), env, fonte([OST]));
let d = await r.json();
ok("achou: faixas com titulo, artista e duracao", r.status === 200 && d.faixas.length === 2 && d.faixas[0].dur === 275 && d.faixas[1].artista === "Hans Zimmer");
ok("achou: capa 600 https e link da Apple", d.album.capa.includes("600x600bb") && d.album.apple.startsWith("https://music.apple.com/"));
ok("MusicBrainz leva User-Agent", pedidos.find((p) => p.alvo.includes("musicbrainz.org")).opts.headers["user-agent"].startsWith("NuvioLegacy"));
ok("cache 30 dias", env.MUS.ttl[0] === CACHE_ACHOU);
const n = pedidos.length;
await rotaMusica(U("/musica/v1/tt1375666?t=Inception&y=2010"), env, fonte([OST]));
ok("segunda consulta sai do KV", pedidos.length === n);

env = { MUS: kv() };
r = await rotaMusica(U("/musica/v1/tt1375666?t=Inception&y=2014"), env, fonte([OST]));
ok("caller year cannot poison cache", (await r.json()).faixas.length === 2 && env.MUS.ttl[0] === CACHE_ACHOU);

env = { MUS: kv() };
r = await rotaMusica(U("/musica/v1/tt1375666?t=Unrelated&y=1999"), env, fonte([OST]));
assert.equal((await r.json()).album?.titulo, OST.title, "caller title cannot poison cache");
r = await rotaMusica(U("/musica/v1/tt1375666?t=Inception&y=2010"), env, fonte([OST]));
assert.equal((await r.json()).album?.titulo, OST.title, "legitimate lookup survives hostile first request");
env = { MUS: kv() };
r = await rotaMusica(U("/musica/v1/tt1375666?t=Inception&y=2010"), env, fonte([]));
ok("canonical miss cached seven days", (await r.json()).vazio && env.MUS.ttl[0] === CACHE_VAZIO);
ok("escolher: score baixo recusado", escolher([{ ...OST, score: 80 }], "Inception", 2010, 0) === null);
ok("escolher: titulo diferente recusado", escolher([{ ...OST, title: "Interstellar OST" }], "Inception", 2010, 0) === null);
ok("escolher: temporada certa", escolher([{ ...OST, title: "Show: Season 2 (Score)", "first-release-date": "2020-01-01" }], "Show", 2019, 2) !== null
  && escolher([{ ...OST, title: "Show: Season 1", "first-release-date": "2019-01-01" }], "Show", 2019, 2) === null);

for (const ruim of ["/musica/v1/xx1?t=a&y=1", "/musica/v1/tt1375666", "/musica/v1/tt1375666?t=%00x", "/musica/v1/tt1/../x?t=a"]) {
  pedidos.length = 0;
  r = await rotaMusica(U(ruim), { MUS: kv() }, fonte([OST]));
  assert.equal(r.status, 400, ruim);
  assert.equal(pedidos.length, 0, ruim);
}
ok("pedido invalido: 400 sem pedir nada", true);

r = await rotaMusica(U("/musica/v1/tt1375666?t=Inception&y=2010"), { MUS: kv() }, async () => { throw new Error("x"); });
ok("fonte caida: 502 e nada no cache", r.status === 502);
r = await rotaMusica(U("/musica/v1/tt1375666?t=Inception&y=2010"), { MUS: kv() }, fonte([OST]), async () => false);
ok("limite estourado: 429", r.status === 429);

r = rotaPaginaMusica();
const html = await r.text();
ok("pagina: html, sem referrer, links Spotify/Apple/YouTube", r.headers.get("content-type").startsWith("text/html") &&
  r.headers.get("referrer-policy") === "no-referrer" && ["open.spotify.com/search", "music.apple.com/search", "music.youtube.com/search"].every((x) => html.includes(x)));
