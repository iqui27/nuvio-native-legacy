// Evaluate the Tizen glue (build/tizen/index.js) in an engine WITHOUT
// globalThis, the way a Tizen 5.5 TV (Chromium M69) would: once as the page
// and once as a pthread worker, which loads this same file (new Worker(_scriptName)).
// Usage: node tizen-globalthis-runner.cjs <index.js> <pagina|worker> [--moderno]
const fs = require('fs');
const vm = require('vm');

const [file, modo, moderno] = process.argv.slice(2);
if (moderno !== '--moderno' && typeof globalThis !== 'undefined') {
  console.log('BASE: este motor tem globalThis; use um sem ele (Node 10)');
  process.exit(2);
}

const URL_BASE = 'file:///opt/usr/apps/nuvio/res/wgt/';
const pedidos = [];
function Xhr() {}
Xhr.prototype.open = function (m, url) { pedidos.push(String(url)); };
Xhr.prototype.send = function () {};
Xhr.prototype.setRequestHeader = function () {};

const ctx = {
  console,
  setTimeout, clearTimeout, setInterval, clearInterval,
  TextDecoder: require('util').TextDecoder,
  navigator: { userAgent: 'Mozilla/5.0 (SMART-TV; LINUX; Tizen 5.5) Chrome/69.0.3497.106 TV Safari/537.36', hardwareConcurrency: 4 },
  location: { href: URL_BASE + 'index.html', pathname: '/opt/usr/apps/nuvio/res/wgt/index.html' },
  XMLHttpRequest: Xhr,
  fetch: url => { pedidos.push(String(url)); return new Promise(() => {}); },
  WebAssembly,
  Atomics,
  SharedArrayBuffer: typeof SharedArrayBuffer !== 'undefined' ? SharedArrayBuffer : undefined,
  performance: require('perf_hooks').performance,
  addEventListener() {},
  removeEventListener() {},
  postMessage() {},
  Worker: function () { this.postMessage = () => {}; },
};
ctx.self = ctx;
if (modo === 'pagina') {
  ctx.window = ctx;
  ctx.document = {
    currentScript: { src: URL_BASE + 'index.js' },
    getElementById: () => null,
    querySelector: () => null,
    addEventListener() {},
    createElement: () => ({ style: {}, getContext: () => null, addEventListener() {} }),
  };
  ctx.Module = { canvas: null, print() {}, printErr() {} };
} else if (modo === 'worker') {
  ctx.WorkerGlobalScope = function WorkerGlobalScope() {};
  ctx.name = 'em-pthread';
  ctx.location = { href: URL_BASE + 'index.js', pathname: '/opt/usr/apps/nuvio/res/wgt/index.js' };
  ctx.importScripts = () => {};
} else {
  console.log('uso: <index.js> <pagina|worker> [--moderno]');
  process.exit(2);
}

function falha(e) {
  const linha = ((e && e.stack) || '').split('\n').find(l => l.includes(' at ') && l.includes(file)) || '';
  const m = linha.match(/:(\d+):\d+\)?$/);
  console.log(`ERRO: ${e && e.name}: ${e && e.message} @linha ${m ? m[1] : '?'}`);
  process.exit(1);
}
process.on('unhandledRejection', falha);

try {
  vm.runInContext(fs.readFileSync(file, 'utf8'), vm.createContext(ctx), { filename: file });
} catch (e) {
  falha(e);
}

setTimeout(() => {
  if (modo === 'worker') {
    if (typeof ctx.onmessage !== 'function') {
      console.log('ERRO: worker avaliou mas nao instalou onmessage');
      process.exit(1);
    }
    console.log('OK worker: topo avaliado, onmessage instalado');
  } else {
    const alvo = pedidos.filter(u => /index\.(data|wasm)/.test(u));
    if (!alvo.length) {
      console.log(`ERRO: pagina nao pediu index.data/index.wasm (pedidos: ${pedidos.join(', ') || 'nenhum'})`);
      process.exit(1);
    }
    console.log(`OK pagina: topo avaliado, pediu ${alvo.join(', ')}`);
  }
  process.exit(0);
}, 300);
