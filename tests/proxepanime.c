// ISSUE #697 (LG C1, 2.0.8): Next Episode on an anime plays an unrelated show.
//
// Root cause in the title lookup, not in the stream query. Anime progress
// keys are absolute ("kitsu:7442:5", one colon — idbase.h:idbase_episodio
// reads a single suffix as the absolute episode). mesmoTitulo accepted only
// the IMDb ":T:E" tail, so an episode-bearing anime id never matched its own
// title. abrirProximoEpisodio then calls
//   player_abrir(cat_indice_vivo(titulo, imdb))
// and cat_indice_titulo returns -1; player_abrir wraps the negative index
// modulo cat_n() to the LAST catalog item — a different, unrelated show.
//
// This test proves the id shapes that reach cat_indice_titulo/cat_indice_vivo
// resolve to the title's own copy for anime namespaces (kitsu, mal, anilist)
// and still resolve for IMDb.
//
//   bash tests/proxepanime.sh
#define main catalog_identity_fixture_main
#include "catalog_episode_identity512.c"
#undef main

int main(void) {
  // A catalog with an unrelated show in the LAST slot: if the anime lookup
  // misses, the wrap lands there — exactly the reported symptom.
  CatItem items[3] = {title(0), title(0), title(0)};
  strcpy(items[0].imdb, "kitsu:7442");
  strcpy(items[1].imdb, "mal:5114");
  strcpy(items[2].imdb, "tt0903747"); // unrelated, last slot
  cat_definir_tudo(items, 3, NULL, 0);

  // Absolute-episode progress keys must find their own title.
  assert(cat_indice_titulo("kitsu:7442:17", -1) == 0);
  assert(cat_indice_titulo("mal:5114:3", -1) == 1);
  assert(cat_indice_vivo(2, "kitsu:7442:17") == 0);

  // And the old behaviour keeps working: IMDb season:episode keys, exact ids,
  // and an id from a different namespace never matches.
  assert(cat_indice_titulo("tt0903747:1:2", -1) == 2);
  assert(cat_indice_titulo("kitsu:7442", -1) == 0);
  assert(cat_indice_titulo("kitsu:9999:1", -1) == -1);
  assert(cat_indice_titulo("tt0903747:99", -1) == -1); // one colon is not ":T:E" for imdb
  puts("PASS #697: anime episode ids resolve to their own title");
}
