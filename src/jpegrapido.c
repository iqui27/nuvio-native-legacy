#include "jpegrapido.h"
#include "sdlcompat.h"
#include <stdio.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
// NO TIZEN O DECODIFICADOR E O DO NAVEGADOR — nativo, fora do heap do WASM, e
// ja reduzido no canvas ao tamanho pedido (webp.c, navegador_decodificar).
// Antes este alvo caia no IMG_Load em software: MEDIDO nos logs do #69 (AU7000,
// Tizen 6.0), "decode lento: 13103 ms (ler 10946)" para um 3840x2160 e
// "livre-no-heap=6.7 MiB" — o mesmo heap de 256 MiB que o player precisa, e o
// que casa com os crashes do #68 durante a reproducao.
#include "webp.h"
#include <stdlib.h>
// DA MEMORIA, sem arquivo. E o caminho do alvo Tizen desde 20/09/2026 (#72):
// toda chamada de arquivo feita por um pthread e proxiada ao fio principal,
// e gravar/ler/apagar cada arte no MEMFS custava tres viagens pelo fio que
// desenha. O fio de rede entrega os bytes direto ao de decode (tex_cache.c,
// Item.bruto) e este e o unico decodificador que eles alcancam.
SDL_Surface *jpeg_rapido_carregar_mem(const unsigned char *dados, size_t n, int largMax,
                                      int *larguraOriginal, int *alturaOriginal) {
  const char *mime = NULL;
  int w = 0, h = 0, ow = 0, oh = 0; uint8_t *px; SDL_Surface *s;
  if (larguraOriginal) *larguraOriginal = 0;
  if (alturaOriginal) *alturaOriginal = 0;
  if (!dados || n < 16 || n > 32L * 1024 * 1024) return NULL;
  if (dados[0] == 0xFF && dados[1] == 0xD8) mime = "image/jpeg";
  else if (dados[0] == 0x89 && dados[1] == 'P' && dados[2] == 'N' && dados[3] == 'G') mime = "image/png";
  else if (!memcmp(dados, "RIFF", 4) && !memcmp(dados + 8, "WEBP", 4)) mime = "image/webp";
  if (!mime) return NULL;   // GIF e o resto: IMG_Load de sempre
  px = navegador_decodificar(dados, n, mime, largMax > 0 ? largMax : 0, &w, &h, &ow, &oh);
  if (!px) return NULL;
  s = nv_superficie(0, w, h, 32, SDL_PIXELFORMAT_ABGR8888);
  if (s) {
    int y;
    for (y = 0; y < h; y++) memcpy((char *)s->pixels + y * s->pitch, px + (size_t)y * w * 4, (size_t)w * 4);
  }
  free(px);
  if (larguraOriginal) *larguraOriginal = ow > 0 ? ow : w;
  if (alturaOriginal) *alturaOriginal = oh > 0 ? oh : h;
  return s;
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
