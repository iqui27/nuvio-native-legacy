#include "webp.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sdlcompat.h"

// Dois decodificadores atras da mesma porta. Ambos entregam um bloco RGBA
// recem-alocado (bytes R,G,B,A na memoria) e dizem o tamanho; quem monta a
// SDL_Surface e webp_carregar la embaixo, uma vez so.
static uint8_t *decodificar(const unsigned char *dados, size_t n, int largMax,
                            int *lw, int *lh, int *ow, int *oh);
static void     soltar(uint8_t *px);

#ifdef __EMSCRIPTEN__
// ---------------------------------------------------------------- Tizen ----
//
// O NAVEGADOR JA SABE LER WEBP; o problema e que ele so sabe de forma
// assincrona, e este codigo roda no fio de decode do tex_cache, que e
// sincrono. O Emscripten nao tem port de libwebp (ver tools/tizen.sh) e o
// dlopen do outro ramo nao existe em WASM — ele compila, devolve NULL sempre,
// e era exatamente esse silencio que a TV do dono mostrava:
//
//   [tex] decode falhou (Unsupported image format) tam=14312 magica=52494646
//
// 52494646 e "RIFF": WebP puro, vindo do addon de posters.
//
// COMO A PONTE FUNCIONA. O fio de decode monta um bloco de 8 int32 no heap
// (estado, largura, altura, ponteiro, tamanho original, origem), manda um
// pedaco de JS para o FIO PRINCIPAL e dorme num futex. O fio principal so
// REPASSA o pedido a um Worker proprio (tools/decodificador.js) e volta para o
// laco de quadro. La, createImageBitmap decodifica, um OffscreenCanvas reduz,
// o worker pede a este fio um bloco do tamanho certo (estado 2 -> malloc
// aqui -> estado 3), escreve os pixels direto na memoria compartilhada e
// acorda o futex (estado 1). O fio principal nao toca em pixel nenhum.
//
// POR QUE SAIU DO FIO PRINCIPAL (#72, AU7000, 1.3.2): a versao anterior fazia
// drawImage + getImageData no `then`, no fio principal, e no AU7000 cada arte
// custava ate 1 s de laco de quadro parado (`swap=1004` no [quadro], FPS=2
// enquanto a fileira carregava, e o app caindo no meio). No Mac isso media
// sub-milissegundo e por isso a nota antiga chamava de barato.
//
// O CAMINHO PELO FIO PRINCIPAL CONTINUA COMO RESERVA: e o que roda se o Worker
// nao subir (Chromium sem OffscreenCanvas, arquivo nao servido). O pedido em
// voo e devolvido a ele pelo onerror, em vez de vencer os 8 s aqui.
//
// POR QUE NAO ASYNCIFY AQUI, que seria mais curto: o laco de quadro em main.c
// ja desenrola por asyncify a cada rAF, e por-lo tambem nos fios de decode
// significaria confiar no unwinding dentro de Worker de pthread para uma
// operacao que acontece centenas de vezes por sessao. O futex nao depende de
// nada disso.
//
// POR QUE UM WORKER PROPRIO E NAO O PTHREAD DE DECODE: o pthread esta
// bloqueado no futex, e a promessa do createImageBitmap so resolveria quando
// ele voltasse ao laco de eventos — nunca. O worker proprio nao roda C; so
// espera pedidos.
#include <emscripten.h>
#include <emscripten/threading.h>
#include <errno.h>

static int jaContou;

