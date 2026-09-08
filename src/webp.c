#include "webp.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Dois decodificadores atras da mesma porta. Ambos entregam um bloco RGBA
// recem-alocado (bytes R,G,B,A na memoria) e dizem o tamanho; quem monta a
// SDL_Surface e webp_carregar la embaixo, uma vez so.
static uint8_t *decodificar(const unsigned char *dados, size_t n, int *lw, int *lh);
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
// COMO A PONTE FUNCIONA. O fio de decode monta um bloco de 4 int32 no heap
// (estado, largura, altura, ponteiro), manda um pedaco de JS para o FIO
// PRINCIPAL e dorme num futex. O fio principal so DISPARA o trabalho e volta
// para o laco de quadro: createImageBitmap decodifica fora da thread de UI,
// e no `then` o resultado passa por um canvas 2D, vira RGBA, e um Atomics
// acorda o fio de decode. O quadro perde so o drawImage/getImageData de um
// poster, sub-milissegundo.
//
// POR QUE NAO ASYNCIFY AQUI, que seria mais curto: o laco de quadro em main.c
// ja desenrola por asyncify a cada rAF, e por-lo tambem nos fios de decode
// significaria confiar no unwinding dentro de Worker de pthread para uma
// operacao que acontece centenas de vezes por sessao. O futex nao depende de
// nada disso.
//
// POR QUE O FIO PRINCIPAL E NAO O PROPRIO WORKER: no worker faltaria
// `document`, e seria OffscreenCanvas — que existe no Chromium 76 da TV, mas
// e mais superficie de risco num aparelho que eu nao tenho na bancada. O fio
// principal tem DOM de verdade.
#include <emscripten.h>
#include <emscripten/threading.h>
#include <errno.h>

static int jaContou;

static uint8_t *decodificar(const unsigned char *dados, size_t n, int *lw, int *lh) {
  int32_t *job;
  int esperou = 0, w, h;
  uint8_t *px;

  // Chamado do fio principal nao da: ele nao pode bloquear em Atomics.wait, e
  // se pudesse seria ele mesmo quem deixaria de rodar o `then`. Hoje o unico
  // chamador e o fio de decode do tex_cache; esta guarda existe para que
  // amanha isto vire NULL em vez de travar o app.
  if (emscripten_is_main_browser_thread()) return NULL;

  // NO HEAP, e nao na pilha desta funcao. Se o navegador estourar o prazo la
  // embaixo, esta funcao volta mas o `then` do JS pode escrever DEPOIS — numa
  // pilha ja reaproveitada isso e corrupcao silenciosa. Vazar 16 bytes num
  // caso que nao deve acontecer custa menos.
  job = (int32_t *)calloc(4, sizeof(int32_t));
  if (!job) return NULL;

  MAIN_THREAD_ASYNC_EM_ASM({
    // UMA DECLARACAO POR LINHA, sem `var a = 1, b = 2`: o bloco do EM_ASM
    // passa pelo pre-processador de C, e la chave nao protege virgula — so
    // parentese protege. Uma virgula solta aqui parte o bloco em dois
    // argumentos de macro e o build morre em "undeclared identifier '$1'".
    var pJob = $0 >> 2;
    var pDados = $1;
    var n = $2;
    // `slice` (e nao `subarray`) de proposito: copia para um ArrayBuffer
    // comum. O Blob nao aceita vista sobre SharedArrayBuffer, e a copia
    // tambem desprende o dado da vida do buffer em C.
    var bytes = HEAPU8.slice(pDados, pDados + n);
    var fim = function (ptr, w, h) {
      HEAP32[pJob + 1] = w;
      HEAP32[pJob + 2] = h;
      HEAP32[pJob + 3] = ptr;
      // seq-cst: publica os tres campos acima antes do estado virar 1.
      Atomics.store(HEAP32, pJob, 1);
      Atomics.notify(HEAP32, pJob);
    };
    try {
      createImageBitmap(new Blob([bytes], { type: 'image/webp' })).then(function (bmp) {
        var w = bmp.width;
        var h = bmp.height;
        var ptr = 0;
        if (w > 0 && h > 0) {
          var cv = document.createElement('canvas');
          cv.width = w; cv.height = h;
          var cx = cv.getContext('2d');
          cx.drawImage(bmp, 0, 0);
          var d = cx.getImageData(0, 0, w, h).data;
          ptr = _malloc(w * h * 4);
          if (ptr) HEAPU8.set(d, ptr);
        }
        if (bmp.close) bmp.close();
        fim(ptr, ptr ? w : 0, ptr ? h : 0);
      }).catch(function () { fim(0, 0, 0); });
    } catch (e) { fim(0, 0, 0); }
  }, (int)(intptr_t)job, (int)(intptr_t)dados, (int)n);

  while (__atomic_load_n(&job[0], __ATOMIC_ACQUIRE) == 0) {
    // Fatias de 250 ms em vez de uma espera longa: o -EWOULDBLOCK da corrida
    // (o JS terminou antes de chegarmos aqui) volta pelo `while`, e um prazo
    // curto mantem o teto de 8 s legivel.
    if (emscripten_futex_wait(&job[0], 0, 250.0) == -ETIMEDOUT) {
      esperou += 250;
      if (esperou >= 8000) {
        printf("[webp] navegador nao respondeu em 8 s; bloco de 16 B vazado\n");
        return NULL;   // job fica vivo de proposito, ver acima
      }
    }
  }

  w  = job[1];
  h  = job[2];
  px = (uint8_t *)(intptr_t)job[3];
  free(job);
  if (!px || w < 1 || h < 1) { free(px); return NULL; }
  if (!jaContou) {
    jaContou = 1;
    printf("[webp] navegador decodificou o primeiro: %dx%d\n", w, h);
  }
  *lw = w; *lh = h;
  return px;
}

