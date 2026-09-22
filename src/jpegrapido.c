#include "jpegrapido.h"
#include "sdlcompat.h"
#include <stdio.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
// ---------------------------------------------------------------- Tizen ----
//
// SOFTWARE DE NOVO, DE PROPOSITO (20/09/2026, #72, registro 10 do AU7000).
//
// A 1.3.2 mandou o decode ao navegador (createImageBitmap + canvas) e a 1.3.3
// levou isso a um Worker. Os dois engasgam a tela do mesmo jeito: o log
// mostra `swap=3279 ms` com CINCO fios vivos e ZERO arquivo — o fio principal
// preso no SwapWindow do WebGL enquanto artes decodificam. A leitura que
// fecha com tudo: canvas e getImageData nesta TV passam pelo processo de
// GPU, o MESMO que o nosso WebGL espera; cada readback de arte serializa
// com o quadro. A 1.0.26, que a pessoa lembra como "rapida", decodificava
// em software (IMG_Load) — arte lenta, tela fluida.
//
// Entao JPEG volta ao software, mas nao ao IMG_Load de tamanho cheio (13 s
// num 4K, #69): a libjpeg do port do Emscripten com scale_denom, o mesmo
// truque do ramo nativo abaixo. PNG e WebP vao ao navegador (webp.c):
// createImageBitmap + canvas devolve os pixels JA no tamanho pedido, o
// bitmap cheio fica fora do heap do WASM. Nao ha port de libpng nem libwebp
// no Emscripten aqui.
//
// PNG ENTROU NO NAVEGADOR EM 21/09/2026 (registro 1106, D1): um PNG de
// 3840x2160 do CDN de colecoes (cdn.jsdelivr.net/gh/luckynumb3rs/...) ia pelo
// IMG_Load de tamanho cheio — 33 MB no heap fixo de 256 MiB, decode de 1,5 a
// 3,5 s repetido 81 vezes na mesma sessao, tela travada enquanto isso. O
// navegador ja reduzia o WebP assim; faltava so o PNG entrar pela mesma
// ponte (navegador_decodificar, mime generico — o Worker so repassa ao
// Blob). Se o Worker nao subir, cai no IMG_Load_RW de sempre.
#include "webp.h"
#include <stdlib.h>
#include <setjmp.h>
#include <SDL2/SDL_image.h>
#include <jpeglib.h>

typedef struct { struct jpeg_error_mgr pub; jmp_buf salto; } ErroSalto;
static void erroSai(j_common_ptr cinfo) {
  ErroSalto *e = (ErroSalto *)cinfo->err;
  { char msg[JMSG_LENGTH_MAX];
    (*cinfo->err->format_message)(cinfo, msg);
    printf("[jpeg] %s\n", msg); fflush(stdout); }
  longjmp(e->salto, 1);
}
static void semSaida(j_common_ptr cinfo) { (void)cinfo; }

static SDL_Surface *jpegEscalado(const unsigned char *dados, size_t n, int largMax,
                                 int *larguraOriginal, int *alturaOriginal) {
  struct jpeg_decompress_struct cinfo;
  ErroSalto erro;
  SDL_Surface * volatile s = NULL;
  unsigned char * volatile linha = NULL;
  memset(&cinfo, 0, sizeof cinfo);
  cinfo.err = jpeg_std_error(&erro.pub);
  erro.pub.error_exit = erroSai;
  erro.pub.output_message = semSaida;
  if (setjmp(erro.salto)) {
    jpeg_destroy_decompress(&cinfo);
    free(linha);
    if (s) SDL_FreeSurface(s);
    return NULL;
  }
  jpeg_create_decompress(&cinfo);
  jpeg_mem_src(&cinfo, (unsigned char *)dados, (unsigned long)n);
  jpeg_read_header(&cinfo, TRUE);
  if (larguraOriginal) *larguraOriginal = (int)cinfo.image_width;
  if (alturaOriginal) *alturaOriginal = (int)cinfo.image_height;
  // A maior reducao que ainda cobre o pedido, igual ao ramo nativo.
  cinfo.scale_num = 1; cinfo.scale_denom = 1;
  if (largMax > 0)
    while (cinfo.scale_denom < 8 &&
           (int)((cinfo.image_width + cinfo.scale_denom * 2 - 1) / (cinfo.scale_denom * 2)) >= largMax)
      cinfo.scale_denom *= 2;
  // A libjpeg 9 do port nao tem JCS_EXT_RGBA (e da turbo): sai RGB e a linha
  // ganha o alfa aqui.
  cinfo.out_color_space = JCS_RGB;
  if (cinfo.scale_denom > 1) { cinfo.dct_method = JDCT_IFAST; cinfo.do_fancy_upsampling = FALSE; }
  jpeg_calc_output_dimensions(&cinfo);
  s = nv_superficie(0, (int)cinfo.output_width, (int)cinfo.output_height, 32, SDL_PIXELFORMAT_ABGR8888);
  linha = malloc((size_t)cinfo.output_width * 3);
  if (!s || !linha) { jpeg_destroy_decompress(&cinfo); free(linha); if (s) SDL_FreeSurface(s); return NULL; }
  jpeg_start_decompress(&cinfo);
  if (cinfo.output_components != 3) { jpeg_destroy_decompress(&cinfo); free(linha); SDL_FreeSurface(s); return NULL; }
  while (cinfo.output_scanline < cinfo.output_height) {
    unsigned char *dst = (unsigned char *)s->pixels + (size_t)cinfo.output_scanline * (size_t)s->pitch;
    JSAMPROW l = (JSAMPROW)linha;
    unsigned x;
    jpeg_read_scanlines(&cinfo, &l, 1);
    for (x = 0; x < cinfo.output_width; x++) {
      dst[x * 4] = linha[x * 3]; dst[x * 4 + 1] = linha[x * 3 + 1];
      dst[x * 4 + 2] = linha[x * 3 + 2]; dst[x * 4 + 3] = 255;
    }
  }
  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);
  free(linha);
  return s;
}