uint8_t *navegador_decodificar(const unsigned char *dados, size_t n, const char *mime,
                               int largMax, int *lw, int *lh, int *ow, int *oh) {
  int32_t *job;
  int esperou = 0, w, h, noWorker;
  uint8_t *px;
  if (ow) *ow = 0;
  if (oh) *oh = 0;

  // Chamado do fio principal nao da: ele nao pode bloquear em Atomics.wait, e
  // se pudesse seria ele mesmo quem deixaria de rodar o `then`. Hoje o unico
  // chamador e o fio de decode do tex_cache; esta guarda existe para que
  // amanha isto vire NULL em vez de travar o app.
  if (emscripten_is_main_browser_thread()) return NULL;

  // NO HEAP, e nao na pilha desta funcao. Se o navegador estourar o prazo la
  // embaixo, esta funcao volta mas o `then` do JS pode escrever DEPOIS — numa
  // pilha ja reaproveitada isso e corrupcao silenciosa. Vazar 16 bytes num
  // caso que nao deve acontecer custa menos.
  job = (int32_t *)calloc(8, sizeof(int32_t));
  if (!job) return NULL;

  MAIN_THREAD_ASYNC_EM_ASM({
    // UMA DECLARACAO POR LINHA, sem `var a = 1, b = 2`: o bloco do EM_ASM
    // passa pelo pre-processador de C, e la chave nao protege virgula — so
    // parentese protege. Uma virgula solta aqui parte o bloco em dois
    // argumentos de macro e o build morre em "undeclared identifier '$1'".
    var pJob = $0 >> 2;
    var pDados = $1;
    var n = $2;
    var mime = UTF8ToString($3);
    var largMax = $4;
    var fim = function (ptr, w, h, ow, oh) {
      HEAP32[pJob + 1] = w;
      HEAP32[pJob + 2] = h;
      HEAP32[pJob + 3] = ptr;
      HEAP32[pJob + 4] = ow;
      HEAP32[pJob + 5] = oh;
      // seq-cst: publica os tres campos acima antes do estado virar 1.
      Atomics.store(HEAP32, pJob, 1);
      Atomics.notify(HEAP32, pJob);
    };
    // CAMINHO ANTIGO, no fio principal. Fica como reserva: e o que roda
    // quando o Worker nao sobe (sem OffscreenCanvas, arquivo ausente, CSP).
    var noFioPrincipal = function () {
      HEAP32[pJob + 6] = 0;
      // `slice` (e nao `subarray`) de proposito: copia para um ArrayBuffer
      // comum. O Blob nao aceita vista sobre SharedArrayBuffer, e a copia
      // tambem desprende o dado da vida do buffer em C.
      var bytes = HEAPU8.slice(pDados, pDados + n);
      try {
        createImageBitmap(new Blob([bytes], { type: mime })).then(function (bmp) {
          var ow = bmp.width;
          var oh = bmp.height;
          var w = ow;
          var h = oh;
          var ptr = 0;
          // REDUZ NO CANVAS, nao no heap: o bitmap inteiro vive na memoria do
          // navegador; so o tamanho pedido atravessa para o WASM. Um fundo de
          // 3840x2160 pedido a 1280 custa 3,7 MB no heap em vez de 33.
          if (largMax > 0 && ow > largMax) {
            w = largMax;
            h = Math.max(1, Math.round(oh * largMax / ow));
          }
          if (w > 0 && h > 0) {
            var cv = document.createElement('canvas');
            cv.width = w; cv.height = h;
            var cx = cv.getContext('2d');
            cx.imageSmoothingEnabled = true;
            if ('imageSmoothingQuality' in cx) cx.imageSmoothingQuality = 'high';
            cx.drawImage(bmp, 0, 0, w, h);
            var d = cx.getImageData(0, 0, w, h).data;
            ptr = _malloc(w * h * 4);
            if (ptr) HEAPU8.set(d, ptr);
          }
          if (bmp.close) bmp.close();
          fim(ptr, ptr ? w : 0, ptr ? h : 0, ow, oh);
        }).catch(function () { fim(0, 0, 0, 0, 0); });
      } catch (e) { fim(0, 0, 0, 0, 0); }
    };
    // CAMINHO NOVO (#72): um Worker proprio, tools/decodificador.js, faz o
    // decode, a reducao e a copia para o heap. O fio principal so repassa o
    // pedido. `Module.nvDec` guarda o worker e os pedidos em voo, para que um
    // worker que morre (arquivo nao servido, OffscreenCanvas ausente no
    // Chromium velho) devolva cada pedido ao caminho antigo em vez de
    // deixa-lo vencer os 8 s no C.
    var D = Module.nvDec;
    if (D === undefined) {
      // Sem virgula em nivel de chave (ver a nota do EM_ASM la em cima).
      D = {};
      D.w = null;
      D.morto = false;
      D.voo = {};
      Module.nvDec = D;
      if (typeof OffscreenCanvas === 'undefined' || typeof Worker === 'undefined') D.morto = true;
      else {
        try {
          D.w = new Worker('decodificador.js');
          D.w.postMessage({ memoria: wasmMemory.buffer });
          D.w.onerror = function (e) {
            D.morto = true;
            console.log('[webp] decodificador.js nao subiu (' + (e && e.message ? e.message : '?') + '); decode volta ao fio principal');
            var k;
            for (k in D.voo) { if (D.voo.hasOwnProperty(k)) { var f = D.voo[k]; delete D.voo[k]; f(); } }
          };
        } catch (e) { D.morto = true; }
      }
    }
    if (D.morto || !D.w) { noFioPrincipal(); }
    else {
      D.voo[pJob] = noFioPrincipal;
      HEAP32[pJob + 6] = 1;
      D.w.postMessage(({ job: $0, dados: pDados, n: n, mime: mime, largMax: largMax }));
    }
  }, (int)(intptr_t)job, (int)(intptr_t)dados, (int)n, (int)(intptr_t)mime, (int)largMax);

  // Estados de job[0]: 0 em aberto; 2 o Worker decodificou e pede um bloco de
  // job[1]*job[2]*4 bytes (so este fio sabe fazer malloc); 3 bloco entregue em
  // job[3]; 1 terminado. O caminho pelo fio principal faz o malloc ele mesmo e
  // vai de 0 a 1 direto. Ver tools/decodificador.js.
  for (;;) {
    int32_t est = __atomic_load_n(&job[0], __ATOMIC_ACQUIRE);
    if (est == 1) break;
    if (est == 2) {
      size_t tam = (size_t)job[1] * (size_t)job[2] * 4;
      job[3] = (int32_t)(intptr_t)(tam ? malloc(tam) : NULL);
      __atomic_store_n(&job[0], 3, __ATOMIC_RELEASE);
      emscripten_futex_wake(&job[0], 1);
      continue;
    }
    // Fatias de 250 ms em vez de uma espera longa: o -EWOULDBLOCK da corrida
    // (o JS terminou antes de chegarmos aqui) volta pelo laco, e um prazo
    // curto mantem o teto de 8 s legivel.
    if (emscripten_futex_wait(&job[0], est, 250.0) == -ETIMEDOUT) {
      esperou += 250;
      if (esperou >= 8000) {
        printf("[webp] navegador nao respondeu em 8 s; bloco de 32 B vazado\n");
        return NULL;   // job fica vivo de proposito, ver acima
      }
    }
  }

  w  = job[1];
  h  = job[2];
  px = (uint8_t *)(intptr_t)job[3];
  if (ow) *ow = job[4];
  if (oh) *oh = job[5];
  noWorker = job[6];
  free(job);
  if (!px || w < 1 || h < 1) { free(px); return NULL; }
  if (!jaContou) {
    jaContou = 1;
    printf("[webp] navegador decodificou o primeiro: %dx%d (%s), %s\n", w, h, mime,
           noWorker ? "no worker" : "no fio principal");
  }
  *lw = w; *lh = h;
  return px;
}

