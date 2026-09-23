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
// OffscreenCanvas e escreve os pixels no bloco que o C JA ALOCOU. O fio
// principal so repassa o pedido (um postMessage) e nao toca em pixel nenhum.
//
// ESTE WORKER NUNCA ESPERA NINGUEM (22/09/2026). Ate a 1.4.1 ele pedia o
// bloco ao C depois do decode e ficava em Atomics.wait ate 8 s pela resposta;
// quando o C ja tinha desistido do pedido, eram 8 s parados por ninguem, com a
// fila inteira atras (icone de 128x128 levando 17 s na Samsung). Agora o C
// aloca tudo antes, pelo tamanho do cabecalho, e o Worker so fecha o job.
//
// PROTOCOLO, em job[0] (int32 na memoria compartilhada; o layout completo
// esta em src/webp.c, enum J_*):
//   0  aberto: o C espera
//   1  pronto: job[1]=w job[2]=h (0 = falhou) job[4]=ow job[5]=oh; pixels em
//      job[3], que tem job[8] bytes — nunca se escreve alem disso
//   4  o C desistiu: nada foi liberado, o job ainda e nosso
//   5  largado: terminamos um job abandonado; so agora o C o libera
// job[7] e o numero do pedido: se nao bate com a mensagem, o job nao e mais
// este pedido e nao se toca em nada.
//
// DOIS PAPEIS, UM ARQUIVO (23/09/2026). src/webp.c cria este arquivo DUAS
// vezes no arranque (navegador_iniciar):
//   - o DECODIFICADOR, que recebe pedidos pelo fio principal (caminho antigo)
//     ou por uma MessagePort (`porta`, canal direto);
//   - a SENTINELA (`sentinela: 1`), que dorme em Atomics.wait no topo de uma
//     pilha de jobs na memoria compartilhada, desempilha o que os fios de
//     decode do C empilharam e repassa pela outra ponta da porta. Ela nunca
//     volta ao laco de eventos, e por isso existe: o decodificador precisa do
//     laco dele livre para as promessas do createImageBitmap.
// Com isso um pedido vai do fio de decode ao decodificador sem passar pelo
// fio principal, que nos logs da Samsung fica 1 a 31 s ocupado. Cada papel
// liga um bit em canalEstado quando esta de pe; o C so usa o canal com os
// dois ligados.
//
// So ES5: o Chromium da TV (76) nao passa pelo esbuild aqui.
'use strict';
var HEAP32 = null, HEAPU8 = null;

function vivo(pJob, seq) {
  var e = Atomics.load(HEAP32, pJob);
  return HEAP32[pJob + 7] === seq && (e === 0 || e === 4);
}

// `direto`: veio pelo canal direto, e o fio principal nao guarda nada dele —
// nao ha `feito` a avisar (seria so trabalho a mais la).
function fim(pJob, seq, w, h, ow, oh, direto) {
  HEAP32[pJob + 1] = w; HEAP32[pJob + 2] = h;
  HEAP32[pJob + 4] = ow; HEAP32[pJob + 5] = oh;
  // 0 -> 1 entrega ao C; se ele ja desistiu (4), 4 -> 5 larga o job.
  if (Atomics.compareExchange(HEAP32, pJob, 0, 1) === 4) Atomics.compareExchange(HEAP32, pJob, 4, 5);
  Atomics.notify(HEAP32, pJob);
  if (!direto) self.postMessage({ feito: seq });
}

