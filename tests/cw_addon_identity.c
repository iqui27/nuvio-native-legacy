#define main catalog_identity_fixture_main
#include "catalog_episode_identity512.c"
#undef main

int main(void) {
  const char *ids[] = {"tt21975436", "kitsu:7442", "tmdb:123"};
  const char *videos[] = {"tt21975436:2:5", "kitsu:7442:17", "provider:opaque-video"};
  for (int i = 0; i < 3; i++) {
    CatItem copies[2] = {title(0), title(0)};
    for (int k = 0; k < 2; k++) {
      strcpy(copies[k].imdb, ids[i]);
      strcpy(copies[k].origem, "configured-addon");
    }
    CatEp episode = ep(2, 5, 0);
    strcpy(episode.vid, videos[i]);
    cat_definir_tudo(copies, 2, NULL, 0);
    cat_definir_episodios(1, &episode, 1);
    char detail[96], cw[96], next[96];
    assert(cat_id_stream(1, 2, 5, detail, sizeof detail));
    assert(cat_id_stream(0, 2, 5, cw, sizeof cw));
    assert(cat_id_stream(cat_indice_titulo(ids[i], 0), 2, 5, next, sizeof next));
    assert(!strcmp(detail, videos[i]));
    assert(!strcmp(cw, detail) && !strcmp(next, detail));
    assert(!strcmp(copies[0].tipo, copies[1].tipo));
    assert(!strcmp(copies[0].origem, copies[1].origem));
  }
  puts("PASS CW and Detail copies preserve published episode IDs");
}
