// A second press inside the neighbor transition retargets the layers.
#include "../src/glance.c"
#include <assert.h>
static Uint32 clockNow = 100;
static int opened, playing, failed, opens, actions, busy, requests;
static int catalogIndex = -1, episodeBusy, episodeRequests;
static int miniActive, retained;
int player_mini_ativo(void) { return miniActive; }
int player_retido(void) { return retained; }
void desc_episodios(int idx, int season) { assert(idx == 0 && season == 0); episodeBusy = 1; episodeRequests++; }
int desc_episodios_carregando(int idx) { assert(idx == 0); return episodeBusy; }
static CatItem item, result;
static char requestedType[16], lastUrl[32];
Uint32 SDL_GetTicks(void) { return clockNow; }
unsigned cat_revisao(void) { return 1; }
int cat_copiar_item(int i, CatItem *out) { if (i != 0) return 0; *out = result; return 1; }
int ctxlista_indice(const char *id, long tmdb) { (void)id; (void)tmdb; return catalogIndex; }
int ctx_aberto(void) { return 0; }
void ctx_acao_rapida(const CatItem *ci, int visto) { (void)ci; (void)visto; actions++; }
int desc_titulo_buscando(void) { return busy; }
void desc_pedir_titulo_semente(const char *id, long tmdb, const char *type, const char *title, const char *year, const char *poster) {
  (void)id; (void)tmdb; (void)title; (void)year; (void)poster;
  requests++; busy = 1; snprintf(requestedType, sizeof requestedType, "%s", type);
}
float gfx_escala_txt(void) { return 1; }
float gfx_escala_txt_entrar(void) { return 1; }
float gfx_escala_tela_txt(void) { return 1; }
void gfx_escala_sair(float a) { (void)a; }
void ajustes_area_conteudo(float l, float r, float *x, float *w) { (void)l; (void)r; *x = 0; *w = 1920; }
static int reduced = 1;
int ajustes_animacoes_reduzidas(void) { return reduced; }
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
static int total(void) { return 2; }
static int getItem(int i, CatItem *out) {
  if (i < 0 || i > 1) return 0;
  *out = item;
  if (i) snprintf(out->imdb, sizeof out->imdb, "ttneighbor");
  return 1;
}
static const GlanceFonte source = { total, getItem, NULL, NULL };
static void key(int k) { SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = k; glance_evento(&e); }
int main(void) {
  snprintf(item.imdb, sizeof item.imdb, "tt123"); snprintf(item.tipo, sizeof item.tipo, "movie");
  // A second press inside the neighbor transition retargets: the first press
  // stays as a fading layer instead of snapping the composite.
  reduced = 0;
  glance_abrir(&source, 0); key(SDLK_RIGHT);
  glance_atualizar(.05f, ++clockNow);
  assert(glance_camadas() == 1);
  key(SDLK_LEFT);
  assert(glance_camadas() == 2 && troca == 0);
  glance_atualizar(.05f, ++clockNow);
  assert(glance_camadas() == 2 && troca > 0 && troca < 1);
  for (int i = 0; i < 30; i++) glance_atualizar(.016f, ++clockNow);
  assert(glance_camadas() == 0);
  puts("glance-retarget: ok");
  return 0;
}
