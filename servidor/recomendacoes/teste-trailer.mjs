// Teste das rotas /v1/trailer/* (src/trailer.js) em Node puro, sem wrangler e
// sem rede: o fetch do IMDb e um duble que registra o que recebeu.
//
//   node servidor/recomendacoes/teste-trailer.mjs
import assert from "node:assert/strict";
import { rotaTrailerImdb, rotaTrailerYoutube, consultaImdb } from "./src/trailer.js";

const ok = (nome, cond) => { assert.ok(cond, nome); console.log("ok", nome); };
const U = (s) => new URL("https://rec.exemplo.dev" + s);

// --- IMDb
let pedidos = [];
const imdb = (status, corpo) => async (alvo, opts) => {
  pedidos.push({ alvo, opts });
  return new Response(corpo, { status });
};
const RESP = '{"data":{"title":{"primaryVideos":{"edges":[{"node":{"playbackURLs":[{"url":"https://imdb-video.media-imdb.com/x.mp4?Expires=1","videoMimeType":"MP4","videoDefinition":"DEF_720p"}]}}]}}}}';

let r = await rotaTrailerImdb(U("/v1/trailer/imdb?id=tt0903747"), imdb(200, RESP));
ok("imdb: 200 com CORS aberto", r.status === 200 && r.headers.get("access-control-allow-origin") === "*");
ok("imdb: corpo cru repassado", (await r.text()) === RESP);
ok("imdb: pergunta vai com Referer imdb.com e Accept json",
  pedidos[0].opts.headers.referer === "https://www.imdb.com/" && pedidos[0].opts.headers.accept === "application/json");
ok("imdb: consulta e a do cliente (api.graphql.imdb.com, o id dentro)",
  pedidos[0].alvo === consultaImdb("tt0903747") && pedidos[0].alvo.startsWith("https://api.graphql.imdb.com/?query=") &&
  decodeURIComponent(pedidos[0].alvo).includes('title(id:"tt0903747")'));

pedidos = [];
for (const ruim of ["", "tt", "nm0000001", "tt123\"){x}", "tt0903747&a=b", "tt12345678901"]) {
  r = await rotaTrailerImdb(U("/v1/trailer/imdb?id=" + encodeURIComponent(ruim)), imdb(200, RESP));
  assert.equal(r.status, 400, "id " + ruim);
}
ok("imdb: id fora de tt+digitos e recusado sem pedir nada", pedidos.length === 0);

r = await rotaTrailerImdb(U("/v1/trailer/imdb?id=tt0903747"), imdb(403, "no"));
ok("imdb: 403 do IMDb vira 502 (nao repassa pagina de erro)", r.status === 502);
r = await rotaTrailerImdb(U("/v1/trailer/imdb?id=tt0903747"), async () => { throw new Error("x"); });
ok("imdb: rede caida vira 502", r.status === 502);

// cache da borda: segundo pedido nao sai
{
  const mapa = new Map();
  const cache = { match: async (k) => mapa.get(k.url)?.clone(), put: async (k, v) => { mapa.set(k.url, v); } };
  pedidos = [];
  await rotaTrailerImdb(U("/v1/trailer/imdb?id=tt0903747"), imdb(200, RESP), cache);
  r = await rotaTrailerImdb(U("/v1/trailer/imdb?id=tt0903747"), imdb(200, RESP), cache);
  ok("imdb: resposta em cache nao repete a pergunta", pedidos.length === 1 && (await r.text()) === RESP);
}

// --- YouTube
r = rotaTrailerYoutube(U("/v1/trailer/yt?id=dQw4w9WgXcQ&mute=1"));
const html = await r.text();
ok("yt: pagina html", r.status === 200 && /text\/html/.test(r.headers.get("content-type")));
ok("yt: embed com origin e widget_referrer desta origem",
  html.includes("https://www.youtube.com/embed/dQw4w9WgXcQ?") &&
  html.includes("origin=" + encodeURIComponent("https://rec.exemplo.dev")) &&
  html.includes("widget_referrer=" + encodeURIComponent("https://rec.exemplo.dev")));
ok("yt: mudo por padrao e enablejsapi", /mute=1/.test(html) && /enablejsapi=1/.test(html));
ok("yt: manda a origem como Referer", r.headers.get("referrer-policy") === "strict-origin-when-cross-origin" &&
  html.includes('referrerpolicy="strict-origin-when-cross-origin"'));
ok("yt: CSP so deixa o frame do YouTube", /frame-src https:\/\/www\.youtube\.com/.test(r.headers.get("content-security-policy")) &&
  /default-src 'none'/.test(r.headers.get("content-security-policy")));
ok("yt: repassa postMessage nos dois sentidos", html.includes("parent.postMessage(e.data,'*')") &&
  html.includes("p.contentWindow.postMessage(e.data,'https://www.youtube.com')"));
ok("yt: mute=0 desmuta", /mute=0/.test(await rotaTrailerYoutube(U("/v1/trailer/yt?id=dQw4w9WgXcQ&mute=0")).text()));
for (const ruim of ["", "abc", "<script>", "dQw4w9WgXcQ\"onload=1", "a".repeat(21)]) {
  const x = rotaTrailerYoutube(U("/v1/trailer/yt?id=" + encodeURIComponent(ruim)));
  assert.equal(x.status, 400, "id " + ruim);
}
ok("yt: id fora de [A-Za-z0-9_-]{6,20} e recusado (nada entra no html)", true);

console.log("teste-trailer: tudo ok");