// PONTE COMUM PNG/WEBP: manda ao navegador (navegador_decodificar, em
// webp.c) e monta a SDL_Surface do jeito que os dois formatos precisam —
// mesma montagem, so muda o mime. NULL quando o Worker nao respondeu (nao
// subiu, ou os 8 s do futex estouraram): o chamador cai no decoder local.
static SDL_Surface *viaNavegador(const unsigned char *dados, size_t n, const char *mime, int largMax,
                                 int *larguraOriginal, int *alturaOriginal) {
  int w = 0, h = 0, ow = 0, oh = 0; uint8_t *px; SDL_Surface *s;
  px = navegador_decodificar(dados, n, mime, largMax > 0 ? largMax : 0, &w, &h, &ow, &oh);
  if (!px) return NULL;
  s = nv_superficie(0, w, h, 32, SDL_PIXELFORMAT_ABGR8888);
  if (s) { int y; for (y = 0; y < h; y++) memcpy((char *)s->pixels + y * s->pitch, px + (size_t)y * w * 4, (size_t)w * 4); }
  free(px);
  if (larguraOriginal) *larguraOriginal = ow > 0 ? ow : w;
  if (alturaOriginal) *alturaOriginal = oh > 0 ? oh : h;
  return s;
}

SDL_Surface *jpeg_rapido_carregar_mem(const unsigned char *dados, size_t n, int largMax,
                                      int *larguraOriginal, int *alturaOriginal) {
  if (larguraOriginal) *larguraOriginal = 0;
  if (alturaOriginal) *alturaOriginal = 0;
  if (!dados || n < 16 || n > 32L * 1024 * 1024) return NULL;
  if (dados[0] == 0xFF && dados[1] == 0xD8)
    return jpegEscalado(dados, n, largMax, larguraOriginal, alturaOriginal);
  if (dados[0] == 0x89 && dados[1] == 'P' && dados[2] == 'N' && dados[3] == 'G') {
    // Navegador primeiro (ja reduzido, fora do heap do WASM); IMG_Load_RW de
    // tamanho cheio so quando o Worker nao respondeu.
    SDL_Surface *s = viaNavegador(dados, n, "image/png", largMax, larguraOriginal, alturaOriginal);
    if (s) return s;
    { SDL_RWops *rw = SDL_RWFromConstMem(dados, (int)n);
      s = rw ? IMG_Load_RW(rw, 1) : NULL;
      if (s) { if (larguraOriginal) *larguraOriginal = s->w; if (alturaOriginal) *alturaOriginal = s->h; }
      return s; }
  }
  if (!memcmp(dados, "RIFF", 4) && !memcmp(dados + 8, "WEBP", 4))
    return viaNavegador(dados, n, "image/webp", largMax, larguraOriginal, alturaOriginal);
  return NULL;   // GIF e o resto: IMG_Load de sempre
}

