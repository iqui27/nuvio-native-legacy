// #739: provedor Posters+ monta URL do serviço.
#include "posterprov.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  PosterProvCfg c;
  char url[PP_URL_MAX];
  memset(&c, 0, sizeof c);
  c.prov = PP_POSTERSPLUS;
  snprintf(c.instancia, sizeof c.instancia, "https://posters.example");
  assert(posterprov_montar_url(&c, "tt0903747", 1396, "series", url, sizeof url));
  assert(strstr(url, "posters.example/poster?"));
  assert(strstr(url, "tmdb_id=1396"));
  assert(strstr(url, "imdb_id=tt0903747"));
  assert(strstr(url, "type=series"));
  puts("d1_postersplus: ok");
  return 0;
}
