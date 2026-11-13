#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "plrilha.h"
enum { VIDEO_DTS_NENHUM, VIDEO_DTS_NATIVO, VIDEO_DTS_PREPARANDO,
       VIDEO_DTS_CONVERTIDO, VIDEO_DTS_FALHOU, VIDEO_DTS_SEM_SOM };
#define PG_SEG_TOTAL 6.0f
static Uint32 pgDesde;
static int carregando, estado, pedidos;
static const char *url = "fixture://a";
static char ultimo[128];
static int player_carregando(void) { return carregando; }
static int ehCanal(void) { return 0; }
static int video_dts_estado(void) { return estado; }
static const char *video_url_atual(void) { return url; }
static const char *i18n(const char *s) { return s; }
void plrilha_pedir(const PlrIlhaPedido *p) { pedidos++; snprintf(ultimo, sizeof ultimo, "%s", p->texto); }
#include "dts-aviso.c"
static Uint32 t;
// Runs `ms` of 16 ms frames; returns how many frames requested the notice.
static int rodar(Uint32 ms) {
  pedidos = 0;
  for (Uint32 fim = t + ms; t < fim; t += 16) dtsAvisoQuadro(t);
  return pedidos;
}
int main(void) {
  t = 1000;
  estado = VIDEO_DTS_CONVERTIDO;
  int n = rodar(8000);
  printf("converted notice frames=%d\n", n);
  assert(n > 250 && n <= 5000 / 16 + 1);           // whole 5 s window, every frame
  assert(!strcmp(ultimo, "Áudio DTS convertido nesta TV"));
  // Seek / retry: pipeline reopens, same source, same state -> no repeat.
  carregando = 1; assert(rodar(500) == 0); carregando = 0;
  estado = VIDEO_DTS_PREPARANDO; assert(rodar(400) == 0);
  estado = VIDEO_DTS_CONVERTIDO; assert(rodar(8000) == 0);
  // Later failure on the same source is a new, meaningful state.
  estado = VIDEO_DTS_FALHOU;
  n = rodar(13000);
  printf("later failure frames=%d\n", n);
  assert(n > 550 && n <= 10000 / 16 + 1);
  assert(!strcmp(ultimo, "Não deu para converter o áudio DTS"));
  // Track change that fails again on the same source: no repeat.
  estado = VIDEO_DTS_PREPARANDO; assert(rodar(400) == 0);
  estado = VIDEO_DTS_FALHOU; assert(rodar(13000) == 0);
  // A short-lived state (TV error 200 right after load) never shows.
  url = "fixture://b";
  estado = VIDEO_DTS_NATIVO; assert(rodar(1000) == 0);
  estado = VIDEO_DTS_SEM_SOM; n = rodar(13000);
  assert(n > 550 && !strcmp(ultimo, "Esta TV não toca áudio DTS"));
  // New source brings the conversion notice back.
  url = "fixture://c";
  estado = VIDEO_DTS_CONVERTIDO; assert(rodar(8000) > 250);
  puts("PASS DTS notice: full window per frame, later failure shown, no repeat after seek/retry/track change");
}
