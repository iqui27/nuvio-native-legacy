// #794: episódios com preferência por addon não usam catalog.nuvio.tv; /meta geral continua.
#include "metaprov.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned int Uint32;
Uint32 SDL_GetTicks(void) { return 0; }
void marco(const char *m) { (void)m; }
const char *ajustes_tmdb_idioma(void) { return "en"; }
char *rede_baixar(const char *url, int seg) { (void)url; (void)seg; return NULL; }

static int nNuvio, nCine;
static char *fakeGet(const char *url, int s, int *st, void *ctx) {
  (void)s; (void)ctx;
  if (strstr(url, "catalog.nuvio.tv")) {
    nNuvio++;
    if (st) *st = 599;
    return NULL;
  }
  if (strstr(url, "cinemeta")) {
    nCine++;
    if (st) *st = 200;
    return strdup("{\"meta\":{\"id\":\"tt123\",\"type\":\"series\",\"name\":\"T\"}}");
  }
  if (st) *st = 404;
  return NULL;
}

int main(void) {
  int prov = -1;
  char *c;
  metaprov_zerar_pausa();
  nNuvio = nCine = 0;
  c = metaprov_meta_com("series", "tt123", 5, fakeGet, NULL, &prov);
  assert(c && prov == METAPROV_CINEMETA && nNuvio == 1 && nCine == 1);
  free(c);

  nNuvio = nCine = 0;
  c = metaprov_meta_ficha_episodios("series", "tt123", 5, 1, &prov);
  assert(c == NULL && prov == -1 && nNuvio == 0 && nCine == 0);
  puts("metaprov #794: ficha de episódios sem Nuvio/Cinemeta com meta externa PASS");
  return 0;
}