SDL_Surface *jpeg_rapido_carregar(const char *caminho, int largMax,
                                  int *larguraOriginal, int *alturaOriginal) {
  FILE *f; long n; unsigned char *dados; SDL_Surface *s;
  if (larguraOriginal) *larguraOriginal = 0;
  if (alturaOriginal) *alturaOriginal = 0;
  if (!caminho) return NULL;
  f = fopen(caminho, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); rewind(f);
  if (n < 16 || n > 32L * 1024 * 1024) { fclose(f); return NULL; }
  dados = malloc((size_t)n);
  if (!dados || fread(dados, 1, (size_t)n, f) != (size_t)n) { free(dados); fclose(f); return NULL; }
  fclose(f);
  s = jpeg_rapido_carregar_mem(dados, (size_t)n, largMax, larguraOriginal, alturaOriginal);
  free(dados);
  return s;
}
#else

#include <dlfcn.h>
#include <setjmp.h>
#include <pthread.h>
#include <stddef.h>
// O CABECALHO CERTO PARA CADA BIBLIOTECA. Na TV a .so.62 tem o layout 6b, e
// o cabecalho vendorado diz JPEG_LIB_VERSION 62; no Mac a libjpeg do brew e
// API 8 e o cabecalho e o do proprio brew. Trocar um pelo outro da
// "Wrong JPEG library version" na jpeg_CreateDecompress — que este modulo
// pega e responde com NULL, sem derrubar nada.
#ifdef __APPLE__
#include <jpeglib.h>
#else
#include "vendor/jpeg62/jpeglib.h"
#endif

typedef struct jpeg_error_mgr *(*FnStdError)(struct jpeg_error_mgr *);
typedef void (*FnCreate)(j_decompress_ptr, int, size_t);
typedef void (*FnStdioSrc)(j_decompress_ptr, FILE *);
typedef int  (*FnReadHeader)(j_decompress_ptr, boolean);
typedef boolean (*FnStart)(j_decompress_ptr);
typedef JDIMENSION (*FnScanlines)(j_decompress_ptr, JSAMPARRAY, JDIMENSION);
typedef boolean (*FnFinish)(j_decompress_ptr);
typedef void (*FnDestroy)(j_decompress_ptr);
typedef void (*FnCalc)(j_decompress_ptr);

static struct {
  void *h;
  FnStdError stdError; FnCreate create; FnStdioSrc stdioSrc; FnReadHeader readHeader;
  FnStart start; FnScanlines scanlines; FnFinish finish; FnDestroy destroy; FnCalc calc;
  int tentado, ok;
} lib;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;

static void *sym(const char *n) { return lib.h ? dlsym(lib.h, n) : NULL; }

static int abrir(void) {
  pthread_mutex_lock(&trava);
  if (!lib.tentado) {
    static const char *NOMES[] = {
#ifdef __APPLE__
      "/opt/homebrew/lib/libjpeg.8.dylib", "libjpeg.8.dylib", "libjpeg.dylib",
#else
      "libjpeg.so.62", "/usr/lib/libjpeg.so.62",
#endif
      NULL };
    int i;
    lib.tentado = 1;
    for (i = 0; NOMES[i] && !lib.h; i++) lib.h = dlopen(NOMES[i], RTLD_NOW | RTLD_LOCAL);
    if (lib.h) {
      lib.stdError   = (FnStdError)sym("jpeg_std_error");
      lib.create     = (FnCreate)sym("jpeg_CreateDecompress");
      lib.stdioSrc   = (FnStdioSrc)sym("jpeg_stdio_src");
      lib.readHeader = (FnReadHeader)sym("jpeg_read_header");
      lib.start      = (FnStart)sym("jpeg_start_decompress");
      lib.scanlines  = (FnScanlines)sym("jpeg_read_scanlines");
      lib.finish     = (FnFinish)sym("jpeg_finish_decompress");
      lib.destroy    = (FnDestroy)sym("jpeg_destroy_decompress");
      lib.calc       = (FnCalc)sym("jpeg_calc_output_dimensions");
      lib.ok = lib.stdError && lib.create && lib.stdioSrc && lib.readHeader &&
               lib.start && lib.scanlines && lib.finish && lib.destroy && lib.calc;
    }
    printf("[jpeg] decode escalado: %s\n", lib.ok ? "libjpeg do sistema aberta" : "sem libjpeg, fica o IMG_Load");
    fflush(stdout);
  }
  pthread_mutex_unlock(&trava);
  return lib.ok;
}

