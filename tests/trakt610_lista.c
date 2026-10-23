// #610: short pages and unsupported IDs must not truncate the watchlist.
#include "stub_perfisessao.h"
#define main biblimites_main
#include "biblimites_trakt.c"
#undef main
#define LIBRARY639_TRAKT
#include "library639_http.inc"

int main(void) {
  CatItem *v = calloc(400, sizeof *v);
  assert(v);
  ligado = 1;
  snprintf(token, sizeof token, "t");
  snprintf(cliente, sizeof cliente, "c");
  filmesWl = 36; seriesWl = 24;
  paginaServidor = 12; paginaSemImdb = 2;
  assert(trakt_lista("watchlist", v, 400) == 48);
  assert(!strcmp(v[12].imdb, "tt0000024"));
  assert(!strcmp(v[47].imdb, "tt5000023"));
  free(v);
  failedPage = 2;
  v = NULL;
  assert(trakt_lista_cresc("watchlist", &v, 400) == -1);
  free(v);
  puts("#610 watchlist: PASS (12-item pages, missing IDs, movies and shows)");
  return 0;
}