function decodificar(m) {
  var pJob = m.job >> 2, seq = m.seq;
  var largMax = m.largMax, d = !!m.direto;
  if (!vivo(pJob, seq)) return;
  // Abandonado antes de comecar: nao gasta decode, so larga.
  if (Atomics.load(HEAP32, pJob) === 4) { fim(pJob, seq, 0, 0, 0, 0, d); return; }
  // `slice`: copia para um ArrayBuffer comum. O Blob nao aceita vista sobre
  // SharedArrayBuffer. job[9]/job[10] e a COPIA do C, que vive com o job.
  var bytes = HEAPU8.slice(HEAP32[pJob + 9], HEAP32[pJob + 9] + HEAP32[pJob + 10]);
  createImageBitmap(new Blob([bytes], { type: m.mime })).then(function (bmp) {
    var ow = bmp.width, oh = bmp.height, w = ow, h = oh;
    if (!vivo(pJob, seq)) { if (bmp.close) bmp.close(); return; }
    if (Atomics.load(HEAP32, pJob) === 4) { if (bmp.close) bmp.close(); fim(pJob, seq, 0, 0, 0, 0, d); return; }
    if (largMax > 0 && ow > largMax) {
      w = largMax;
      h = Math.max(1, Math.round(oh * largMax / ow));
    }
    if (!(w > 0 && h > 0) || w * h * 4 > HEAP32[pJob + 8] || !HEAP32[pJob + 3]) {
      if (bmp.close) bmp.close(); fim(pJob, seq, 0, 0, ow, oh, d); return;
    }
    var cv = new OffscreenCanvas(w, h);
    var cx = cv.getContext('2d');
    cx.imageSmoothingEnabled = true;
    if ('imageSmoothingQuality' in cx) cx.imageSmoothingQuality = 'high';
    cx.drawImage(bmp, 0, 0, w, h);
    if (bmp.close) bmp.close();
    HEAPU8.set(cx.getImageData(0, 0, w, h).data, HEAP32[pJob + 3]);
    fim(pJob, seq, w, h, ow, oh, d);
  }).catch(function () { if (vivo(pJob, seq)) fim(pJob, seq, 0, 0, 0, 0, d); });
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

// Layout do job que a sentinela le (espelho do enum J_* de src/webp.c).
var J_SEQ = 7, J_FILA = 12, J_MIME = 13, J_LARG = 14;

// SENTINELA. Pilha de Treiber: os fios do C empilham por CAS em job[J_FILA];
// aqui se pega a pilha INTEIRA de uma vez (exchange por 0), e por isso nao ha
// ABA. A pilha sai do mais novo para o mais velho; repassa na ordem de chegada.
// Os campos de um job so sao lidos ANTES de ele ser repassado: depois disso o
// decodificador pode fecha-lo e o C pode libera-lo.
function sentinela(m) {
  var H = new Int32Array(m.memoria);
  var cab = m.cab >> 2, porta = m.porta;
  Atomics.or(H, m.canal >> 2, 2);
  for (;;) {
    var p = Atomics.exchange(H, cab, 0);
    if (!p) { Atomics.wait(H, cab, 0); continue; }
    var lista = [];
    while (p) { lista.push(p); p = H[(p >> 2) + J_FILA]; }
    for (var i = lista.length - 1; i >= 0; i--) {
      var j = lista[i] >> 2;
      porta.postMessage({ job: lista[i], seq: H[j + J_SEQ], mime: H[j + J_MIME] === 2 ? 'image/png' : 'image/webp',
                          largMax: H[j + J_LARG], direto: 1 });
    }
  }
}

function pedido(m) {
  if (!HEAP32) { return; }
  try { decodificar(m); } catch (e) { if (vivo(m.job >> 2, m.seq)) fim(m.job >> 2, m.seq, 0, 0, 0, 0, !!m.direto); }
}

self.onmessage = function (ev) {
  var m = ev.data;
  if (m.sentinela) { sentinela(m); return; }
  if (m.memoria) {
    HEAP32 = new Int32Array(m.memoria);
    HEAPU8 = new Uint8Array(m.memoria);
    if (m.porta) {
      m.porta.onmessage = function (e) { pedido(e.data); };
      Atomics.or(HEAP32, m.canal >> 2, 1);
    }
    return;
  }
  if (m.gif)       { try { gifIniciar(m.gif); } catch (e) { self.postMessage({ gifPronto: { id: m.gif.id, i: -1, falhou: 1 } }); } return; }
  if (m.gifQuadro) { try { gifQuadro(m.gifQuadro); } catch (e) { self.postMessage({ gifPronto: { id: m.gifQuadro.id, i: m.gifQuadro.i, falhou: 1 } }); } return; }
  if (m.gifSoltar) { gifSoltar(m.gifSoltar); return; }
  pedido(m);
};
