// #566: a custom service template must reach the same selector used by cards.
#include "posterprov.h"
#include <stdio.h>
#include <string.h>

int main(void) {
  PosterProvCfg c = {0};
  const char *fallback = "https://image.tmdb.org/t/p/w500/original.jpg";
  const char *url;
  c.prov = PP_MODELO;
  snprintf(c.modelo, sizeof c.modelo,
           "https://postersplus.example.invalid/poster?imdb_id={imdb_id}"
           "&tmdb_id={tmdb_id?}&type={type}&key=FAKE_AbC%%2B123");
  posterprov_configurar(&c);
  posterprov_preferir_addon(0);
  url = posterprov_card_addon("cinemeta", "tt0903747", 0, "series", fallback);
  if (strcmp(url, "https://postersplus.example.invalid/poster?imdb_id=tt0903747"
                  "&tmdb_id=&type=series&key=FAKE_AbC%2B123")) {
    fprintf(stderr, "FAIL: non-metahub custom template was not used for the card: %s\n", url);
    return 1;
  }
  if (!posterprov_modelo_cabe(c.modelo) || !posterprov_e_provedor(url)) return 1;
  url = posterprov_card_addon("cinemeta", "tt0903747", 1396, "series", fallback);
  if (!strstr(url, "&tmdb_id=1396&type=series&")) return 1;
  // Required IDs still fall back; unknown optional markers remain invalid.
  snprintf(c.modelo, sizeof c.modelo, "https://posters.invalid/{tmdb_id}");
  posterprov_configurar(&c);
  if (strcmp(posterprov_card_addon("cinemeta", "tt0903747", 0, "series", fallback), fallback)) return 1;
  if (posterprov_modelo_valido("https://posters.invalid/{unknown?}")) return 1;
  snprintf(c.modelo, sizeof c.modelo,
           "https://posters.invalid/{type}/{tipo_tmdb}/{id}?imdb={imdb?}&tmdb={tmdb?}");
  posterprov_configurar(&c);
  if (strcmp(posterprov_card_addon("addon", "tmdb:t1396", 0, "series", fallback),
             "https://posters.invalid/series/tv/1396?imdb=&tmdb=1396")) return 1;
  puts("PASS: non-metahub custom template reaches the card; optional and required IDs respected");
  return 0;
}
