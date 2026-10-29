// Run the real discovery worker synchronously, replacing HTTP only.
#define trakt_lista_cresc bibfonte_remote_list
#include "../src/descoberta.c"
#undef trakt_lista_cresc
#include <assert.h>
static int watchlists, failLibrary;
int bibfonte_remote_list(const char *which, CatItem **out, int max) {
  (void)max;
  if (failLibrary) { *out = NULL; return -1; }
  if (strcmp(which, "watchlist")) return 0;
  watchlists++;
  *out = calloc(1, sizeof **out); assert(*out);
  strcpy((*out)->imdb, "tt9999999"); strcpy((*out)->tipo, "series");
  (*out)->naLista = 1;
#ifndef BIBFONTE_BASELINE
  (*out)->listaFontes = 1u << AJ_SALVOS_TRAKT;
#endif
  return 1;
}
int bibfonte_discovery_watchlist(void) {
  watchlists = 0;
  ListasTrakt job = {0}; job.sessao = nv_perfil_sessao_ler();
  fioListas(&job);
  if (job.nWl) assert(cat_acrescentar_lote(job.wl, job.nWl, NULL) == 1);
  free(job.wl); free(job.col);
  failLibrary = 1;
  int kept = 0;
  CatItem *fallback = listaBuscar("watchlist", &kept);
#ifndef BIBFONTE_BASELINE
  assert(kept == 1 && fallback && !strcmp(fallback[0].imdb, "tt9999999"));
#endif
  free(fallback);
  failLibrary = 0;
  return watchlists;
}