static uint8_t *decodificar(const unsigned char *dados, size_t n, int largMax,
                            int *lw, int *lh, int *ow, int *oh) {
  return navegador_decodificar(dados, n, "image/webp", largMax, lw, lh, ow, oh);
}

static void soltar(uint8_t *px) { free(px); }   // o _malloc do JS e este malloc

#else
// -------------------------------------------------------- webOS e Mac ------
#include <dlfcn.h>

typedef int      (*FnInfo)(const uint8_t *, size_t, int *, int *);
typedef uint8_t *(*FnRgba)(const uint8_t *, size_t, int *, int *);
typedef void     (*FnFree)(void *);

// A API AVANCADA da libwebp (decode.h), so o que a escala precisa. As structs
// sao copiadas do cabecalho porque o alvo LG nao tem os headers no toolchain —
// e a lib entra por dlopen justamente por isso. O layout e estavel desde o ABI
// 0x0200 (os `pad[]` existem para isso), e WebPInitDecoderConfigInternal so
// recusa quando o byte MAIOR da versao difere: 0x0209 casa com a libwebp 0.5
// da TV e com a 1.x do Mac. Se recusar, o caminho antigo (tamanho cheio +
// tex_reduzir) continua valendo — nada quebra, so gasta mais.
//
// POR QUE ESCALAR DENTRO DO DECODER (19/09/2026): os fundos do Xperience
// passaram a vir em 3840x2160 (~180 KB cada, 17 estilos por marca). Em
// WebPDecodeRGBA um deles e 33 MB de RGBA em tamanho cheio ANTES da reducao
// para os 1920 do heroi — na C9 de 2 GB e um pico por pasta focada, e na
// Samsung e um oitavo do heap fixo. Com use_scaling a libwebp reduz linha a
// linha e o bloco que nasce ja tem o tamanho pedido: 8 MB para 1920.
typedef struct { int width, height, has_alpha, has_animation, format; uint32_t pad[5]; } WpFeatures;
typedef struct {
  int colorspace;                // 1 = MODE_RGBA
  int width, height;
  int is_external_memory;
  union {
    struct { uint8_t *rgba; int stride; size_t size; } RGBA;
    struct { uint8_t *y, *u, *v, *a; int y_stride, u_stride, v_stride, a_stride;
             size_t y_size, u_size, v_size, a_size; } YUVA;
  } u;
  uint32_t pad[4];
  uint8_t *private_memory;
} WpBuffer;
typedef struct {
  int bypass_filtering, no_fancy_upsampling, use_cropping;
  int crop_left, crop_top, crop_width, crop_height;
  int use_scaling, scaled_width, scaled_height;
  int use_threads, dithering_strength, flip, alpha_dithering_strength;
  uint32_t pad[5];
} WpOptions;
typedef struct { WpFeatures input; WpBuffer output; WpOptions options; } WpConfig;
#define WP_ABI 0x0209
#define WP_MODE_RGBA 1
typedef int  (*FnCfgInit)(WpConfig *, int);
typedef int  (*FnDecode)(const uint8_t *, size_t, WpConfig *);
typedef void (*FnFreeBuf)(WpBuffer *);

