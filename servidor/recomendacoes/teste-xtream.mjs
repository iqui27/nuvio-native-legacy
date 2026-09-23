// Teste da rota /v1/xtream (src/xtream.js) em Node puro, sem wrangler e sem
// rede: o fetch do painel e um duble que registra o que recebeu. Cobre a
// allowlist de caminho/action, a remontagem da query, a recusa de destino
// privado (SSRF), o crivo do redirect, o tipo de imagem, o teto de tamanho e
// — o que mais importa — que usuario e senha NAO aparecem no log.
//
//   node servidor/recomendacoes/teste-xtream.mjs
import assert from "node:assert/strict";
import { rotaXtream, destinoPublico } from "./src/xtream.js";

const logs = [];
console.log = (...a) => logs.push(a.join(" "));
const USUARIO = "joaozinho", SENHA = "s3nh4Secreta";

let pedidos = [];
function painel(resp) {
  return async (u, op) => {
    pedidos.push({ u, op });
    return typeof resp === "function" ? resp(u) : resp;
  };
}
// O cliente manda POST com a url crua em text/plain (ver o topo de xtream.js).
const W = "https://w.test/v1/xtream";
const rota = (alvo, buscar) =>
  rotaXtream(new Request(W, { method: "POST", headers: { "content-type": "text/plain" }, body: alvo }), new URL(W), buscar);
const rotaGet = (alvo, buscar) => {
  const u = new URL(W + "?u=" + encodeURIComponent(alvo));
  return rotaXtream(new Request(u), u, buscar);
};
const json = (s, st = 200, cab = {}) =>
  new Response(s, { status: st, headers: { "content-type": "application/json", ...cab } });

let r, corpo;

// Lista: passa, com CORS, sem cache, e a query remontada so com o conhecido.
pedidos = [];
r = await rota(`http://painel.exemplo.tv:8080/player_api.php?username=${USUARIO}&password=${SENHA}&action=get_live_streams&extra=injetado`,
  painel(json("[]")));
assert.equal(r.status, 200);
assert.equal(await r.text(), "[]");
assert.equal(r.headers.get("access-control-allow-origin"), "*");
assert.equal(r.headers.get("cache-control"), "no-store");
assert.equal(pedidos.length, 1);
assert.ok(pedidos[0].u.startsWith("http://painel.exemplo.tv:8080/player_api.php?"));
assert.ok(!pedidos[0].u.includes("extra"), "parametro fora da allowlist nao chega ao painel");
assert.equal(pedidos[0].op.redirect, "manual");
console.error("ok  lista: repassa, CORS *, no-store, query remontada");

// Login/info (sem action) e as outras actions de lista.
for (const acao of ["", "&action=get_live_categories", "&action=get_vod_streams", "&action=get_series_info&series_id=3", "&action=get_short_epg&stream_id=9&limit=4"]) {
  r = await rota(`http://p.exemplo.tv/player_api.php?username=a&password=b${acao}`, painel(json("{}")));
  assert.equal(r.status, 200, acao);
}
console.error("ok  lista: actions da allowlist");

// Action fora da allowlist e caminho de video: recusa sem buscar.
pedidos = [];
for (const alvo of [
  "http://p.exemplo.tv/player_api.php?username=a&password=b&action=create_line",
  "http://p.exemplo.tv/panel_api.php?username=a&password=b",
  "http://p.exemplo.tv/xmltv.php?username=a&password=b",
  "http://p.exemplo.tv/get.php?username=a&password=b&type=m3u",
  "http://p.exemplo.tv/live/a/b/1.m3u8",
  "http://p.exemplo.tv/movie/a/b/1.mp4",
  "http://p.exemplo.tv/a/b/1.ts",
]) {
  r = await rota(alvo, painel(json("{}")));
  assert.equal(r.status, 403, alvo);
}
assert.equal(pedidos.length, 0, "nada recusado chega a sair");
console.error("ok  allowlist: action/caminho fora dela e video recusados sem rede");

// SSRF: host privado, loopback, metadados, IPv6, formas numericas, intranet.
for (const h of ["127.0.0.1", "localhost", "10.0.0.5", "172.20.1.1", "192.168.1.20", "169.254.169.254",
                 "100.77.116.81", "0.0.0.0", "[::1]", "[fd00::1]", "2130706433", "0x7f.1", "zimaos",
                 "tv.local", "x.internal", "224.0.0.1"]) {
  assert.equal(destinoPublico(`http://${h}:8080/player_api.php`), null, h);
  r = await rota(`http://${h}:8080/player_api.php?username=a&password=b`, painel(json("{}")));
  assert.equal(r.status, 403, h);
}
assert.equal(destinoPublico("ftp://p.exemplo.tv/x"), null);
assert.equal(destinoPublico("http://u:s@p.exemplo.tv/x"), null);
assert.equal(pedidos.length, 0);
assert.ok(destinoPublico("http://172.32.0.1/player_api.php"));   // fora do 172.16/12
assert.ok(destinoPublico("https://8.8.8.8/player_api.php"));
console.error("ok  ssrf: privado, loopback, 169.254, IPv6, numerico e intranet recusados");

