// GIF animado das capas de colecao (#29) e das fotos de perfil (#45).
// Ver gif.h para onde anima e por que.
#include "gif.h"
// 1 quadro a frente (o da rc1) — ver a nota de ADD_FIOS em addons.c (#80).
#ifndef NV_GIF_ADIANTE
#define NV_GIF_ADIANTE 1
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include "tex_cache.h"
#endif

// ---------------------------------------------------------------- estrutura
//
// CAMINHA PELO ARQUIVO SEM DECODIFICAR PIXEL NENHUM. Todo bloco do GIF e
// prefixado por tamanho, entao da para achar cada quadro pulando os dados
// comprimidos sem tocar no LZW. Isso serve a duas perguntas:
//   - "isto e animacao ou uma imagem parada?" (gif_animado)
//   - "onde comeca e acaba cada quadro?"      (gif_mapear + gif_montar)
//
// Nao ha limite de tamanho aqui de proposito: a funcao le o arquivo inteiro
// uma vez, na abertura do painel, e o maior GIF de capa medido tem ~1 MB. Se
// um dia isso mudar, o teto entra em quem chama.

// Pula uma cadeia de sub-blocos: cada um comeca com o proprio tamanho, e a
// cadeia acaba num tamanho zero. Devolve a posicao depois do terminador, ou 0
// quando o arquivo acaba no meio (arquivo truncado nao vira laco infinito).
static size_t pularSubBlocos(const unsigned char *b, size_t n, size_t p) {
  while (p < n) {
    size_t len = b[p++];
    if (!len) return p;
    if (p + len > n) return 0;
    p += len;
  }
  return 0;
}

// Cabecalho + descritor de tela logica + paleta global, em bytes. 0 se o
// arquivo nem chega a ser um GIF.
static size_t cabecalhoN(const unsigned char *b, size_t n) {
  size_t p;
  if (n < 14 || memcmp(b, "GIF8", 4)) return 0;
  // 6 da assinatura + 7 do descritor de tela; o bit 7 do quinto byte do
  // descritor diz se ha paleta global, que ocupa 3 * 2^(N+1) bytes depois.
  p = 13;
  if (b[10] & 0x80) p += (size_t)3 << ((b[10] & 7) + 1);
  return p <= n ? p : 0;
}