static FnInfo pInfo; static FnRgba pRgba; static FnFree pFree;
static FnCfgInit pCfgInit; static FnDecode pDecode; static FnFreeBuf pFreeBuf;
static int tentado, escalaAvisada;

static void abrir(void) {
  static const char *nomes[] = { "libwebp.so.7", "libwebp.so", "libwebp.7.dylib", "/opt/homebrew/lib/libwebp.7.dylib", NULL };
  void *h = NULL; int i;
  tentado = 1;
  for (i = 0; nomes[i] && !h; i++) h = dlopen(nomes[i], RTLD_NOW);
  if (!h) { printf("[webp] libwebp ausente; .webp nao vai decodificar\n"); return; }
  pInfo = (FnInfo)dlsym(h, "WebPGetInfo");
  pRgba = (FnRgba)dlsym(h, "WebPDecodeRGBA");
  pFree = (FnFree)dlsym(h, "WebPFree");   // ausente em libwebp antiga: free() serve
  if (!pInfo || !pRgba) { printf("[webp] libwebp sem WebPDecodeRGBA\n"); pInfo = NULL; pRgba = NULL; }
  pCfgInit = (FnCfgInit)dlsym(h, "WebPInitDecoderConfigInternal");
  pDecode  = (FnDecode)dlsym(h, "WebPDecode");
  pFreeBuf = (FnFreeBuf)dlsym(h, "WebPFreeDecBuffer");
  if (!pCfgInit || !pDecode || !pFreeBuf) {
    printf("[webp] libwebp sem API avancada: WebP grande decodifica em tamanho cheio\n");
    pCfgInit = NULL;
  }
}

