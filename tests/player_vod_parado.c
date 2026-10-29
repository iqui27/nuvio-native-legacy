// Row 11: uma linha de "[player] VOD parado" por parada, nao por quadro, e
// nenhuma enquanto a pessoa pausou. Roda o bloco de producao (extraido por
// tests/player_vod_parado.sh) frame a frame, com a saida em arquivo.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef uint32_t Uint32;
static unsigned T;
static double POS = 409.641;
static int comVideo = 1, pausaPessoa = 0;
static unsigned SDL_GetTicks(void) { return T; }
static unsigned video_bufferando_ms(void) { return 0; }
static double video_pos(void) { return POS; }
static int video_pronto(void) { return 1; }
static int video_tocando(void) { return 0; }
static int ehCanal(void) { return 0; }
static void tex_estatisticas(int *i, void *a, long *b, void *c, void *d) {
  (void)a; (void)c; (void)d; *i = 151; *b = 127402393;
}
static void memlog_amostra(char *m, int n) { snprintf(m, n, " | mem=454.1/395.8MB"); }

static void quadro(void) {
#include "bloco.inc"
}

// Conta as linhas do log na saida do arquivo.
static int linhas(const char *cam) {
  FILE *f = fopen(cam, "r");
  int n = 0;
  char l[512];
  if (!f) return -1;
  while (fgets(l, sizeof l, f)) n++;
  fclose(f);
  return n;
}

int main(int argc, char **argv) {
  const char *cam = argc > 1 ? argv[1] : "/tmp/vod-parado.log";
  FILE *out = freopen(cam, "w", stdout);
  int f, n;
  if (!out) { fprintf(stderr, "FAIL: nao abriu %s\n", cam); return 1; }

  // 30 s de VOD parado a 60 fps, posicao congelada (o caso do harness).
  for (f = 0; f < 60 * 30; f++) { T = 1000 + f * 16; quadro(); }
  fflush(stdout);
  n = linhas(cam);
  if (n != 1) {
    fprintf(stderr, "FAIL: %d linhas em 30 s de VOD parado (esperado 1)\n", n);
    return 1;
  }

  // A posicao andando de novo: nova parada depois dos 8 s -> uma linha nova.
  for (f = 0; f < 600; f++) { T += 16; POS += 0.1; quadro(); }
  for (f = 0; f < 60 * 30; f++) { T += 16; quadro(); }
  fflush(stdout);
  n = linhas(cam);
  if (n != 2) {
    fprintf(stderr, "FAIL: %d linhas depois de andar e parar de novo (esperado 2)\n", n);
    return 1;
  }

  // Pausa da pessoa nao e parada: nao conta, nem reinicia a contagem.
  pausaPessoa = 1;
  for (f = 0; f < 60 * 30; f++) { T += 16; POS += 0.1; quadro(); }
  for (f = 0; f < 60 * 30; f++) { T += 16; quadro(); }
  fflush(stdout);
  n = linhas(cam);
  if (n != 2) {
    fprintf(stderr, "FAIL: %d linhas com a pessoa pausada (esperado 2, sem linha nova)\n", n);
    return 1;
  }

  fclose(out);
  puts("player_vod_parado: PASS");
  return 0;
}