// Le o arquivo inteiro. Devolve NULL (e nao deixa lixo) em qualquer falha.
static unsigned char *lerTudo(const char *caminho, size_t *n) {
  FILE *f;
  long tam;
  unsigned char *b;
  size_t lidos;
  *n = 0;
  if (!caminho || !caminho[0]) return NULL;
  f = fopen(caminho, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  tam = ftell(f);
  rewind(f);
  if (tam < 14) { fclose(f); return NULL; }
  b = (unsigned char *)malloc((size_t)tam);
  if (!b) { fclose(f); return NULL; }
  lidos = fread(b, 1, (size_t)tam, f);
  fclose(f);
  if (lidos != (size_t)tam) { free(b); return NULL; }
  *n = lidos;
  return b;
}

// ---------------------------------------------------------------- orcamento
//
// POR QUE HA UM (24/09/2026). Registros 2340/2341/2351 (Tizen 6, deviceMemory
// 1): cada GIF de avatar que comecava a animar custava um quadro de 2,3 s
// (`des=2353`, `des=2342`, `des=2387`) e o Worker nao dava conta do ritmo ("deu
// a volta nos 35 quadros em 4119 ms", um GIF de 1050 ms). Numa TV que ja passa
// segundos parada com a propria home (`swap=` de 3 a 6 s sem GIF nenhum), a
// animacao e trabalho que ela nao tem de onde tirar. Nao esta provado que o
// GIF derrubou a pagina: as tres sessoes morreram na home, 150 a 420 s depois
// do ultimo GIF. O que esta medido e o custo acima.
//
// A MEDIDA E O QUE O GIF DECODIFICA POR VOLTA, quadros x tela logica x 4. Nao e
// memoria presa (o Worker guarda um quadro composto por vez, ver
// tools/decodificador.js), e o trabalho de decode por volta — e seria a
// memoria presa na <img> de reserva, que guarda todos.
//
//   RAM <= 1 GB       0   nao anima: fica o primeiro quadro, parado
//   1 GB < RAM < 4   48 MB  35x512x512 (37 MB) e 51x360x360 (26 MB) animam;
//                          75x500x375 (56 MB) fica parado
//   RAM >= 4 GB     sem teto (o que sempre foi)
//   sem deviceMemory 48 MB  Chromium que nao informa e o mais velho
//
// Os 48 MB sao CHUTE pelos GIFs dos registros, nao medida de limite: nenhum
// aparelho de 2 GB aqui. Os GIFs de colecao vistos (21x498x448 = 19 MB,
// 45x480x270 = 23 MB) continuam animando em 2 GB.
size_t gif_custo(int quadros, int telaW, int telaH) {
  if (quadros < 1 || telaW < 1 || telaH < 1) return 0;
  return (size_t)quadros * (size_t)telaW * (size_t)telaH * 4u;
}

// ARTE ANTES DO GIF. Ver gif.h. POR QUE (registros D1 da TV do rawldon,
// Tizen 6, 2 GB): na tela de perfis o FPS estava em 32-36 e caiu para 21-24
// assim que os avatares GIF passaram a animar (2565, 1.4.5, 32,6 s a 46,5 s).
// E a home ja carrega cartazes por tras dessa tela: nas sessoes 1839 (1.4.1),
// 2512 e 2519 (1.4.4) o GIF animava nos MESMOS segundos em que os primeiros
// cartazes baixavam e decodificavam (decode de 476 a 1413 ms cada). O Worker
// do GIF e outro, mas os quatro nucleos sao os mesmos. E o GIF nem entregava
// o ritmo que pedia: 21 quadros de 50 ms (1,05 s) davam a volta em 2,3 a
// 3,5 s, e o do relato do rawldon, 198 quadros em 27,6 s — 7 por segundo.
// Nao esta medido quanto a arte ganha com isto; esta medido que o GIF
// disputa CPU no momento em que ela carrega.
int gif_pode_trocar(double agora, double vence, double ultimaTroca, int artePendente) {
  if (agora < vence) return 0;
  if (artePendente > 0 && agora - ultimaTroca < GIF_PASSO_OCUPADO_MS) return 0;
  return 1;
}

size_t gif_orcamento_para(double memGB) {
  if (memGB <= 0.0) return (size_t)48 * 1024 * 1024;
  if (memGB <= 1.0) return 0;
  if (memGB < 4.0) return (size_t)48 * 1024 * 1024;
  return GIF_SEM_TETO;
}

#ifdef __EMSCRIPTEN__
EM_JS(double, gif_js_memoria_gb, (), {
  try {
    var m = (typeof navigator !== 'undefined') ? navigator.deviceMemory : 0;
    return (typeof m === 'number' && m > 0) ? m : 0;
  } catch (e) { return 0; }
});

// Decidido uma vez: a RAM nao muda com o app aberto.
static size_t orcamento(void) {
  static int lido;
  static size_t orc;
  if (!lido) {
    double gb = gif_js_memoria_gb();
    lido = 1;
    orc = gif_orcamento_para(gb);
    if (orc == GIF_SEM_TETO)
      printf("[gif] orcamento de animacao: sem teto (deviceMemory=%g GB)\n", gb);
    else if (!orc)
      printf("[gif] orcamento de animacao: nenhum (deviceMemory=%g GB): GIF fica no primeiro quadro\n", gb);
    else
      printf("[gif] orcamento de animacao: %u MB por GIF (deviceMemory=%g GB)\n",
             (unsigned)(orc / (1024 * 1024)), gb);
    fflush(stdout);
  }
  return orc;
}

int gif_pode_animar(void) { return orcamento() != 0; }
#else
int gif_pode_animar(void) { return 0; }
#endif

int gif_mapear(const unsigned char *b, size_t n, GifQuadro *q, int max) {
  size_t p, gce = 0, gceN = 0;
  int achados = 0;
  if (!b || !q || max < 1) return 0;
  p = cabecalhoN(b, n);
  if (!p) return 0;

  while (p < n && achados < max) {
    unsigned char sep = b[p];
    if (sep == 0x3B) break;                       // fim do arquivo
    if (sep == 0x21) {                            // extensao: rotulo + blocos
      size_t ini = p, fimExt;
      if (p + 2 > n) break;
      fimExt = pularSubBlocos(b, n, p + 2);
      if (!fimExt) break;
      // O CONTROLE GRAFICO VALE PARA O PROXIMO QUADRO, e e dele que saem o
      // atraso e o metodo de descarte. Guardado inteiro (e nao so
      // interpretado) porque gif_montar precisa copia-lo junto do quadro.
      if (b[p + 1] == 0xF9) { gce = ini; gceN = fimExt - ini; }
      p = fimExt;
      continue;
    }
    if (sep != 0x2C) break;                       // byte inesperado: desiste
    if (p + 10 > n) break;                        // separador + 9 do descritor
    {
      GifQuadro *d = &q[achados];
      unsigned char campos = b[p + 9];
      size_t dados = p + 10;
      d->esq  = b[p + 1] | (b[p + 2] << 8);
      d->topo = b[p + 3] | (b[p + 4] << 8);
      d->larg = b[p + 5] | (b[p + 6] << 8);
      d->alt  = b[p + 7] | (b[p + 8] << 8);
      if (campos & 0x80) dados += (size_t)3 << ((campos & 7) + 1);  // paleta local
      if (dados >= n) break;
      dados = pularSubBlocos(b, n, dados + 1);    // +1: tamanho minimo do codigo
      if (!dados) break;                          // cadeia nao fechou: nao e quadro
      d->ini = p; d->fim = dados;
      d->gce = gce; d->gceN = gceN;
      d->atraso = 100;
      d->descarte = 0;
      if (gceN >= 6) {
        d->descarte = (b[gce + 3] >> 2) & 7;
        d->atraso = (b[gce + 4] | (b[gce + 5] << 8)) * 10;
        // ATRASO DE 0 OU 1 CENTESIMO VIRA 100 ms. E o que todo navegador faz
        // com os GIF antigos que gravavam 0; sem isto um GIF "de 0 ms"
        // tentaria trocar de quadro a cada volta do laco.
        if (d->atraso < 20) d->atraso = 100;
      }
      gce = 0; gceN = 0;
      achados++;
      p = dados;
    }
  }
  return achados;
}

size_t gif_montar(const unsigned char *b, size_t n, const GifQuadro *q,
                  unsigned char *saida, size_t cap) {
  size_t cab, total;
  if (!b || !q) return 0;
  cab = cabecalhoN(b, n);
  if (!cab) return 0;
  if (q->fim <= q->ini || q->fim > n) return 0;
  if (q->gceN && q->gce + q->gceN > n) return 0;
  total = cab + q->gceN + (q->fim - q->ini) + 1;   // +1: o 0x3B do fim
  if (!saida) return total;
  if (cap < total) return 0;
  memcpy(saida, b, cab);
  if (q->gceN) memcpy(saida + cab, b + q->gce, q->gceN);
  memcpy(saida + cab + q->gceN, b + q->ini, q->fim - q->ini);
  saida[total - 1] = 0x3B;
  return total;
}

int gif_animado(const char *caminho) {
  GifQuadro q[2];
  size_t n = 0;
  unsigned char *b = lerTudo(caminho, &n);
  int r;
  if (!b) return 0;
  // DOIS JA RESPONDEM A PERGUNTA. Varrer um GIF de duzentos quadros ate o fim
  // so para dizer "sim" seria trabalho jogado fora.
  r = gif_mapear(b, n, q, 2) > 1;
  free(b);
  return r;
}

// ---------------------------------------------------------------- animacao
#ifdef __EMSCRIPTEN__

// QUEM CONTA O TEMPO E O APP, E NAO O NAVEGADOR — e essa e a correcao do #49.
//
// A versao anterior punha o GIF inteiro numa <img> e pedia a ela, 15 vezes por
// segundo, "o que esta valendo agora" (drawImage num canvas). A ideia era que
// o Chromium anima uma <img> sozinho. Ele anima — QUANDO PINTA a imagem. Nao e
// um relogio: o avanco de quadro do Blink sai do desenho da propria imagem.
//
// MEDIDO em Chrome 152, com o GIF de tres quadros de /tmp e amostragem a cada
// 67 ms durante 5-6 s, em tres ambientes independentes (aba de painel oculta,
// headless=new com visibilityState "visible", janela ocluida): das ~90
// amostras de cada rodada, 100% devolveram o PRIMEIRO quadro. E as quatro
// variantes deram o MESMO resultado — <img> fora do DOM, <img> no DOM de 2x2
// com opacity 0.02 (exatamente o que a v1.0.51 passou a fazer), <img> no DOM
// de 300x170 opaca, e URL de blob contra URL http. Ou seja: anexar a <img> ao
// documento, que foi a correcao da v1.0.51, nao muda nada que se possa medir.
// Forcar repintura por captura de tela tambem nao descongelou.
//
// Nao consegui o controle positivo (uma janela de verdade, pintada num monitor
// aceso) nesta maquina, entao NAO afirmo qual condicao especifica congela a
// TV do @rawldon. Afirmo o que basta para decidir: o desenho da capa dependia
// de um comportamento do navegador que o app nao controla, nao observa e nao
// consegue nem detectar quando falha — e falha em silencio, sempre no quadro
// zero. Isso sai do caminho.
//
// O QUE ENTRA NO LUGAR: gif_mapear corta o arquivo em quadros e gif_montar
// remonta cada um como um GIF DE UM QUADRO SO. Do navegador passa a se usar so
// o decodificador de imagem PARADA — nao ha animacao nenhuma para ele avancar
// ou parar. O app compoe os quadros num canvas (honrando o metodo de descarte,
// que a <img> fazia de graca) e sobe o resultado para a textura, GPU->GPU.
//
// Se o fatiamento falhar (GIF que o caminhamento nao entende), o caminho
// antigo da <img> continua ai como reserva: nunca fica pior que antes.

// Teto de quadros. 256 quadros a 100 ms sao 25 s de animacao — uma capa de
// colecao nao chega perto disso, e o teto limita o estado estatico a ~12 KB.
#define NV_GIF_MAX_Q 256

// --- reserva: o caminho antigo, pela <img> que o navegador anima -------------
EM_JS(void, gif_js_preparar, (const char *cam, const unsigned char *dados, int n), {
  var caminho = UTF8ToString(cam);
  if (Module.nvGif && Module.nvGif.caminho === caminho) return;
  if (Module.nvGif && Module.nvGif.url) URL.revokeObjectURL(Module.nvGif.url);
  if (Module.nvGif && Module.nvGif.img && Module.nvGif.img.parentNode)
    Module.nvGif.img.parentNode.removeChild(Module.nvGif.img);
  var bytes = HEAPU8.slice(dados, dados + n);
  var url = URL.createObjectURL(new Blob([bytes], { type: 'image/gif' }));
  var img = new Image();
  img.src = url;
  // A <img> entra no documento. MEDIDO que isto nao basta (ver a nota grande
  // acima), mas tambem nao custa nada e nao piora: fica, porque este ramo so
  // roda quando o fatiamento falhou e ai tudo que possa ajudar vale.
  img.style.cssText = 'position:fixed;left:0;top:0;width:2px;height:2px;' +
                      'opacity:0.02;pointer-events:none;z-index:9';
  (document.body || document.documentElement).appendChild(img);
  Module.nvGif = { caminho: caminho, url: url, img: img, cv: null, cx: null };
});

// Devolve 0 enquanto a <img> nao carregou; depois disso escreve os pixels em
// `destino` (RGBA, larg*alt*4) e devolve 1.
EM_JS(int, gif_js_quadro, (unsigned char *destino, int larg, int alt), {
  var g = Module.nvGif;
  if (!g || !g.img || !g.img.complete || !g.img.naturalWidth) return 0;
  if (!g.cv || g.cv.width !== larg || g.cv.height !== alt) {
    g.cv = document.createElement('canvas');
    g.cv.width = larg; g.cv.height = alt;
    g.cx = g.cv.getContext('2d');
  }
  g.cx.clearRect(0, 0, larg, alt);
  g.cx.drawImage(g.img, 0, 0, larg, alt);
  var d = g.cx.getImageData(0, 0, larg, alt).data;
  HEAPU8.set(d, destino);
  return 1;
});

// Sobe o quadro direto para a textura pela propria WebGL, sem passar por
// getImageData nem pelo heap do wasm. `GL.textures[nome]` e a tabela da
// biblioteca GL do Emscripten: o nome que o C recebeu de glGenTextures aponta
// para o WebGLTexture de verdade.
EM_JS(int, gif_js_subir, (int nomeTex, int larg, int alt, int mesmoTamanho), {
  var g = Module.nvGif;
  if (!g || !g.img || !g.img.complete || !g.img.naturalWidth) return 0;
  var ctx = (typeof GL !== 'undefined' && GL.currentContext) ? GL.currentContext.GLctx : null;
  var tex = (typeof GL !== 'undefined' && GL.textures) ? GL.textures[nomeTex] : null;
  if (!ctx || !tex) return 0;
  if (!g.cv || g.cv.width !== larg || g.cv.height !== alt) {
    g.cv = document.createElement('canvas');
    g.cv.width = larg; g.cv.height = alt;
    g.cx = g.cv.getContext('2d');
  }
  g.cx.drawImage(g.img, 0, 0, larg, alt);
  ctx.bindTexture(ctx.TEXTURE_2D, tex);
  try {
    if (mesmoTamanho) ctx.texSubImage2D(ctx.TEXTURE_2D, 0, 0, 0, ctx.RGBA, ctx.UNSIGNED_BYTE, g.cv);
    else              ctx.texImage2D(ctx.TEXTURE_2D, 0, ctx.RGBA, ctx.RGBA, ctx.UNSIGNED_BYTE, g.cv);
  } catch (e) { return 0; }
  return 1;
});

EM_JS(void, gif_js_soltar, (), {
  if (Module.nvGif && Module.nvGif.url) URL.revokeObjectURL(Module.nvGif.url);
  // Sai do documento junto: sem isto cada cartaz focado deixaria uma <img>
  // no canto, gastando decodificacao por nada.
  if (Module.nvGif && Module.nvGif.img && Module.nvGif.img.parentNode)
    Module.nvGif.img.parentNode.removeChild(Module.nvGif.img);
  Module.nvGif = null;
});

EM_JS(int, gif_js_largura, (), {
  var g = Module.nvGif;
  return (g && g.img && g.img.naturalWidth) ? g.img.naturalWidth : 0;
});
EM_JS(int, gif_js_altura, (), {
  var g = Module.nvGif;
  return (g && g.img && g.img.naturalHeight) ? g.img.naturalHeight : 0;
});

// --- caminho principal: um quadro por vez, relogio do app --------------------

EM_JS(void, gif_js_seq_soltar, (), {
  var s = Module.nvGifSeq;
  if (!s) return;
  for (var i = 0; i < s.urls.length; i++) if (s.urls[i]) URL.revokeObjectURL(s.urls[i]);
  if (s.wk) { try { s.wk.postMessage({ gifSoltar: { id: s.wid } }); } catch (e) {} }
  { var pr; for (pr in s.prontos) { if (s.prontos.hasOwnProperty(pr) && s.prontos[pr] && s.prontos[pr].close) s.prontos[pr].close(); } }
  Module.nvGifSeq = null;
});

// `telaW`/`telaH` sao a TELA LOGICA do GIF, que e onde os quadros se compoem —
// e nao o tamanho de um quadro, que pode ser menor (quadro parcial).
EM_JS(void, gif_js_seq_iniciar, (int n, int telaW, int telaH), {
  var v = Module.nvGifSeq;
  if (v) for (var i = 0; i < v.urls.length; i++) if (v.urls[i]) URL.revokeObjectURL(v.urls[i]);
  Module.nvGifSeq = { n: n, w: telaW, h: telaH,
                      urls: new Array(n), imgs: new Array(n), meta: new Array(n),
                      comp: null, cctx: null, salvo: null, posto: -1, subido: -1,
                      cv: null, cx: null,
                      // caminho pelo Worker (#84): quadros crus, sessao no worker,
                      // bitmaps prontos e pedidos em voo. `wk` fica null quando
                      // nao ha Worker (Chromium velho) e o caminho <img> continua.
                      bufs: new Array(n), wk: null, wid: 0, outW: 0, outH: 0,
                      prontos: {}, pedidos: {}, falhas: 0 };
  Module.nvGifId = (Module.nvGifId | 0) + 1;
  Module.nvGifSeq.wid = Module.nvGifId;
  // URL DE BLOB SO QUANDO O CAMINHO <img> PRECISA (24/09/2026). Antes cada
  // quadro ganhava a sua na chegada: 75 createObjectURL no fio principal e
  // mais uma copia do GIF inteiro no registro de blobs do navegador, para um
  // caminho que so roda quando o Worker falta. Com o Worker, nenhuma nasce.
  if (!Module.nvGifUrl) Module.nvGifUrl = function (q, k) {
    if (!q.urls[k] && q.bufs[k]) q.urls[k] = URL.createObjectURL(new Blob([q.bufs[k]], { type: 'image/gif' }));
    return q.urls[k];
  };
});

// Um quadro, ja remontado como GIF de um quadro so. So guarda os BYTES (uma
// URL de blob): o <img> e a decodificacao vem depois, sob demanda.
EM_JS(void, gif_js_seq_quadro, (int i, const unsigned char *d, int n,
                                int esq, int topo, int larg, int alt, int descarte), {
  var s = Module.nvGifSeq;
  if (!s || i < 0 || i >= s.n) return;
  var bytes = HEAPU8.slice(d, d + n);
  s.bufs[i] = bytes.buffer;
  s.meta[i] = { esq: esq, topo: topo, larg: larg, alt: alt, descarte: descarte };
});

// LIGA A SESSAO NO WORKER, uma vez por GIF e por tamanho de saida.
//
// WORKER PROPRIO DO GIF (21/09/2026), e nao o do decode de imagem. Na 1.3.8
// o GIF dividia o Worker com os JPEG/WebP, e aquele Worker BLOQUEIA
// (Atomics.wait ate 8 s) enquanto espera o fio de decode do C alocar o
// bloco de cada imagem (ate a 1.4.1; desde 22/09/2026 ele nao espera mais,
// ver src/webp.c, mas separar os dois continua sem custo). Numa TV de 2 GB com muitas artes chegando, os
// quadros do GIF ficavam na fila atras dessas esperas: medido nos registros
// da 1.3.8/1.3.9, "deu a volta nos 90 quadros em 200745 ms" (pokaz) e "83
// quadros em 845400 ms" (cudz007) — 2 a 10 s por quadro. Este Worker so
// compoe GIF, nunca espera ninguem, e nao precisa da memoria compartilhada.
// Sem Worker ou OffscreenCanvas (ou depois de 3 falhas) tudo cai no <img>.
EM_JS(int, gif_js_seq_worker, (int outW, int outH), {
  var s = Module.nvGifSeq;
  var D = Module.nvDecGif;
  if (!s) return 0;
  if (s.wk && s.outW === outW && s.outH === outH) return 1;
  if (D === undefined && typeof Worker !== 'undefined' && typeof OffscreenCanvas !== 'undefined') {
    D = {}; D.w = null; D.morto = false;
    Module.nvDecGif = D;
    try {
      D.w = new Worker('decodificador.js');
      D.w.onerror = function () { D.morto = true; };
      D.w.onmessage = function (ev) {
        var m = ev.data;
        if (!m || !m.gifPronto) return;
        var q = Module.nvGifSeq;
        if (!q || q.wid !== m.gifPronto.id) { if (m.bitmap && m.bitmap.close) m.bitmap.close(); return; }
        delete q.pedidos[m.gifPronto.i];
        if (m.gifPronto.falhou) { q.falhas++; if (q.falhas > 3) { q.wk = null; } return; }
        if (q.prontos[m.gifPronto.i] && q.prontos[m.gifPronto.i].close) q.prontos[m.gifPronto.i].close();
        q.prontos[m.gifPronto.i] = m.bitmap;
      };
    } catch (e) { D.morto = true; }
  }
  if (!D || D.morto || !D.w || typeof OffscreenCanvas === 'undefined') { s.wk = null; return 0; }
  // Sessao nova (ou tamanho novo): manda os quadros crus. Copias, nao
  // transferencia: os ArrayBuffers ficam aqui para o caminho <img> de reserva.
  var k, ids = [];
  for (k = 0; k < s.n; k++) { if (!s.bufs[k]) { s.wk = null; return 0; } ids.push(s.bufs[k].slice(0)); }
  if (s.wk) D.w.postMessage({ gifSoltar: { id: s.wid } });
  Module.nvGifId = (Module.nvGifId | 0) + 1;
  s.wid = Module.nvGifId;
  var pr; for (pr in s.prontos) { if (s.prontos.hasOwnProperty(pr) && s.prontos[pr] && s.prontos[pr].close) s.prontos[pr].close(); }
  s.prontos = {}; s.pedidos = {}; s.falhas = 0;
  s.outW = outW; s.outH = outH;
  D.w.postMessage({ gif: { id: s.wid, w: s.w, h: s.h, outW: outW, outH: outH, meta: s.meta, frames: ids } }, ids);
  s.wk = D.w;
  return 1;
});

EM_JS(void, gif_js_seq_pedir, (int i), {
  var s = Module.nvGifSeq;
  if (!s || !s.wk || i < 0 || i >= s.n) return;
  if (s.prontos[i] || s.pedidos[i]) return;
  s.pedidos[i] = 1;
  s.wk.postMessage({ gifQuadro: { id: s.wid, i: i } });
});

// O quadro `i` ja esta decodificado? Cria o <img> na primeira pergunta, o que
// dispara a decodificacao — e por isso que o C pergunta pelo PROXIMO quadro
// antes de precisar dele.
EM_JS(int, gif_js_seq_pronto, (int i), {
  var s = Module.nvGifSeq;
  if (!s || i < 0 || i >= s.n || !s.bufs[i]) return 0;
  if (s.wk) { if (s.prontos[i]) return 1; if (!s.pedidos[i]) s.wk.postMessage({ gifQuadro: { id: s.wid, i: i } }), s.pedidos[i] = 1; return 0; }
  if (!s.imgs[i]) { var im = new Image(); im.src = Module.nvGifUrl(s, i); s.imgs[i] = im; }
  return (s.imgs[i].complete && s.imgs[i].naturalWidth) ? 1 : 0;
});

// COMPOE O QUADRO `i` E SOBE PARA A TEXTURA. Devolve 0 enquanto o quadro nao
// decodificou — o chamador segura o que estava na tela.
EM_JS(int, gif_js_seq_subir, (int i, int nomeTex, int larg, int alt, int mesmoTamanho, int adiante), {
  var s = Module.nvGifSeq;
  if (!s || i < 0 || i >= s.n) return 0;
  if (s.wk) {
    // CAMINHO PELO WORKER: o bitmap chega composto e no tamanho certo; aqui
    // so o upload. Pede os proximos `adiante` quadros; um bitmap pronto e
    // descartado logo depois de subir (o worker manda outro quando pedido).
    var bmp = s.prontos[i];
    var d2;
    for (d2 = 1; d2 <= adiante; d2++) {
      var pk2 = (i + d2) % s.n;
      if (!s.prontos[pk2] && !s.pedidos[pk2]) { s.pedidos[pk2] = 1; s.wk.postMessage({ gifQuadro: { id: s.wid, i: pk2 } }); }
    }
    if (s.subido === i && mesmoTamanho) return 1;
    if (!bmp) return 0;
    var ctx2 = (typeof GL !== 'undefined' && GL.currentContext) ? GL.currentContext.GLctx : null;
    var tex2 = (typeof GL !== 'undefined' && GL.textures) ? GL.textures[nomeTex] : null;
    if (!ctx2 || !tex2) return 0;
    ctx2.bindTexture(ctx2.TEXTURE_2D, tex2);
    try {
      if (mesmoTamanho && bmp.width === larg && bmp.height === alt)
        ctx2.texSubImage2D(ctx2.TEXTURE_2D, 0, 0, 0, ctx2.RGBA, ctx2.UNSIGNED_BYTE, bmp);
      else
        ctx2.texImage2D(ctx2.TEXTURE_2D, 0, ctx2.RGBA, ctx2.RGBA, ctx2.UNSIGNED_BYTE, bmp);
    } catch (e) { return 0; }
    if (bmp.close) bmp.close();
    delete s.prontos[i];
    s.subido = i;
    return 1;
  }
  var im = s.imgs[i];
  if (!im) {
    if (!Module.nvGifUrl(s, i)) return 0;
    im = new Image(); im.src = s.urls[i]; s.imgs[i] = im;
  }
  if (!im.complete || !im.naturalWidth) return 0;
  if (!s.comp) {
    s.comp = document.createElement('canvas');
    s.comp.width = s.w; s.comp.height = s.h;
    s.cctx = s.comp.getContext('2d');
  }
  if (s.posto !== i) {
    // DESCARTE DO QUADRO ANTERIOR. E o que a <img> fazia de graca e o que
    // faltaria se cada quadro fosse simplesmente desenhado por cima do outro.
    if (i === 0) s.cctx.clearRect(0, 0, s.w, s.h);      // volta do laco
    else if (s.posto >= 0) {
      var a = s.meta[s.posto];
      if (a && a.descarte === 2) s.cctx.clearRect(a.esq, a.topo, a.larg, a.alt);
      else if (a && a.descarte === 3 && s.salvo) {
        try { s.cctx.putImageData(s.salvo, 0, 0); } catch (e) {}
      }
    }
    var m = s.meta[i];
    if (m && m.descarte === 3) {
      try { s.salvo = s.cctx.getImageData(0, 0, s.w, s.h); } catch (e) { s.salvo = null; }
    }
    // O <img> ja e do tamanho da tela logica, com o quadro na posicao certa e
    // o resto transparente: por isso 0,0 e nao esq,topo.
    s.cctx.drawImage(im, 0, 0);
    s.posto = i;
    // JANELA DE QUATRO QUADROS decodificados (este e os tres seguintes), e os
    // tres seguintes ja em decode. Eram tres com so o proximo em decode: na
    // Samsung o decode de um quadro leva mais que o atraso do GIF (60-100 ms)
    // e o quadro seguinte nunca estava pronto na hora — "plays but stutters"
    // (rawldon, #72, 1.3.4-rc1). Uma capa de 90 quadros de 640x360 seriam
    // ~80 MB se todos ficassem vivos; quatro sao ~4 MB.
    // NV_GIF_ADIANTE quadros em decode a frente (3; a rc1 tinha 1 — a build
    // de comparacao do #80 usa 1 para isolar se o decode extra no fio
    // principal e o que voltou a travar).
    var ad = adiante;
    for (var k = 0; k < s.n; k++) {
      var dentro = false;
      for (var d0 = 0; d0 <= ad; d0++) if (k === (i + d0) % s.n) dentro = true;
      if (!dentro && s.imgs[k]) s.imgs[k] = null;
    }
    for (var d = 1; d <= ad; d++) {
      var pk = (i + d) % s.n;
      if (!s.imgs[pk] && Module.nvGifUrl(s, pk)) { var nk = new Image(); nk.src = s.urls[pk]; s.imgs[pk] = nk; }
    }
  }
  // MESMO QUADRO E MESMO TAMANHO: nao ha o que subir de novo. Quem chama
  // pergunta a 15 fps e o GIF pode estar a 8.
  if (s.subido === i && mesmoTamanho) return 1;
  if (!s.cv || s.cv.width !== larg || s.cv.height !== alt) {
    s.cv = document.createElement('canvas');
    s.cv.width = larg; s.cv.height = alt;
    s.cx = s.cv.getContext('2d');
  }
  s.cx.clearRect(0, 0, larg, alt);
  s.cx.drawImage(s.comp, 0, 0, larg, alt);
  var ctx = (typeof GL !== 'undefined' && GL.currentContext) ? GL.currentContext.GLctx : null;
  var tex = (typeof GL !== 'undefined' && GL.textures) ? GL.textures[nomeTex] : null;
  if (!ctx || !tex) return 0;
  ctx.bindTexture(ctx.TEXTURE_2D, tex);
  try {
    if (mesmoTamanho) ctx.texSubImage2D(ctx.TEXTURE_2D, 0, 0, 0, ctx.RGBA, ctx.UNSIGNED_BYTE, s.cv);
    else              ctx.texImage2D(ctx.TEXTURE_2D, 0, ctx.RGBA, ctx.RGBA, ctx.UNSIGNED_BYTE, s.cv);
  } catch (e) { return 0; }
  s.subido = i;
  return 1;
});

static GLuint tex;
static char   preso[512];
static int    texW, texH;

static GifQuadro seq[NV_GIF_MAX_Q];
static int    nSeq, iSeq;
static int    telaW, telaH;
static double proxTroca;          // instante (ms) em que o quadro corrente vence
static double ultimaTroca;        // instante (ms) da ultima troca de quadro
static int    avisouFila;         // o log de "GIF cede a arte" ja saiu neste GIF

// Pedidos de arte em rede ou decode (tex_cache.c). So onde a RAM tem teto de
// GIF (< 4 GB): nas TVs maiores o GIF segue o ritmo proprio, como sempre.
// Chamado so quando um quadro vence, nao a cada desenho.
static int artePendente(void) {
  int pend = 0;
  if (orcamento() == GIF_SEM_TETO) return 0;
  tex_estatisticas(NULL, &pend, NULL, NULL, NULL);
  if (pend > 0 && !avisouFila) {
    avisouFila = 1;
    printf("[gif] %d arte(s) na fila: o GIF troca no maximo a cada %.0f ms ate ela esvaziar\n",
           pend, GIF_PASSO_OCUPADO_MS);
    fflush(stdout);
  }
  return pend;
}
// A VOLTA COMPLETA, uma linha de log so. Sem ela o log diz que o fatiamento
// deu certo e nao diz se o quadro chegou a TROCAR — que e a pergunta do #49, e
// a que as fotos do aparelho nao respondiam. Uma linha por capa focada.
static double inicioVolta;
static int    contouVolta;
// O GIF preso passou do orcamento: gif_textura devolve 0 sem reler o arquivo
// ate o caminho mudar (ou gif_parar/gif_ocioso soltar).
static int    recusado;
// Ultima vez que alguem pediu gif_textura (ms). E o que gif_ocioso olha.
static double ultimoUso;
#ifndef NV_GIF_OCIOSO_MS
#define NV_GIF_OCIOSO_MS 1500
#endif

// Corta o arquivo e entrega os quadros ao navegador. Deixa nSeq em 0 quando
// nao der para fatiar — e ai o chamador cai no caminho antigo.
static void fatiar(const unsigned char *b, size_t n) {
  unsigned char *tmp = NULL;
  size_t cap = 0;
  int i;
  nSeq = gif_mapear(b, n, seq, NV_GIF_MAX_Q);
  if (nSeq < 2) {
    printf("[gif] so %d quadro(s) no fatiamento: fica no caminho antigo da <img>\n", nSeq);
    fflush(stdout);
    nSeq = 0;
    return;
  }
  telaW = b[6] | (b[7] << 8);
  telaH = b[8] | (b[9] << 8);
  if (telaW < 1 || telaH < 1) { nSeq = 0; return; }
  // O ORCAMENTO DECIDE ANTES DE QUALQUER COPIA para o navegador: recusado, o
  // GIF nao custa mais nada alem desta leitura (quem chama fica na foto).
  { size_t custo = gif_custo(nSeq, telaW, telaH), orc = orcamento();
    if (orc != GIF_SEM_TETO && custo > orc) {
      printf("[gif] %d quadros %dx%d = %u MB por volta, acima do orcamento de %u MB: fica o primeiro quadro\n",
             nSeq, telaW, telaH, (unsigned)(custo / (1024 * 1024)), (unsigned)(orc / (1024 * 1024)));
      fflush(stdout);
      nSeq = 0;
      recusado = 1;
      return;
    } }
  gif_js_seq_iniciar(nSeq, telaW, telaH);
  for (i = 0; i < nSeq; i++) {
    size_t prec = gif_montar(b, n, &seq[i], NULL, 0);
    if (!prec) continue;
    if (prec > cap) {
      unsigned char *novo = (unsigned char *)realloc(tmp, prec);
      if (!novo) break;
      tmp = novo; cap = prec;
    }
    if (gif_montar(b, n, &seq[i], tmp, cap) != prec) continue;
    gif_js_seq_quadro(i, tmp, (int)prec, seq[i].esq, seq[i].topo,
                      seq[i].larg, seq[i].alt, seq[i].descarte);
  }
  free(tmp);
  iSeq = 0;
  proxTroca = 0.0;
  ultimaTroca = 0.0;
  avisouFila = 0;
  inicioVolta = 0.0;
  contouVolta = 0;
  printf("[gif] %d quadros %dx%d, primeiro passo %d ms: quem anima e o app\n",
         nSeq, telaW, telaH, seq[0].atraso);
  fflush(stdout);
}

GLuint gif_textura(const char *caminho, int largAlvo) {
  int w, h;
  unsigned char *px;
  if (!caminho || !caminho[0] || largAlvo < 8) return 0;
  ultimoUso = emscripten_get_now();
  if (!orcamento()) return 0;

  if (strcmp(preso, caminho)) {
    size_t n = 0;
    unsigned char *b = lerTudo(caminho, &n);
    if (!b) return 0;
    gif_js_seq_soltar();
    gif_js_soltar();
    nSeq = 0;
    recusado = 0;
    fatiar(b, n);
    // A <img> DE RESERVA SO ONDE NAO HA TETO. Ela guarda TODOS os quadros
    // decodificados na memoria do navegador (e o que o orcamento existe para
    // evitar); onde ha teto, GIF que o fatiamento nao entende fica parado.
    if (!nSeq && !recusado && orcamento() == GIF_SEM_TETO) gif_js_preparar(caminho, b, (int)n);
    free(b);
    snprintf(preso, sizeof preso, "%s", caminho);
    texW = texH = 0;
  }
  if (recusado) return 0;

  if (nSeq > 1) {
    double agora = emscripten_get_now();
    if (proxTroca <= 0.0) { proxTroca = agora + seq[iSeq].atraso; inicioVolta = agora; ultimaTroca = agora; }
    else if (agora >= proxTroca &&
             gif_pode_trocar(agora, proxTroca, ultimaTroca, artePendente())) {
      int prox = (iSeq + 1) % nSeq;
      // SO AVANCA PARA UM QUADRO JA DECODIFICADO. Trocar para um <img> que
      // ainda nao chegou seria um piscar; segurar o atual e invisivel.
      if (gif_js_seq_pronto(prox)) {
        iSeq = prox;
        ultimaTroca = agora;
        // Relogio ancorado no vencimento ANTERIOR, nao em `agora`: chegar
        // 20 ms atrasado num quadro nao empurra todos os seguintes. Se o
        // atraso acumulado passou de um quadro inteiro, realinha.
        proxTroca += seq[iSeq].atraso;
        if (proxTroca < agora) proxTroca = agora + seq[iSeq].atraso;
        if (!iSeq && !contouVolta) {
          contouVolta = 1;
          printf("[gif] deu a volta nos %d quadros em %.0f ms\n", nSeq, agora - inicioVolta);
          fflush(stdout);
        }
      }
    }
    w = largAlvo;
    h = (int)((double)telaH * largAlvo / telaW + 0.5);
    if (h < 1) h = 1;
    if (!tex) {
      glGenTextures(1, &tex);
      if (!tex) return 0;
    }
    glBindTexture(GL_TEXTURE_2D, tex);
    if (w != texW || h != texH) {
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    // Sessao no worker (ou nada, e cai no <img>). `w`/`h` sao o tamanho do
    // card na tela: e nesse tamanho que o worker devolve os bitmaps.
    gif_js_seq_worker(w, h);
    if (gif_js_seq_subir(iSeq, (int)tex, w, h, w == texW && h == texH, NV_GIF_ADIANTE)) {
      texW = w; texH = h;
      return tex;
    }
    return 0;
  }

  { int nw = gif_js_largura(), nh = gif_js_altura();
    if (nw < 1 || nh < 1) return 0;              // ainda carregando
    w = largAlvo;
    h = (int)((double)nh * largAlvo / nw + 0.5);
    if (h < 1) h = 1; }

  if (!tex) {
    glGenTextures(1, &tex);
    if (!tex) return 0;
  }
  glBindTexture(GL_TEXTURE_2D, tex);
  if (w != texW || h != texH) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }
  // GPU -> GPU. Se a WebGL recusar (contexto perdido, tabela GL ausente), cai
  // no caminho pela CPU — mais lento, mas anima.
  if (gif_js_subir((int)tex, w, h, w == texW && h == texH)) {
    texW = w; texH = h;
    return tex;
  }
  px = (unsigned char *)malloc((size_t)w * (size_t)h * 4);
  if (!px) return 0;
  if (!gif_js_quadro(px, w, h)) { free(px); return 0; }
  if (w != texW || h != texH) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    texW = w; texH = h;
  } else {
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px);
  }
  free(px);
  return tex;
}

