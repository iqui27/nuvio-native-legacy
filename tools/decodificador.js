// DECODIFICADOR DE IMAGEM FORA DO FIO PRINCIPAL (alvo Tizen).
//
// Ate a 1.3.2 o navegador decodificava a imagem no FIO PRINCIPAL: o
// createImageBitmap ate corria fora dele, mas o drawImage reduzido e o
// getImageData que trazem os pixels para o heap corriam no `then`, entre um
// quadro e outro do app. No AU7000 (#72) isso apareceu como `swap=1004` ms no
// [quadro] e FPS=2 enquanto uma fileira carregava: cada arte custava ate 1 s
// de fio principal parado, e uma fileira tem seis.
//
// Este Worker faz tudo isso aqui: recebe o pedido, le os bytes comprimidos
// DIRETO da memoria compartilhada do WASM, decodifica e reduz num
// OffscreenCanvas, pede ao fio de decode do C um bloco do tamanho certo e
// escreve os pixels nele. O fio principal so repassa o pedido (um postMessage)
// e nao toca em pixel nenhum.
//
// PROTOCOLO, em job[0] (8 int32 na memoria compartilhada, ver src/webp.c):
//   0  pedido em aberto
//   2  worker decodificou: job[1]=w job[2]=h job[4]=ow job[5]=oh; precisa de
//      w*h*4 bytes — e so o C sabe fazer malloc
//   3  C alocou: job[3]=ptr (0 = sem memoria)
//   1  terminado: job[3]=ptr com os pixels, ou 0 quando falhou
// Cada passo termina em Atomics.notify; os dois lados esperam com prazo.
//
// So ES5: o Chromium da TV (76) nao passa pelo esbuild aqui.
'use strict';
var HEAP32 = null, HEAPU8 = null;

function fim(pJob, estado) {
  Atomics.store(HEAP32, pJob, estado);
  Atomics.notify(HEAP32, pJob);
}

function falhou(pJob) {
  HEAP32[pJob + 1] = 0; HEAP32[pJob + 2] = 0; HEAP32[pJob + 3] = 0;
  fim(pJob, 1);
}

function decodificar(m) {
  var pJob = m.job >> 2;
  var largMax = m.largMax;
  // `slice`: copia para um ArrayBuffer comum. O Blob nao aceita vista sobre
  // SharedArrayBuffer, e o C libera `dados` assim que o pedido termina.
  var bytes = HEAPU8.slice(m.dados, m.dados + m.n);
  createImageBitmap(new Blob([bytes], { type: m.mime })).then(function (bmp) {
    var ow = bmp.width, oh = bmp.height, w = ow, h = oh;
    if (largMax > 0 && ow > largMax) {
      w = largMax;
      h = Math.max(1, Math.round(oh * largMax / ow));
    }
    if (!(w > 0 && h > 0)) { if (bmp.close) bmp.close(); falhou(pJob); return; }
    var cv = new OffscreenCanvas(w, h);
    var cx = cv.getContext('2d');
    cx.imageSmoothingEnabled = true;
    if ('imageSmoothingQuality' in cx) cx.imageSmoothingQuality = 'high';
    cx.drawImage(bmp, 0, 0, w, h);
    if (bmp.close) bmp.close();
    var d = cx.getImageData(0, 0, w, h).data;
    // Pede o bloco ao C e espera por ele. O malloc leva microssegundos; o
    // prazo so existe para este worker nao ficar preso se o fio de decode
    // desistiu (ele desiste em 8 s, ver src/webp.c).
    HEAP32[pJob + 1] = w; HEAP32[pJob + 2] = h;
    HEAP32[pJob + 4] = ow; HEAP32[pJob + 5] = oh;
    fim(pJob, 2);
    var esperou = 0;
    while (Atomics.load(HEAP32, pJob) === 2) {
      if (Atomics.wait(HEAP32, pJob, 2, 250) === 'timed-out') {
        esperou += 250;
        if (esperou >= 8000) return;   // o C ja foi embora; nada a escrever
      }
    }
    var ptr = HEAP32[pJob + 3];
    if (ptr) HEAPU8.set(d, ptr);
    fim(pJob, 1);
  }).catch(function () { falhou(pJob); });
}

self.onmessage = function (ev) {
  var m = ev.data;
  if (m.memoria) {
    HEAP32 = new Int32Array(m.memoria);
    HEAPU8 = new Uint8Array(m.memoria);
    return;
  }
  if (!HEAP32) { return; }
  try { decodificar(m); } catch (e) { falhou(m.job >> 2); }
};
