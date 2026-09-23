// --pre-js do teste da fila do decodificador (tests/decodefila-tizen.sh).
//
// O Node nao tem createImageBitmap nem OffscreenCanvas, mas tem o que importa
// para a PONTE: SharedArrayBuffer, Atomics num worker_thread e pthreads do
// Emscripten de verdade. Este arquivo troca so o CODEC: o `Worker` pedido por
// src/webp.c com o nome 'decodificador.js' vira um worker_thread que roda o
// tools/decodificador.js REAL, com um createImageBitmap falso. A imagem falsa
// e um PNG de verdade ate o IHDR (webp.c le o tamanho dali) seguido de um
// registro "FAKE" com cor e atraso em ms. Cada pedido do teste tem uma cor
// unica: pixel trocado entre pedidos aparece como cor errada, e bloco nunca
// escrito aparece como a cor-isca que o teste deixa na memoria liberada.
//
// NV_SHIM_MORTO=1: o Worker "morre" (onerror) 30 ms depois de subir e nunca
// responde; o fio principal ganha o mesmo codec falso, e o teste exercita o
// reencaminhamento de src/webp.c para o caminho antigo.
(function () {
  var wt = require('node:worker_threads');
  if (!wt.isMainThread) return;           // pthreads do Emscripten: nada a fazer
  var path = require('node:path');
  var Original = globalThis.Worker;
  var raiz = process.env.NV_RAIZ || process.cwd();
  var morto = process.env.NV_SHIM_MORTO === '1';
  var codec = [
    "function Bmp(w, h, c) { this.width = w; this.height = h; this.c = c; }",
    "Bmp.prototype.close = function () {};",
    "globalThis.createImageBitmap = function (blob) {",
    "  return blob.arrayBuffer().then(function (ab) {",
    "    var b = new Uint8Array(ab), o = 24;",
    "    if (b.length < o + 16 || b[o] !== 70 || b[o + 1] !== 65 || b[o + 2] !== 75 || b[o + 3] !== 69) throw new Error('formato');",
    "    var w = b[o + 4] | (b[o + 5] << 8), h = b[o + 6] | (b[o + 7] << 8), atraso = b[o + 12] | (b[o + 13] << 8);",
    "    var c = [b[o + 8], b[o + 9], b[o + 10], b[o + 11]];",
    "    return new Promise(function (ok) { setTimeout(function () { ok(new Bmp(w, h, c)); }, atraso); });",
    "  });",
    "};",
    "function Tela(w, h) { this.width = w; this.height = h; this.c = [0, 0, 0, 0]; }",
    "Tela.prototype.getContext = function () { var cv = this; return {",
    "  drawImage: function (bmp) { cv.c = bmp.c; },",
    "  getImageData: function (x, y, w, h) { var d = new Uint8ClampedArray(w * h * 4);",
    "    for (var i = 0; i < d.length; i += 4) { d[i] = cv.c[0]; d[i + 1] = cv.c[1]; d[i + 2] = cv.c[2]; d[i + 3] = cv.c[3]; }",
    "    return { data: d }; } }; };",
    "globalThis.OffscreenCanvas = Tela;"
  ].join('\n');
  var boot = [
    "var wt = require('node:worker_threads');",
    "globalThis.self = globalThis;",
    "self.postMessage = function (m, t) { wt.parentPort.postMessage(m, t); };",
    codec,
    "wt.parentPort.on('message', function (d) { if (self.onmessage) self.onmessage({ data: d }); });",
    "require(" + JSON.stringify(path.join(raiz, 'tools/decodificador.js')) + ");"
  ].join('\n');
  function Falso() {
    var eu = this;
    if (morto) {
      setTimeout(function () { if (eu.onerror) eu.onerror({ message: 'teste: worker morto' }); }, 30);
      this.postMessage = function () {};
      return;
    }
    var w = new Original(boot, { eval: true });
    w.on('message', function (m) { if (eu.onmessage) eu.onmessage({ data: m }); });
    w.on('error', function (e) { if (eu.onerror) eu.onerror(e); });
    w.unref();
    this.postMessage = function (m, t) { w.postMessage(m, t); };
  }
  globalThis.Worker = function (url, opts) {
    if (url === 'decodificador.js') return new Falso();
    return new Original(url, opts);
  };
  // NV_SHIM_OCUPADO=<ms>: o FIO PRINCIPAL fica ocupado em blocos de <ms>, com
  // 20 ms livres entre um e outro — o papel das tarefas longas de 1 a 31 s dos
  // logs da Samsung 1.4.1. Pedido que depende do fio principal espera o bloco
  // inteiro; o canal direto (src/webp.c, sentinela) nao deve nem perceber.
  var ocupado = parseInt(process.env.NV_SHIM_OCUPADO || '0', 10);
  if (ocupado > 0) {
    var tOcup = setInterval(function () { var t0 = Date.now(); while (Date.now() - t0 < ocupado) {} }, 20);
    tOcup.unref();
  }
  // webp.c so escolhe o Worker quando o fio principal diz ter OffscreenCanvas.
  (0, eval)(codec);
  if (morto) globalThis.document = { createElement: function () { return new globalThis.OffscreenCanvas(0, 0); } };
})();
