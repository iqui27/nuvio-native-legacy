// #794: Prefer metadata add-on detail skips catalog.nuvio.tv for tt* meta.
#include "metaprov.h"
#include <stddef.h>
#include <time.h>
#include <unistd.h>

void marco(const char *m) { (void)m; }
int ajustes_meta_externo(void) { return 1; }
int ajustes_tmdb_cw(void) { return 0; }
int lig(int op) { (void)op; return 0; }
const char *ajustes_tmdb_idioma(void) { return "en"; }
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned int Uint32;
Uint32 SDL_GetTicks(void) { return 0; }
char *rede_baixar_st(const char *u, int s, const char *const *c, int *st);
char *rede_baixar(const char *u, int s) {
  int st = 0;
  return rede_baixar_st(u, s, NULL, &st);
}
char *rede_baixar_st(const char *u, int s, const char *const *c, int *st) {
  (void)s; (void)c;
  if (strstr(u, "catalog.nuvio.tv")) { if (st) *st = 599; return NULL; }
  if (strstr(u, "cinemeta")) { if (st) *st = 200; return strdup("{\"meta\":{\"id\":\"tt123\",\"type\":\"series\",\"name\":\"T\"}}"); }
  if (st) *st = 404;
  return NULL;
}

int main(void) {
  int prov = -1;
  MetaprovGet get = (MetaprovGet)rede_baixar_st;
  char *c = metaprov_meta_com("series", "tt123", 5, get, NULL, &prov);
  assert(c && prov == METAPROV_CINEMETA);
  free(c);
  puts("metaprov #794: Nuvio catalog skipped when external meta preferred PASS");
  return 0;
}