// Redirect: segue para destino publico no mesmo modo; recusa privado e video.
pedidos = [];
r = await rota("http://p.exemplo.tv/player_api.php?username=a&password=b",
  painel((u) => u.includes("p.exemplo.tv")
    ? new Response(null, { status: 302, headers: { location: "http://lb2.exemplo.tv:25461/player_api.php?username=a&password=b" } })
    : json("{\"ok\":1}")));
assert.equal(r.status, 200);
assert.equal(pedidos.length, 2);
r = await rota("http://p.exemplo.tv/player_api.php?username=a&password=b",
  painel(new Response(null, { status: 302, headers: { location: "http://192.168.0.1/player_api.php" } })));
assert.equal(r.status, 403);
r = await rota("http://p.exemplo.tv/player_api.php?username=a&password=b",
  painel(new Response(null, { status: 302, headers: { location: "http://cdn.exemplo.tv/live/a/b/1.m3u8" } })));
assert.equal(r.status, 403);
console.error("ok  redirect: publico segue, privado e video recusados");

// Status do painel passa como esta (credencial recusada, servidor fora).
r = await rota("http://p.exemplo.tv/player_api.php?username=a&password=b", painel(json("{\"user_info\":{\"auth\":0}}", 401)));
assert.equal(r.status, 401);
r = await rota("http://p.exemplo.tv/player_api.php?username=a&password=b", painel(json("x", 503)));
assert.equal(r.status, 503);
r = await rota("http://p.exemplo.tv/player_api.php?username=a&password=b", async () => { throw new TypeError("falhou http://p.exemplo.tv/?password=" + SENHA); });
assert.equal(r.status, 504);
console.error("ok  upstream: 4xx/5xx repassados, falha de rede vira 504");

// Imagem: so image/*, com teto.
const png = new Uint8Array([0x89, 0x50, 0x4e, 0x47, 1, 2, 3]);
r = await rota("http://24horas.exemplo.cc/logos/espn.png", painel(new Response(png, { headers: { "content-type": "image/png" } })));
assert.equal(r.status, 200);
assert.equal(r.headers.get("content-type"), "image/png");
assert.equal((await r.arrayBuffer()).byteLength, png.length);
r = await rota("http://24horas.exemplo.cc/index.html", painel(new Response("<html>", { headers: { "content-type": "text/html" } })));
assert.equal(r.status, 415);
r = await rota("http://24horas.exemplo.cc/logo.png", painel(new Response("x", { headers: { "content-type": "image/png", "content-length": String(3 * 1024 * 1024) } })));
assert.equal(r.status, 502);
// Sem content-length: o corte e no fluxo.
const grande = new Uint8Array(3 * 1024 * 1024);
r = await rota("http://24horas.exemplo.cc/logo.png", painel(new Response(grande, { headers: { "content-type": "image/png" } })));
await assert.rejects(r.arrayBuffer());
console.error("ok  imagem: so image/*, teto de 2 MB por cabecalho e por fluxo");

assert.equal((await rota("", painel(json("{}")))).status, 400);

// POST com JSON {"u"} tambem vale; corpo gigante e recusado.
r = await rotaXtream(new Request(W, { method: "POST", headers: { "content-type": "application/json" },
  body: JSON.stringify({ u: "http://p.exemplo.tv/player_api.php?username=a&password=b" }) }), new URL(W), painel(json("[]")));
assert.equal(r.status, 200);
r = await rota("http://p.exemplo.tv/" + "x".repeat(5000), painel(json("{}")));
assert.equal(r.status, 413);
console.error("ok  post: text/plain e JSON {u}; corpo > 4 KB recusado");

// GET: so imagem. Lista por GET = 405; credencial no u= = 403 sem NENHUM log.
pedidos = [];
r = await rotaGet("http://24horas.exemplo.cc/logos/espn.png", painel(new Response(png, { headers: { "content-type": "image/png" } })));
assert.equal(r.status, 200);
const antes = logs.length;
r = await rotaGet(`http://p.exemplo.tv/player_api.php?username=${USUARIO}&password=${SENHA}`, painel(json("[]")));
assert.equal(r.status, 403);
r = await rotaGet("http://p.exemplo.tv/logo.png?Password=x", painel(json("[]")));
assert.equal(r.status, 403);
assert.equal(logs.length, antes, "recusa de credencial no GET nao loga");
r = await rotaGet("http://p.exemplo.tv/player_api.php", painel(json("[]")));
assert.equal(r.status, 405);
assert.equal(pedidos.length, 1, "so a imagem saiu");
console.error("ok  get: imagem passa, lista 405, credencial na url 403 sem log");

// Nenhuma linha de log tem usuario, senha ou query.
assert.ok(logs.length > 10, "houve log");
for (const l of logs) {
  assert.ok(!l.includes(USUARIO) && !l.includes(SENHA) && !l.includes("password") && !l.includes("?"), l);
}
console.error("ok  log: " + logs.length + " linhas, nenhuma com credencial ou query (ex.: " + logs[0] + ")");
