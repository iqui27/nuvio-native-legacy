// BANCADA DO DECODIFICADOR DE GIF NO MAC (tests/gifbanco.sh, #84).
//
//   gifbanco <arquivo.gif> <largura do card> [quadros.rgba do ffmpeg]
//
// Mede o que o fio de decode gasta por quadro — na tela logica inteira e
// reduzido ao card — contra o atraso que o proprio GIF pede. Com o terceiro
// argumento (os quadros compostos pelo ffmpeg, `-f rawvideo -pix_fmt rgba`),
// confere tambem pixel a pixel a primeira volta contra outro decodificador.
#include "../src/gif.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double agora(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

static unsigned char *ler(const char *c, size_t *n) {
  FILE *f = fopen(c, "rb");
  unsigned char *b;
  long t;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); t = ftell(f); rewind(f);
  b = (unsigned char *)malloc((size_t)t);
  *n = b ? fread(b, 1, (size_t)t, f) : 0;
  fclose(f);
  return b;
}

// Tres voltas; devolve ms por quadro.
static double medir(const char *c, int sw, int sh, int *nq, int *nominal) {
  size_t n;
  unsigned char *b = ler(c, &n);
  GifDec *d = gif_dec_abrir(b, n, sw, sh);
  double t0, t;
  int i, y0, y1, total;
  if (!d) { fprintf(stderr, "nao abriu %s\n", c); exit(1); }
  *nq = gif_dec_quadros(d);
  *nominal = 0;
  for (i = 0; i < *nq; i++) *nominal += gif_dec_atraso(d, i);
  total = *nq * 3;
  t0 = agora();
  for (i = 0; i < total; i++) gif_dec_proximo(d, &y0, &y1);
  t = agora() - t0;
  gif_dec_fechar(d);
  return t / total;
}

int main(int argc, char **argv) {
  int tw, th, sw, sh, nq, nominal;
  double inteiro, reduzido;
  size_t n;
  unsigned char *b;
  if (argc < 3) { fprintf(stderr, "uso: gifbanco arquivo.gif largura [ref.rgba]\n"); return 2; }
  b = ler(argv[1], &n);
  if (!b || n < 10) return 1;
  tw = b[6] | (b[7] << 8); th = b[8] | (b[9] << 8);
  free(b);
  gif_tamanho_saida(tw, th, atoi(argv[2]), &sw, &sh);
  inteiro = medir(argv[1], tw, th, &nq, &nominal);
  reduzido = medir(argv[1], sw, sh, &nq, &nominal);
  printf("%-10s %3d quadros %dx%d -> %dx%d  nominal %5d ms/volta (%.1f ms/quadro)  "
         "decode %.2f ms/quadro inteiro, %.2f reduzido  => volta em %.0f ms de CPU\n",
         strrchr(argv[1], '/') ? strrchr(argv[1], '/') + 1 : argv[1], nq, tw, th, sw, sh,
         nominal, (double)nominal / nq, inteiro, reduzido, reduzido * nq);

  if (argc > 3) {
    size_t rn, quadro = (size_t)tw * th * 4, dif = 0, tot = 0;
    unsigned char *ref = ler(argv[3], &rn);
    GifDec *d;
    int i, y0, y1;
    b = ler(argv[1], &n);
    d = gif_dec_abrir(b, n, tw, th);
    if (!ref || !d) return 1;
    for (i = 0; i < nq && (size_t)(i + 1) * quadro <= rn; i++) {
      const unsigned char *a = gif_dec_composta(d), *r = ref + (size_t)i * quadro;
      size_t k;
      gif_dec_proximo(d, &y0, &y1);
      for (k = 0; k < quadro; k += 4) {
        int alfaA = a[k + 3] > 127, alfaR = r[k + 3] > 127;
        tot++;
        if (alfaA != alfaR || (alfaA && memcmp(a + k, r + k, 3))) dif++;
      }
    }
    printf("           conferido contra o ffmpeg: %d quadros, %zu de %zu pixels diferentes\n", i, dif, tot);
    gif_dec_fechar(d);
    free(ref);
    if (dif) return 1;
  }
  return 0;
}
