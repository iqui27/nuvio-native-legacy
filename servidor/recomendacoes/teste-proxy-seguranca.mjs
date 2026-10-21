// /v1/proxy devolve conteudo de terceiros no origin do app hospedado: nada que o
// navegador renderize como pagina (HTML, SVG, XML, JS) pode sair com o tipo
// original, e toda resposta leva nosniff + CSP com sandbox. Node puro, sem rede:
// o fetch do destino e um duble.
//
//   node servidor/recomendacoes/teste-proxy-seguranca.mjs
import assert from "node:assert/strict";
import { rotaProxy } from "./src/proxy.js";

console.log = () => {};
let n = 0;
async function pedir(ct, corpo = "x", status = 200) {
  const headers = ct === null ? {} : { "content-type": ct };
  const buscar = async () => new Response(corpo, { status, headers });
  const req = new Request("https://w.test/v1/proxy?u=x", { headers: { "cf-connecting-ip": `10.0.${Math.floor(++n / 250)}.${n % 250}` } });
  const url = new URL("https://w.test/v1/proxy?u=" + encodeURIComponent("https://example.com/a.bin"));
  return rotaProxy(req, url, {}, buscar);
}
function seguro(r) {
  assert.equal(r.headers.get("x-content-type-options"), "nosniff");
  assert.match(r.headers.get("content-security-policy"), /default-src 'none'; sandbox/);
}

// Tipos que o navegador renderiza ou executa saem como texto puro.
for (const ct of ["text/html", "text/html; charset=utf-8", "image/svg+xml", "application/xhtml+xml",
                  "application/xml", "text/xml", "text/javascript", "text/css", "text/x-unknown"]) {
  const r = await pedir(ct, "<script>alert(1)</script>");
  assert.equal(r.status, 200, ct);
  assert.match(r.headers.get("content-type"), /^text\/plain/, `${ct} -> ${r.headers.get("content-type")}`);
  seguro(r);
}
// O que o app realmente consome continua com o tipo certo.
for (const ct of ["application/json", "application/json; charset=utf-8", "text/vtt", "text/plain",
                  "application/x-subrip", "application/vnd.apple.mpegurl", "image/png", "image/jpeg", "image/webp"]) {
  const r = await pedir(ct);
  assert.equal(r.headers.get("content-type"), ct, ct);
  seguro(r);
}
// Corpo de erro do destino (o tipo nao e checado acima de 300) tambem sai neutro.
{ const r = await pedir("text/html", "<h1>404</h1>", 404);
  assert.equal(r.status, 404);
  assert.match(r.headers.get("content-type"), /^text\/plain/);
  seguro(r); }
// Sem content-type: octet-stream, nunca "adivinhado".
{ const r = await pedir(null, "{}", 200);
  assert.equal(r.status, 200);
  seguro(r); }
console.log = console.error;
console.log("teste-proxy-seguranca: ok");