// Decodifica ja reduzido a `lw` de largura. NULL quando a API avancada nao
// existe ou recusou (versao, memoria): o chamador cai no tamanho cheio.
static uint8_t *decodificarEscalado(const unsigned char *dados, size_t n, int w, int h, int lw, int *lh) {
  WpConfig cfg;
  uint8_t *px;
  size_t linha = (size_t)lw * 4, y;
  int alto = (int)(((long)h * lw + w / 2) / w);
  if (alto < 1) alto = 1;
  if (!pCfgInit) return NULL;
  memset(&cfg, 0, sizeof cfg);
  if (!pCfgInit(&cfg, WP_ABI)) {
    if (!escalaAvisada) { escalaAvisada = 1; printf("[webp] libwebp recusou o ABI %#x: sem escala no decoder\n", WP_ABI); }
    pCfgInit = NULL;
    return NULL;
  }
  cfg.options.use_scaling   = 1;
  cfg.options.scaled_width  = lw;
  cfg.options.scaled_height = alto;
  cfg.output.colorspace     = WP_MODE_RGBA;
  if (pDecode(dados, n, &cfg) != 0 || !cfg.output.u.RGBA.rgba) { pFreeBuf(&cfg.output); return NULL; }
  // Copia para um bloco proprio, contiguo, que `soltar` libera com free():
  // o da libwebp tem stride proprio e so WebPFreeDecBuffer sabe solta-lo.
  px = malloc(linha * (size_t)cfg.output.height);
  if (px)
    for (y = 0; y < (size_t)cfg.output.height; y++)
      memcpy(px + y * linha, cfg.output.u.RGBA.rgba + y * (size_t)cfg.output.u.RGBA.stride, linha);
  *lh = cfg.output.height;
  pFreeBuf(&cfg.output);
  return px;
}

static uint8_t *decodificar(const unsigned char *dados, size_t n, int largMax,
                            int *lw, int *lh, int *ow, int *oh) {
  int w = 0, h = 0; uint8_t *px;
  if (!tentado) abrir();
  if (!pRgba || !pInfo(dados, n, &w, &h) || w < 1 || h < 1) return NULL;
  if (ow) *ow = w;
  if (oh) *oh = h;
  if (largMax > 0 && w > largMax) {
    px = decodificarEscalado(dados, n, w, h, largMax, lh);
    if (px) { *lw = largMax; return px; }
  }
  px = pRgba(dados, n, &w, &h);
  if (!px) return NULL;
  *lw = w; *lh = h;
  return px;
}

// O bloco da API avancada ja foi copiado para malloc; o do WebPDecodeRGBA e
// da lib. Os dois passam por aqui: WebPFree e free() sao o mesmo alocador na
// libwebp, e e por isso que a copia acima pode usar malloc.
static void soltar(uint8_t *px) { if (pFree) pFree(px); else free(px); }
#endif

SDL_Surface *webp_carregar_larg(const char *caminho, int largMax, int *ow, int *oh) {
  FILE *f; long n; unsigned char *dados; int w = 0, h = 0; uint8_t *px; SDL_Surface *s;
  if (ow) *ow = 0;
  if (oh) *oh = 0;
  if (!caminho) return NULL;
  f = fopen(caminho, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); rewind(f);
  if (n < 16 || n > 32L * 1024 * 1024) { fclose(f); return NULL; }
  dados = malloc((size_t)n);
  if (!dados || fread(dados, 1, (size_t)n, f) != (size_t)n) { free(dados); fclose(f); return NULL; }
  fclose(f);
  if (memcmp(dados, "RIFF", 4) || memcmp(dados + 8, "WEBP", 4)) { free(dados); return NULL; }
  px = decodificar(dados, (size_t)n, largMax, &w, &h, ow, oh);
  free(dados);
  if (!px) return NULL;
  // ABGR8888 no SDL = bytes R,G,B,A na memoria em little-endian, que e o que
  // os dois decodificadores entregam. Copia para uma superficie propria: a do
  // SDL_..From apontaria para memoria de fora.
  s = nv_superficie(0, w, h, 32, SDL_PIXELFORMAT_ABGR8888);
  if (s) {
    int y;
    for (y = 0; y < h; y++) memcpy((char *)s->pixels + y * s->pitch, px + (size_t)y * w * 4, (size_t)w * 4);
  }
  soltar(px);
  return s;
}

SDL_Surface *webp_carregar(const char *caminho) {
  return webp_carregar_larg(caminho, 0, NULL, NULL);
}
