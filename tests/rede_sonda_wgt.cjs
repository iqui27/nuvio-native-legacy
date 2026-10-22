// Executa o EM_JS da sonda com XHR falso; nao substitui teste na Samsung.
const fs = require('node:fs');
const assert = require('node:assert/strict');
const source = fs.readFileSync('src/rede.c', 'utf8');
const body = source.match(/EM_JS\(int, nv_url_sonda,[\s\S]*?, \{([\s\S]*?)\n\}\);/)[1];
let resposta, destino, header, leuCorpo = 0;
global.HEAP32 = new Int32Array(32);
global.UTF8ToString = x => x;
let mimeSaida;
global.stringToUTF8 = (x, p) => { if (p === 'MIME') mimeSaida = x; else destino = x; };
global.XMLHttpRequest = class {
  open(method, url, async) { assert.equal(method, 'GET'); assert.equal(async, false); }
  setRequestHeader(name, value) { header[name] = value; }
  send() { if (resposta.throw) throw Error('network'); this.status = resposta.status; this.responseURL = resposta.url; }
  getResponseHeader(n) { return (resposta.h || {})[n] ?? null; }
  get responseText() { leuCorpo++; throw Error('must not copy media body into WASM'); }
};
const sonda = new Function('url', 'cabs', 'dst', 'tam', 'status', 'mime', 'mimeTam', 'corpo', body);
function run(r, tamanho = 256) {
  resposta = r; destino = ''; header = {}; HEAP32[1] = 999;
  return sonda('https://media.example/a', 'Referer: https://addon.example/\nUser-Agent: test', 8, tamanho, 4, 'MIME', 64, 12);
}
assert.equal(run({status:200,url:'https://media.example/final.mp4'}), 1);
assert.equal(destino, 'https://media.example/final.mp4');
assert.equal(header.Range, 'bytes=0-63');
assert.equal(header.Referer, 'https://addon.example/');
assert.equal(HEAP32[1], 200);
for (const status of [0,403,404,500]) {
  assert.equal(run({status,url:'https://media.example/denied'}), 0);
  assert.equal(destino, ''); assert.equal(HEAP32[1], status);
}
assert.equal(run({throw:true}), 0); assert.equal(HEAP32[1], 0);
assert.equal(run({status:200,url:'https://media.example/' + 'x'.repeat(300)}), 0);
assert.equal(run({status:200,url:'é'.repeat(50)}, 100), 0);
assert.equal(run({status:206,url:'https://media.example/' + 'x'.repeat(3000)}, 4096), 1);
assert.equal(run({status:200,url:'https://media.example/p',h:{'Content-Type':'Text/HTML; charset=utf-8','Content-Length':'9'}}), 1);
assert.equal(mimeSaida, 'text/html'); assert.equal(HEAP32[3], 9);
assert.equal(run({status:200,url:'https://media.example/v',h:{'Content-Type':'video/mp4','Content-Length':'99999'}}), 1);
assert.equal(HEAP32[3], -1);
assert.equal(run({status:206,url:'https://media.example/v',h:{'Content-Length':'64'}}), 1);
assert.equal(HEAP32[3], -1);
assert.equal(leuCorpo, 0);
console.log('rede_sonda_wgt: status, Range/headers, URL bytes, transporte e nenhuma copia de corpo ok');
