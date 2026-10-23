// Fullscreen "Watch trailer" (wasm): a failed or stalled dubbed YouTube falls
// back Apple -> IMDb, reopened fullscreen, like autoplay. Drives the real
// tocarTrailerCheio()/trailerEscada() of src/detail.c compiled as the wasm
// path; only the trailer element and the source lookups are fakes.
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#define __EMSCRIPTEN__ 1
#define cat_item                         f_cat_item
#define trailerapple_url                 f_apple_url
#define trailerapple_respondeu           f_apple_respondeu
#define trailerimdb_url                  f_imdb_url
#define trailerimdb_respondeu            f_imdb_respondeu
#define trailerfonte_tizen               f_tizen
#define extras_n_trailers                f_n_trailers
#define extras_trailer_yt                f_trailer_yt
#define extras_trailer_idioma_preferido  f_idioma_preferido
#define extras_carregando                f_carregando
#define trailer_suportado                f_suportado
#define trailer_abrir                    f_abrir
#define trailer_aberto                   f_aberto
#define trailer_tocando                  f_tocando
#define trailer_cheia                    f_cheia
#define trailer_falhou                   f_falhou
#define trailer_fechar                   f_fechar
#define trailer_estado                   f_estado
#include "../src/detail.c"

static CatItem fItem;
const CatItem *f_cat_item(int i) { (void)i; return &fItem; }
const char *f_apple_url(const char *id) { (void)id; return "apple.m3u8"; }
int f_apple_respondeu(const char *id) { (void)id; return 1; }
const char *f_imdb_url(const char *id, const char **x) { (void)id; (void)x; return "imdb.mp4"; }
int f_imdb_respondeu(const char *id) { (void)id; return 1; }
int f_tizen(void) { return 1; }
int f_n_trailers(void) { return 1; }
const char *f_trailer_yt(int k) { (void)k; return "ytDUB"; }
int f_idioma_preferido(int k) { (void)k; return 1; }
int f_carregando(void) { return 0; }

static int aberto, cheia, falhou, aberturas;
static char fonte[64];
int f_suportado(void) { return 1; }
void f_abrir(const char *u, GfxRect r, int som, int c) {
  (void)r; (void)som;
  snprintf(fonte, sizeof fonte, "%s", u);
  aberto = 1; cheia = c; falhou = 0; aberturas++;
}
int f_aberto(void) { return aberto; }
int f_tocando(void) { return 0; }
int f_cheia(void) { return cheia; }
int f_falhou(void) { return falhou; }
void f_fechar(void) { aberto = 0; }
int f_estado(void) { return -1; }

static int falhas;
static void check(const char *nome, int ok) {
  printf("%s %s\n", ok ? "ok" : "FALHOU", nome);
  if (!ok) falhas++;
}

int main(void) {
  Uint32 t;
  snprintf(fItem.imdb, sizeof fItem.imdb, "tt0000001");
  trailerfonte_definir_imdb_tizen(1);

  tocarTrailerCheio(0);
  check("Watch trailer opens dubbed YouTube fullscreen", !strcmp(fonte, "ytDUB") && cheia);

  // YouTube error 153 (trailer_atualizar closed it and marked falhou), page scrolled.
  aberto = 0; falhou = 1;
  t = SDL_GetTicks();
  trailerEscada(t, 0);
  check("error falls back to Apple, fullscreen", aberturas == 2 && !strcmp(fonte, "apple.m3u8") && cheia);

  // Apple stalls with no `playing`: bounded preparation deadline.
  trailerEscada(t + NV_TRAILER_PREPARA_MS - 1, 0);
  check("no fallback before the deadline", aberturas == 2);
  trailerEscada(t + NV_TRAILER_PREPARA_MS, 0);
  check("stall falls back to IMDb, fullscreen", aberturas == 3 && !strcmp(fonte, "imdb.mp4") && cheia);

  // Back closes fullscreen (no failure): the ladder stops.
  aberto = 0;
  trailerEscada(t + 3 * NV_TRAILER_PREPARA_MS, 0);
  check("Back ends the ladder", aberturas == 3 && trailerEtapa == 0);

  printf("trailer-cheia-escada: %s\n", falhas ? "FALHOU" : "tudo ok");
  return falhas != 0;
}
