// Exercise the production preview state machine without network or a window.
#include "../src/glance.c"
#include <assert.h>
static Uint32 clockNow = 100;
static int opened, playing, failed, opens, actions, busy, requests;
static CatItem item, result;
static char requestedType[16], lastUrl[32];
Uint32 SDL_GetTicks(void) { return clockNow; }
unsigned cat_revisao(void) { return 1; }
int cat_copiar_item(int i, CatItem *out) { if (i != 0) return 0; *out = result; return 1; }
int ctxlista_indice(const char *id, long tmdb) { (void)id; (void)tmdb; return -1; }
int ctx_aberto(void) { return 0; }
void ctx_acao_rapida(const CatItem *ci, int visto) { (void)ci; (void)visto; actions++; }
int desc_titulo_buscando(void) { return busy; }
void desc_pedir_titulo_semente(const char *id, long tmdb, const char *type, const char *title, const char *year, const char *poster) {
  (void)id; (void)tmdb; (void)title; (void)year; (void)poster;
  requests++; busy = 1; snprintf(requestedType, sizeof requestedType, "%s", type);
}
int ajustes_animacoes_reduzidas(void) { return 1; }
int ajustes_trailer_fonte(void) { return 0; }
int trailer_suportado(void) { return 1; }
int trailer_aberto(void) { return opened; }
int trailer_tocando(void) { return playing; }
int trailer_falhou(void) { return failed; }
void trailer_fechar(void) { opened = playing = failed = 0; }
void trailer_abrir(const char *url, GfxRect r, int som, int cheia) {
  (void)r; (void)som; (void)cheia; opened = 1; opens++; snprintf(lastUrl, sizeof lastUrl, "%s", url);
}
void trailerapple_pedir(const char *id, const char *title, const char *meta, int series) { (void)id; (void)title; (void)meta; (void)series; }
const char *trailerapple_url(const char *id) { (void)id; return "apple"; }
int trailerapple_respondeu(const char *id) { (void)id; return 1; }
void trailerimdb_pedir(const char *id) { (void)id; }
const char *trailerimdb_url(const char *id, const char **name) { (void)id; (void)name; return "imdb"; }
int trailerimdb_respondeu(const char *id) { (void)id; return 1; }
void extras_hero_trailer_pedir(const char *id, int series, long tmdb) { (void)id; (void)series; (void)tmdb; }
int extras_hero_trailer_obter(const char *id, char *dst, unsigned cap) { (void)id; (void)dst; (void)cap; return 0; }
static int total(void) { return 1; }
static int getItem(int i, CatItem *out) { if (i) return 0; *out = item; return 1; }
static const GlanceFonte source = { total, getItem, NULL, NULL };
static void key(int k) { SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = k; glance_evento(&e); }
int main(void) {
  snprintf(item.imdb, sizeof item.imdb, "tt123"); snprintf(item.tipo, sizeof item.tipo, "movie");
  glance_abrir(&source, 0); glance_atualizar(.016f, clockNow);
  assert(opens == 1 && !strcmp(lastUrl, "apple"));
  playing = 1;
  for (int i = 0; i < 5; i++) glance_atualizar(.016f, ++clockNow);
  assert(opens == 1);
  playing = 0; failed = 1; glance_atualizar(.016f, ++clockNow);
  assert(opens == 2 && !strcmp(lastUrl, "imdb"));
  playing = 1;
  for (int i = 0; i < 5; i++) glance_atualizar(.016f, ++clockNow);
  assert(opens == 2 && !strcmp(lastUrl, "imdb"));
  playing = 0; failed = 1; glance_atualizar(.016f, ++clockNow);
  assert(opens == 2 && !opened);
  snprintf(item.imdb, sizeof item.imdb, "tmdb:42"); item.tmdb = 42;
  snprintf(item.tipo, sizeof item.tipo, "series");
  glance_abrir(&source, 0); key(SDLK_DOWN); key(SDLK_DOWN); key(SDLK_RETURN);
  assert(requests == 1 && !strcmp(requestedType, "tv"));
  result = item; snprintf(result.imdb, sizeof result.imdb, "ttresolved");
  snprintf(result.tipo, sizeof result.tipo, "movie");
  assert(!glance_tomar_acao(0) && actions == 0);
  snprintf(result.tipo, sizeof result.tipo, "series"); result.tmdb = 43;
  assert(!glance_tomar_acao(0) && actions == 0);
  result.tmdb = 42;
  assert(glance_tomar_acao(0) && actions == 1);
  puts("glance-state: ok");
  return 0;
}
