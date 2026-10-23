// Exercise the production filmography adapter with overlapping TMDB IDs.
#include "../src/detail.c"
#include <assert.h>
static CatItem catalogItem;
int ctxlista_indice(const char *id, long tmdb) { (void)id; assert(tmdb == 42); return 0; }
int cat_copiar_item(int i, CatItem *out) { assert(i == 0); *out = catalogItem; return 1; }
const char *pessoa_credito_imdb(int i) { (void)i; return ""; }
long pessoa_credito_tmdb(int i) { (void)i; return 42; }
const char *pessoa_credito_tipo(int i) { (void)i; return "tv"; }
const char *pessoa_credito_titulo(int i) { (void)i; return "TV title"; }
const char *pessoa_credito_ano(int i) { (void)i; return "2025"; }
const char *pessoa_credito_poster(int i) { (void)i; return "tv-poster"; }
long extras_colecao_tmdb(int i) { (void)i; return 42; }
const char *extras_colecao_titulo(int i) { (void)i; return "Movie title"; }
const char *extras_colecao_ano(int i) { (void)i; return "2024"; }
const char *extras_colecao_poster(int i) { (void)i; return "movie-poster"; }
int main(void) {
  CatItem out;
  catalogItem.tmdb = 42;
  snprintf(catalogItem.tipo, sizeof catalogItem.tipo, "movie");
  snprintf(catalogItem.titulo, sizeof catalogItem.titulo, "Wrong movie");
  assert(glanceDetItem(0, &out));
  assert(!strcmp(out.tipo, "series") && !strcmp(out.titulo, "TV title"));
  assert(!strcmp(out.imdb, "tmdb:42") && out.tmdb == 42);
  snprintf(catalogItem.tipo, sizeof catalogItem.tipo, "series");
  snprintf(catalogItem.titulo, sizeof catalogItem.titulo, "Catalog TV");
  assert(glanceDetItem(0, &out) && !strcmp(out.titulo, "Catalog TV"));
  glanceDetColecao = 1;
  assert(glanceDetItem(0, &out));
  assert(!strcmp(out.tipo, "movie") && !strcmp(out.titulo, "Movie title"));
  snprintf(catalogItem.tipo, sizeof catalogItem.tipo, "movie");
  assert(glanceDetItem(0, &out) && !strcmp(out.titulo, "Catalog TV"));
  puts("glance-collision: ok");
  return 0;
}
