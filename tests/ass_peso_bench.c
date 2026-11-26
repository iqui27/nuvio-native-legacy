// Spike de medida (frente "ASS com estilo e PGS", 2.1): quanto custa um quadro
// de legenda ASS no libass e um quadro PGS no nosso decodificador, por cena e
// por resolucao de render. NAO e teste de aprovacao: imprime uma tabela.
//
// Mede o que o fio do assrender faz por quadro: ass_render_frame + a copia dos
// bitmaps (ass_frame_copiar). Sem GL, sem fontconfig, so com a fonte do app,
// como na TV. O relogio e o da maquina que roda: o numero absoluto so vale
// para ela; a RAZAO entre resolucoes e o que orienta o asspeso.
#include <ass/ass.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/resource.h>
#include "../src/pgs.h"

/* Soma de um byte de cada copia: impressa no fim, para o -O2 nao apagar as
 * copias que o benchmark mede. */
static volatile unsigned long sumidouro;

static double agora_ms(void) {
  struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}
static int cmp(const void *a, const void *b) {
  double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y;
}

typedef struct { char *b; size_t n, cap; } Txt;
static void pr(Txt *t, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
#include <stdarg.h>
static void pr(Txt *t, const char *fmt, ...) {
  va_list ap; int k;
  if (t->cap - t->n < 1024) { t->cap = t->cap ? t->cap * 2 : 1 << 16; t->b = realloc(t->b, t->cap); }
  va_start(ap, fmt); k = vsnprintf(t->b + t->n, t->cap - t->n, fmt, ap); va_end(ap);
  t->n += (size_t)k;
}

static void cabecalho(Txt *t) {
  pr(t, "[Script Info]\nScriptType: v4.00+\nPlayResX: 1920\nPlayResY: 1080\nWrapStyle: 0\nScaledBorderAndShadow: yes\n\n"
        "[V4+ Styles]\nFormat: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n"
        "Style: Default,Inter Display,64,&H00FFFFFF,&H000000FF,&H00000000,&H80000000,0,0,0,0,100,100,0,0,1,3,1.5,2,60,60,50,1\n"
        "Style: Kara,Inter Display,58,&H00FFE0A0,&H00FFFFFF,&H00402000,&H80000000,-1,0,0,0,100,100,1,0,1,4,0,8,60,60,40,1\n"
        "Style: Placa,Inter Display,90,&H0030C0FF,&H000000FF,&H00101010,&H80000000,-1,0,0,0,100,100,0,0,1,5,0,5,0,0,0,1\n\n"
        "[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n");
}
// As quatro cenas duram 0..6 s.
static void cena_dialogo(Txt *t) {
  cabecalho(t);
  pr(t, "Dialogue: 0,0:00:00.00,0:00:03.00,Default,,0,0,0,,Voce nao devia ter voltado aqui.\\NEu avisei da ultima vez.\n");
  pr(t, "Dialogue: 0,0:00:03.00,0:00:06.00,Default,,0,0,0,,{\\i1}Entao por que deixou a porta aberta?{\\i0}\n");
}
static void cena_karaoke(Txt *t) {
  int l, s;
  cabecalho(t);
  for (l = 0; l < 2; l++) {
    pr(t, "Dialogue: 0,0:00:00.00,0:00:06.00,Kara,,0,0,0,,{\\an%d\\blur2\\fad(300,300)}", l ? 2 : 8);
    for (s = 0; s < 12; s++) pr(t, "{\\kf50}%s ", l ? "sora" : "kimi");
    pr(t, "\n");
  }
  pr(t, "Dialogue: 0,0:00:00.00,0:00:06.00,Default,,0,0,0,,{\\an2}Traducao da musica de abertura\n");
}
static void cena_placa(Txt *t) {
  int i;
  cabecalho(t);
  /* Placa tipografada: 40 camadas com blur, giro e escala animados por \t e \move. */
  for (i = 0; i < 40; i++)
    pr(t, "Dialogue: %d,0:00:00.00,0:00:06.00,Placa,,0,0,0,,{\\an5\\move(%d,%d,%d,%d)\\blur%d\\bord%d\\frz%d"
          "\\t(0,6000,\\frz%d\\fscx%d\\fscy%d)\\1a&H%02X&}LOJA DE DEPARTAMENTOS\n",
       i, 500 + i * 6, 300 + i * 4, 1100 + i * 6, 640 + i * 4, 1 + i % 4, 2 + i % 5, i % 9 - 4,
       i % 11 - 5, 100 + i % 20, 100 + i % 20, (i * 5) % 200);
}
static void cena_gradiente(Txt *t) {
  int i;
  cabecalho(t);
  /* Gradiente de fansub: a mesma fala repetida em 120 faixas com \clip. */
  for (i = 0; i < 120; i++)
    pr(t, "Dialogue: 1,0:00:00.00,0:00:06.00,Default,,0,0,0,,{\\an2\\pos(960,1000)\\clip(%d,0,%d,1080)\\blur1"
          "\\1c&H%02X%02XFF&\\t(0,6000,\\1c&HFF%02X%02X&)}O ceu desta cidade nunca foi tao vermelho\n",
       300 + i * 11, 311 + i * 11, i * 2, 255 - i * 2, i * 2, 255 - i * 2);
}

typedef struct { const char *nome; void (*gera)(Txt *); } Cena;
static const Cena CENAS[] = {
  { "dialogo", cena_dialogo }, { "karaoke", cena_karaoke },
  { "placa-40-camadas", cena_placa }, { "gradiente-120-clips", cena_gradiente },
};
typedef struct { const char *nome; int w, h; } Res;
static const Res RES[] = {
  { "4K 1:1", 3840, 2160 }, { "1080p 1:1", 1920, 1080 }, { "1080p /2", 960, 540 },
  { "1080p /3", 640, 360 }, { "720p 1:1", 1280, 720 },
};

static void calado(int nivel, const char *fmt, va_list ap, void *u) { (void)nivel; (void)fmt; (void)ap; (void)u; }

static void mede_ass(const char *fonte) {
  size_t c, r;
  printf("ASS (libass %x, fonte %s, 24 quadros/s, 6 s de cena)\n", ass_library_version(), fonte);
  printf("%-22s %-10s %8s %8s %8s %9s %7s %10s\n", "cena", "render", "medio ms", "p95 ms", "max ms", "textura ms", "imagens", "KiB/quadro");
  for (c = 0; c < sizeof CENAS / sizeof *CENAS; c++) {
    Txt t = { 0 };
    CENAS[c].gera(&t);
    for (r = 0; r < sizeof RES / sizeof *RES; r++) {
      ASS_Library *lib = ass_library_init();
      ASS_Renderer *ren = ass_renderer_init(lib);
      ASS_Track *trk;
      double v[160], soma = 0, tex = 0; int n = 0, f, imgs = 0; size_t bytes = 0;
      ass_set_message_cb(lib, calado, NULL);
      ass_set_frame_size(ren, RES[r].w, RES[r].h);
      ass_set_storage_size(ren, 1920, 1080);
      ass_set_shaper(ren, ASS_SHAPING_COMPLEX);
      ass_set_hinting(ren, ASS_HINTING_LIGHT);
      ass_set_cache_limits(ren, 0, 32);
      ass_set_fonts(ren, fonte, "Inter Display", ASS_FONTPROVIDER_NONE, NULL, 0);
      trk = ass_read_memory(lib, t.b, t.n, NULL);
      if (!trk) { fprintf(stderr, "faixa invalida\n"); exit(1); }
      for (f = 0; f < 144; f++) {
        int mudou = 0; unsigned char *copia;
        double a = agora_ms(), m;
        ASS_Image *im0 = ass_render_frame(ren, trk, (long long)(f * 1000 / 24), &mudou), *im;
        size_t b = 0; int k = 0;
        for (im = im0; im; im = im->next) {    /* a copia que ass_frame_copiar faz: linha a linha, w bytes */
          size_t s = (size_t)im->w * (size_t)im->h;
          int y;
          if (!s || !im->bitmap || im->stride < im->w) continue;
          copia = malloc(s);
          if (!copia) continue;
          for (y = 0; y < im->h; y++)
            memcpy(copia + (size_t)y * (size_t)im->w, im->bitmap + (size_t)y * (size_t)im->stride, (size_t)im->w);
          sumidouro += copia[s / 2];           /* observavel: o -O2 nao apaga a copia */
          free(copia);
          b += s * 4; k++;                     /* RGBA que vai para a textura */
        }
        m = agora_ms();
        v[n] = m - a; soma += v[n]; n++;
        /* O que ass_textura_locked faz no fio de DESENHO antes do glTexImage2D:
         * alfa de 8 bits -> RGBA. O envio a GPU em si nao e medido aqui. */
        for (im = im0; mudou && im; im = im->next) {
          size_t px = (size_t)im->w * im->h, x; int y;
          unsigned char *rgba;
          if (!px) continue;
          rgba = malloc(px * 4);
          if (!rgba) continue;
          for (y = 0; y < im->h; y++) {
            const unsigned char *src = im->bitmap + (size_t)y * im->stride;
            for (x = 0; x < (size_t)im->w; x++) {
              unsigned char *d = rgba + ((size_t)y * im->w + x) * 4;
              d[0] = 255; d[1] = 200; d[2] = 100; d[3] = (unsigned char)((src[x] * 255u + 127u) / 255u);
            }
          }
          sumidouro += rgba[px * 4 - 1];       /* observavel no -O2 */
          free(rgba);
        }
        tex += agora_ms() - m;
        if (b > bytes) bytes = b;
        if (k > imgs) imgs = k;
      }
      qsort(v, (size_t)n, sizeof *v, cmp);
      printf("%-22s %-10s %8.2f %8.2f %8.2f %9.2f %7d %10zu\n", CENAS[c].nome, RES[r].nome,
             soma / n, v[n * 95 / 100], v[n - 1], tex / n, imgs, bytes / 1024);
      ass_free_track(trk); ass_renderer_done(ren); ass_library_done(lib);
    }
    free(t.b);
  }
}

// PGS: um display set com um objeto w x h (texto "listrado": corridas curtas,
// como letras com contorno), fatiado em ODS de ate 65 KiB como no Blu-ray.
static size_t monta_pgs(uint8_t *o, int w, int h) {
  size_t n = 0, rleN = 0, i, pos = 0;
  uint8_t *rle = malloc((size_t)w * h + (size_t)h * 2 + 16);
  int x, y, primeiro = 1;
  for (y = 0; y < h; y++) {
    for (x = 0; x < w;) {
      int run = 3 + (x * 7 + y * 3) % 23, cor = ((x / 9 + y / 5) % 3 == 0) ? 0 : 1 + (x + y) % 3;
      if (run > w - x) run = w - x;
      if (cor && run < 3) { int k; for (k = 0; k < run; k++) rle[rleN++] = (uint8_t)cor; }
      else if (cor) { rle[rleN++] = 0; rle[rleN++] = (uint8_t)(0x80 | run); rle[rleN++] = (uint8_t)cor; }
      else { rle[rleN++] = 0; rle[rleN++] = (uint8_t)run; }
      x += run;
    }
    rle[rleN++] = 0; rle[rleN++] = 0;
  }
#define U8(v) o[n++] = (uint8_t)(v)
#define U16(v) do { U8((v) >> 8); U8(v); } while (0)
  U8(0x16); U16(19); U16(1920); U16(1080); U8(0x10); U16(1); U8(0x80); U8(0); U8(0); U8(1);
  U16(0); U8(0); U8(0); U16((1920 - w) / 2); U16(1080 - h);
  U8(0x17); U16(10); U8(1); U8(0); U16((1920 - w) / 2); U16(1080 - h); U16(w); U16(h);
  U8(0x14); U16(2 + 3 * 5); U8(0); U8(0);
  for (i = 1; i <= 3; i++) { U8(i); U8(i == 1 ? 235 : i == 2 ? 16 : 128); U8(128); U8(128); U8(i == 3 ? 128 : 255); }
  while (pos < rleN) {
    size_t cab = primeiro ? 11 : 4, cabe = 65535 - cab, parte = rleN - pos < cabe ? rleN - pos : cabe;
    int ult = pos + parte == rleN;
    U8(0x15); U16(cab + parte); U16(0); U8(0); U8((primeiro ? 0x80 : 0) | (ult ? 0x40 : 0));
    if (primeiro) { size_t tot = rleN + 4; U8(tot >> 16); U8(tot >> 8); U8(tot); U16(w); U16(h); }
    memcpy(o + n, rle + pos, parte); n += parte; pos += parte; primeiro = 0;
  }
  U8(0x80); U16(0);
  free(rle);
  return n;
}

static void mede_pgs(void) {
  static const struct { const char *nome; int w, h; } T[] = {
    { "duas linhas 1400x160", 1400, 160 }, { "placa 1920x540", 1920, 540 }, { "tela cheia 1920x1080", 1920, 1080 },
  };
  size_t i;
  uint8_t *buf = malloc(8u << 20);
  printf("\nPGS (src/pgs.c: RLE + paleta -> RGBA), 200 quadros\n");
  printf("%-22s %8s %8s %8s %10s %10s\n", "quadro", "medio ms", "p95 ms", "max ms", "bloco KiB", "RGBA KiB");
  for (i = 0; i < sizeof T / sizeof *T; i++) {
    size_t n = monta_pgs(buf, T[i].w, T[i].h);
    double v[200], soma = 0; int k, ok = 0;
    Pgs *d = pgs_novo();
    for (k = 0; k < 200; k++) {
      PgsQuadro q; double a = agora_ms();
      ok += pgs_decodificar(d, buf, n, &q) && q.n == 1 && q.obj[0].w == T[i].w;
      v[k] = agora_ms() - a; soma += v[k];
      pgs_quadro_liberar(&q);
    }
    if (ok != 200) { fprintf(stderr, "pgs: %d/200 quadros decodificados\n", ok); exit(1); }
    qsort(v, 200, sizeof *v, cmp);
    printf("%-22s %8.3f %8.3f %8.3f %10zu %10zu\n", T[i].nome, soma / 200, v[190], v[199],
           n / 1024, (size_t)T[i].w * T[i].h * 4 / 1024);
    pgs_liberar(d);
  }
  free(buf);
}

int main(int argc, char **argv) {
  struct rusage ru;
  const char *fonte = argc > 1 ? argv[1] : "deploy/app/fonts/InterDisplay-Regular.ttf";
  mede_ass(fonte);
  mede_pgs();
  getrusage(RUSAGE_SELF, &ru);
  printf("\npico de memoria do processo (RSS): %ld MiB\n", ru.ru_maxrss / 1024);
  printf("(controle das copias: %lu)\n", (unsigned long)sumidouro);
  return 0;
}
