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

// GIF ANIMADO (#84, 20/09/2026): os quadros tambem sao compostos AQUI. Ate a
// 1.3.7 cada quadro era um <img> decodificado no fio principal mais um
// drawImage num canvas de la; numa AU7000 isso sao ~15 decodes por segundo no
// mesmo fio que desenha, e o rawldon mediu 29-30 FPS com 20 janks so com o
// GIF na tela. Agora o fio principal manda os quadros fatiados uma vez
// (`gif`), pede quadro a quadro (`gifQuadro`) e recebe um ImageBitmap ja
// composto e reduzido ao tamanho do card — o unico trabalho que sobra la e
// o texImage2D do bitmap. A composicao segue a regra de descarte do GIF
// (2 = limpa a area do quadro, 3 = volta ao estado anterior).
//
// Os quadros sao pedidos EM ORDEM; num salto para tras (volta do laco, i==0)
// a tela logica e limpa e recomeca — e o que a <img> fazia sozinha.
var gifs = {};
function gifIniciar(m) {
  var g = {};
  g.w = m.w; g.h = m.h; g.outW = m.outW; g.outH = m.outH; g.meta = m.meta;
  g.frames = m.frames.map(function (b) { return new Blob([b], { type: 'image/gif' }); });
  g.comp = new OffscreenCanvas(m.w, m.h); g.cctx = g.comp.getContext('2d');
  g.out = new OffscreenCanvas(m.outW, m.outH); g.octx = g.out.getContext('2d');
  g.octx.imageSmoothingEnabled = true;
  if ('imageSmoothingQuality' in g.octx) g.octx.imageSmoothingQuality = 'high';
  g.salvo = null; g.posto = -1;
  gifs[m.id] = g;
}
function gifQuadro(m) {
  var g = gifs[m.id];
  var i = m.i;
  if (!g || i < 0 || i >= g.frames.length) return;
  createImageBitmap(g.frames[i]).then(function (bmp) {
    var gg = gifs[m.id];
    if (!gg) { if (bmp.close) bmp.close(); return; }
    if (i === 0 || i <= gg.posto) { gg.cctx.clearRect(0, 0, gg.w, gg.h); gg.salvo = null; }
    else if (gg.posto >= 0) {
      var a = gg.meta[gg.posto];
      if (a && a.descarte === 2) gg.cctx.clearRect(a.esq, a.topo, a.larg, a.alt);
      else if (a && a.descarte === 3 && gg.salvo) { try { gg.cctx.putImageData(gg.salvo, 0, 0); } catch (e) {} }
    }
    var mm = gg.meta[i];
    if (mm && mm.descarte === 3) { try { gg.salvo = gg.cctx.getImageData(0, 0, gg.w, gg.h); } catch (e) { gg.salvo = null; } }
    gg.cctx.drawImage(bmp, 0, 0);
    if (bmp.close) bmp.close();
    gg.posto = i;
    gg.octx.clearRect(0, 0, gg.outW, gg.outH);
    gg.octx.drawImage(gg.comp, 0, 0, gg.outW, gg.outH);
    var ib = gg.out.transferToImageBitmap();
    self.postMessage({ gifPronto: { id: m.id, i: i }, bitmap: ib }, [ib]);
  }).catch(function () { self.postMessage({ gifPronto: { id: m.id, i: i, falhou: 1 } }); });
}
function gifSoltar(m) { delete gifs[m.id]; }

self.onmessage = function (ev) {
  var m = ev.data;
  if (m.memoria) {
    HEAP32 = new Int32Array(m.memoria);
    HEAPU8 = new Uint8Array(m.memoria);
    return;
  }
  if (m.gif)       { try { gifIniciar(m.gif); } catch (e) { self.postMessage({ gifPronto: { id: m.gif.id, i: -1, falhou: 1 } }); } return; }
  if (m.gifQuadro) { try { gifQuadro(m.gifQuadro); } catch (e) { self.postMessage({ gifPronto: { id: m.gifQuadro.id, i: m.gifQuadro.i, falhou: 1 } }); } return; }
  if (m.gifSoltar) { gifSoltar(m.gifSoltar); return; }
  if (!HEAP32) { return; }
  try { decodificar(m); } catch (e) { falhou(m.job >> 2); }
};
