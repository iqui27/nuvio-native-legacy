#include "posterprov.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
const char *extras_colecao_poster(int i);
int main(void) {
  PosterProvCfg c = { .prov = PP_MODELO };
  char url[PP_URL_MAX];
  snprintf(c.modelo, sizeof c.modelo, "https://posters.invalid/api/poster/{type}/{id}?fmt=jpeg");
  posterprov_configurar(&c);
  assert(!strcmp(extras_colecao_poster(0), "https://posters.invalid/api/poster/movie/278?fmt=jpeg"));
  snprintf(c.modelo, sizeof c.modelo, "https://posters.invalid/poster?tmdb_id={tmdb_id?}&imdb_id={imdb_id?}&stremio_id={id}&type={type}&shape={shape}&access_key=FAKE%%2Bkey");
  assert(posterprov_modelo_cabe(c.modelo));
  assert(posterprov_montar_url(&c,"tt0903747",1396,"series",url,sizeof url));
  assert(!strcmp(url,"https://posters.invalid/poster?tmdb_id=1396&imdb_id=tt0903747&stremio_id=tt0903747&type=series&shape=poster&access_key=FAKE%2Bkey"));
  posterprov_configurar(&c);
  assert(!strcmp(posterprov_card_formato(NULL,"tt0903747",1396,"series","still.jpg",1),
    "https://posters.invalid/poster?tmdb_id=1396&imdb_id=tt0903747&stremio_id=tt0903747&type=series&shape=landscape&access_key=FAKE%2Bkey"));
  assert(strstr(posterprov_card("tt0903747",1396,"series","original.jpg"), "shape=poster"));
  posterprov_configurar(NULL);
  assert(!strcmp(extras_colecao_poster(0), "original.jpg"));
  c.prov = PP_RPDB;
  strcpy(c.chave,"FAKE");
  posterprov_configurar(&c);
  assert(!strcmp(posterprov_card_formato(NULL,"tt0903747",1396,"series","still.jpg",1),"still.jpg"));
  puts("PASS: collection selector, exported template, independent shapes and opt-in");
  return 0;
}