static void soltar(uint8_t *px) { free(px); }   // o _malloc do JS e este malloc

#else
// -------------------------------------------------------- webOS e Mac ------
#include <dlfcn.h>

typedef int      (*FnInfo)(const uint8_t *, size_t, int *, int *);
typedef uint8_t *(*FnRgba)(const uint8_t *, size_t, int *, int *);
typedef void     (*FnFree)(void *);
static FnInfo pInfo; static FnRgba pRgba; static FnFree pFree;
static int tentado;

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
}

static uint8_t *decodificar(const unsigned char *dados, size_t n, int *lw, int *lh) {
  int w = 0, h = 0; uint8_t *px;
  if (!tentado) abrir();
  if (!pRgba || !pInfo(dados, n, &w, &h) || w < 1 || h < 1) return NULL;
  px = pRgba(dados, n, &w, &h);
  if (!px) return NULL;
  *lw = w; *lh = h;
  return px;
}

static void soltar(uint8_t *px) { if (pFree) pFree(px); else free(px); }
#endif

SDL_Surface *webp_carregar(const char *caminho) {
  FILE *f; long n; unsigned char *dados; int w = 0, h = 0; uint8_t *px; SDL_Surface *s;
  if (!caminho) return NULL;
  f = fopen(caminho, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); rewind(f);
  if (n < 16 || n > 32L * 1024 * 1024) { fclose(f); return NULL; }
  dados = malloc((size_t)n);
  if (!dados || fread(dados, 1, (size_t)n, f) != (size_t)n) { free(dados); fclose(f); return NULL; }
  fclose(f);
  if (memcmp(dados, "RIFF", 4) || memcmp(dados + 8, "WEBP", 4)) { free(dados); return NULL; }
  px = decodificar(dados, (size_t)n, &w, &h);
  free(dados);
  if (!px) return NULL;
  // ABGR8888 no SDL = bytes R,G,B,A na memoria em little-endian, que e o que
  // os dois decodificadores entregam. Copia para uma superficie propria: a do
  // SDL_..From apontaria para memoria de fora.
  s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ABGR8888);
  if (s) {
    int y;
    for (y = 0; y < h; y++) memcpy((char *)s->pixels + y * s->pitch, px + (size_t)y * w * 4, (size_t)w * 4);
  }
  soltar(px);
  return s;
}
