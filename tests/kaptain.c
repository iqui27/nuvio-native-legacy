// Real Kaptain sources, exercised through the production collection parser and URL builder.
#include <assert.h>
#include "../src/descoberta.c"
int ajustes_tmdb_ligado(void) { return 1; }
const char *ajustes_tmdb_idioma(void) { return "en-US"; }
const char *addons_base_por_id(const char *id) { (void)id; return ""; }
int main(void) {
  char body[65536], url[4096];
  FILE *fp = fopen("tests/fixtures/kaptain-new.json", "rb");
  assert(fp);
  size_t n = fread(body, 1, sizeof body - 1, fp); fclose(fp); body[n] = 0;
  col_definir_json(body);
  assert(col_n() == 2);
  snprintf(tmdbChave, sizeof tmdbChave, "offline-fixture-key");
  for (int i = 0; i < 2; i++) {
    const ColFolder *folder = col_folder(i);
    assert(folder && folder->nSources > 0);
    for (int j = 0; j < folder->nSources; j++) {
      const ColSource *s = &folder->sources[j];
      assert(!strcmp(tmdbMontarUrl(s, 2, url, sizeof url), "results"));
      assert(strstr(url, i ? "/discover/tv?" : "/discover/movie?"));
      assert(strstr(url, "&page=2&sort_by="));
      assert(strstr(url, s->ordenar));
      assert(strstr(url, i ? "&first_air_date_year=2026" : "&year=2026"));
      assert(strstr(url, i ? "&vote_count.gte=25" : "&vote_count.gte=50"));
      assert(strstr(url, "&with_original_language=en"));
      assert(strstr(url, "&with_genres="));
    }
  }
  ColSource source = {0};
  snprintf(source.filtros, sizeof source.filtros,
    "{\"voteAverageGte\":7.5,\"voteAverageLte\":9,\"voteCountGte\":\"50\","
    "\"releaseDateGte\":\"2026-01-01\",\"releaseDateLte\":\"2026-10-01\","
    "\"withWatchProviders\":\"8|9\",\"withoutWatchProviders\":\"10\",\"watchRegion\":\"GB\"}");
  tmdbMontarUrl(&source, 1, url, sizeof url);
  assert(strstr(url, "&vote_average.gte=7.5") && strstr(url, "&vote_average.lte=9"));
  assert(strstr(url, "&vote_count.gte=50"));
  assert(strstr(url, "&primary_release_date.gte=2026-01-01"));
  assert(strstr(url, "&primary_release_date.lte=2026-10-01"));
  assert(strstr(url, "&watch_region=GB") && strstr(url, "&without_watch_providers=10"));
  assert(strstr(url, "&with_watch_monetization_types=flatrate|free|ads|rent|buy"));
  puts("kaptain: real New Movies/New Series sources build official discover URLs");
}