// ERRO DA libjpeg E longjmp, como manda o manual: error_exit nao pode voltar.
// O jmp_buf mora junto do cinfo, um por decode, entao dois fios de decode nao
// se atropelam.
typedef struct { struct jpeg_error_mgr pub; jmp_buf salto; } ErroSalto;
static void erroSai(j_common_ptr cinfo) {
  ErroSalto *e = (ErroSalto *)cinfo->err;
  // A mensagem fica no log uma vez por arquivo: e a unica pista de que uma
  // arte caiu no caminho lento. Sem credencial nenhuma numa url de arte.
  { char msg[JMSG_LENGTH_MAX];
    (*cinfo->err->format_message)(cinfo, msg);
    printf("[jpeg] %s\n", msg); fflush(stdout); }
  longjmp(e->salto, 1);
}
static void semSaida(j_common_ptr cinfo) { (void)cinfo; }   // avisos: silencio

SDL_Surface *jpeg_rapido_carregar(const char *caminho, int largMax,
                                  int *larguraOriginal, int *alturaOriginal) {
  struct jpeg_decompress_struct cinfo;
  ErroSalto erro;
  FILE *f;
  SDL_Surface * volatile s = NULL;   /* volatile: sobrevive ao longjmp */
  unsigned char magia[2];
  if (larguraOriginal) *larguraOriginal = 0;
  if (alturaOriginal) *alturaOriginal = 0;
  if (!caminho || largMax < 1) return NULL;
  if (!abrir()) return NULL;
  f = fopen(caminho, "rb");
  if (!f) return NULL;
  // So JPEG entra: PNG e WebP seguem pelos leitores de sempre.
  if (fread(magia, 1, 2, f) != 2 || magia[0] != 0xFF || magia[1] != 0xD8) { fclose(f); return NULL; }
  rewind(f);

  memset(&cinfo, 0, sizeof cinfo);
  cinfo.err = lib.stdError(&erro.pub);
  erro.pub.error_exit = erroSai;
  erro.pub.output_message = semSaida;
  if (setjmp(erro.salto)) {
    lib.destroy(&cinfo);
    fclose(f);
    if (s) SDL_FreeSurface(s);
    return NULL;
  }
  lib.create(&cinfo, JPEG_LIB_VERSION, sizeof(struct jpeg_decompress_struct));
  lib.stdioSrc(&cinfo, f);
  lib.readHeader(&cinfo, TRUE);
  if (larguraOriginal) *larguraOriginal = (int)cinfo.image_width;
  if (alturaOriginal) *alturaOriginal = (int)cinfo.image_height;

  // A MAIOR REDUCAO QUE AINDA COBRE O PEDIDO: 1/2 enquanto a metade for >=
  // largMax, e assim por diante ate 1/8. Um 1920 pedido a 736 sai 960; pedido
  // a 1920 sai inteiro; um 500 pedido a 256 sai inteiro (250 nao cobriria).
  cinfo.scale_num = 1; cinfo.scale_denom = 1;
  while (cinfo.scale_denom < 8 &&
         (int)((cinfo.image_width + cinfo.scale_denom * 2 - 1) / (cinfo.scale_denom * 2)) >= largMax)
    cinfo.scale_denom *= 2;
  // Bytes R,G,B,A na memoria = SDL_PIXELFORMAT_ABGR8888, o formato de toda
  // textura do app: nada a converter depois. JCS_EXT_RGBA existe na turbo
  // desde a 1.1 — a .so.62 da TV e 1.5.
  cinfo.out_color_space = JCS_EXT_RGBA;
  // Mais barato onde nao se ve: o DCT rapido e o upsampling simples custam
  // menos e a diferenca some numa arte que ainda vai ser reduzida. Em escala
  // 1:1 (o heroi de 1920) fica o decode de sempre, pixel por pixel.
  if (cinfo.scale_denom > 1) { cinfo.dct_method = JDCT_IFAST; cinfo.do_fancy_upsampling = FALSE; }
  lib.calc(&cinfo);

  s = nv_superficie(0, (int)cinfo.output_width, (int)cinfo.output_height, 32,
                    SDL_PIXELFORMAT_ABGR8888);
  if (!s) { lib.destroy(&cinfo); fclose(f); return NULL; }
  lib.start(&cinfo);
  if (cinfo.output_components != 4) {   // a biblioteca recusou o RGBA: nao arrisca
    lib.destroy(&cinfo); fclose(f); SDL_FreeSurface(s); return NULL;
  }
  while (cinfo.output_scanline < cinfo.output_height) {
    JSAMPROW linha = (JSAMPROW)((unsigned char *)s->pixels + (size_t)cinfo.output_scanline * (size_t)s->pitch);
    lib.scanlines(&cinfo, &linha, 1);
  }
  lib.finish(&cinfo);
  lib.destroy(&cinfo);
  fclose(f);
  return s;
}
#endif