void gif_parar(void) {
  if (!preso[0]) return;
  gif_js_seq_soltar();
  gif_js_soltar();
  preso[0] = 0;
  texW = texH = 0;
  nSeq = 0;
  iSeq = 0;
  proxTroca = 0.0;
  ultimaTroca = 0.0;
  avisouFila = 0;
  inicioVolta = 0.0;
  contouVolta = 0;
  recusado = 0;
}

// A textura `tex` NAO sai: e pequena (o tamanho do card) e quem chama guarda o
// nome dela entre amostras; apaga-la deixaria esse nome apontando para nada.
void gif_ocioso(void) {
  double agora;
  if (!preso[0]) return;
  agora = emscripten_get_now();
  if (agora - ultimoUso < NV_GIF_OCIOSO_MS) return;
  if (!recusado) {
    printf("[gif] ninguem desenha o GIF ha %.0f ms: animacao solta\n", agora - ultimoUso);
    fflush(stdout);
  }
  gif_parar();
}

#else   /* webOS e Mac: sem animacao. Ver o cabecalho de gif.h. */

GLuint gif_textura(const char *caminho, int largAlvo) {
  (void)caminho; (void)largAlvo;
  return 0;
}
void gif_parar(void) {}
void gif_ocioso(void) {}

#endif
