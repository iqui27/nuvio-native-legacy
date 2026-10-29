// Empty addon cast must fall back to TMDB without live network requests.
#define main detail_main
#include "detalheanime.c"
#undef main

static const char *credits =
  "{\"cast\":[{\"id\":101,\"name\":\"Actor One\",\"character\":\"First\","
  "\"profile_path\":\"/one.jpg\"}]}";

static void setup(const char *regular, const char *aggregate) {
  nRotas = nPedidos = 0;
  rota("/find/tt9561862?", "{\"tv_results\":[{\"id\":86831}],\"movie_results\":[{\"id\":86831}]}");
  rota("/credits?", regular);
  rota("/aggregate_credits?", aggregate);
}

int main(void) {
  static CatItem d;
  fakeTmdbElenco = 1;
  snprintf(tmdbChave, sizeof tmdbChave, "%s", "01234567890123456789012345678901");
  char aggregate[4096];
  size_t used = (size_t)snprintf(aggregate, sizeof aggregate, "{\"cast\":[");
  for (int i = 0; i < CAT_ELENCO_MAX + 2; i++)
    used += (size_t)snprintf(aggregate + used, sizeof aggregate - used,
      "%s{\"id\":%d,\"name\":\"Actor %d\",\"profile_path\":\"/actor.jpg\","
      "\"roles\":[{\"character\":\"First role\"},{\"character\":\"Second role\"}]}",
      i ? "," : "", 200 + i, i);
  snprintf(aggregate + used, sizeof aggregate - used, "]}");

  // Reproduce opening Love, Death & Robots from an addon with no cast names.
  fakeMetaExterno = 1;
  nFake = 1;
  fake[0] = (FakeAddon){"Metadata", "https://metadata.test", "metadata", "tt", 0};
  setup("{\"cast\":[]}", aggregate);
  rota("metadata.test/meta/series/tt9561862.json",
    "{\"meta\":{\"id\":\"tt9561862\",\"type\":\"series\","
    "\"name\":\"Love, Death & Robots\",\"cast\":[],\"director\":[\"Tim Miller\"]}}");
  rota("catalog.nuvio.tv", NULL);
  catalogoDe("tt9561862", "series", "Love, Death & Robots", "metadata");
  abrir();
  const CatItem *opened = cat_item(0);
  assert(opened->nElenco == CAT_ELENCO_MAX);
  assert(!strcmp(opened->elenco[0].nome, "Actor 0"));
  assert(!strcmp(opened->elenco[0].papel, "First role"));
  assert(opened->elenco[0].tmdb == 200);
  assert(opened->elenco[CAT_ELENCO_MAX - 1].tmdb == 200 + CAT_ELENCO_MAX - 1);
  assert(!strcmp(opened->elenco[0].foto, "https://image.tmdb.org/t/p/w185/actor.jpg"));
  assert(pediu("/tv/86831/aggregate_credits?"));

  // Movies and TV with ordinary credits need no aggregate request.
  for (int tv = 0; tv < 2; tv++) {
    memset(&d, 0, sizeof d);
    fichaDe(&d, "{\"cast\":[]}", 0);
    setup(credits, aggregate);
    fotosDoElenco(&d, "tt9561862", tv, DESC_MANTER_ELENCO);
    assert(d.nElenco == 1 && d.elenco[0].tmdb == 101);
    assert(!strcmp(d.elenco[0].papel, "First"));
    assert(strstr(d.elenco[0].foto, "/one.jpg"));
    assert(nPedidos == 3 && !pediu("/aggregate_credits?"));
    assert(pediu(tv ? "/tv/86831/credits?" : "/movie/86831/credits?"));
  }

  // Existing addon names keep their order, portraits, roles and request count.
  memset(&d, 0, sizeof d);
  fichaDe(&d, "{\"app_extras\":{\"cast\":[{\"name\":\"Actor One\","
    "\"character\":\"Addon role\",\"photo\":\"https://addon.test/one.jpg\"}]}}", 0);
  setup(credits, aggregate);
  fotosDoElenco(&d, "tt9561862", 1, DESC_MANTER_ELENCO);
  assert(d.nElenco == 1 && d.elenco[0].tmdb == 101);
  assert(!strcmp(d.elenco[0].papel, "Addon role"));
  assert(!strcmp(d.elenco[0].foto, "https://addon.test/one.jpg"));
  assert(nPedidos == 3 && !pediu("/aggregate_credits?"));
  setup("{\"cast\":[]}", aggregate);
  fotosDoElenco(&d, "tt9561862", 1, DESC_MANTER_ELENCO);
  assert(d.nElenco == 1 && nPedidos == 3 && !pediu("/aggregate_credits?"));

  // Disabled credits and empty movie credits never try the TV endpoint.
  for (int enabled = 0; enabled < 2; enabled++) {
    fakeTmdbElenco = enabled;
    memset(&d, 0, sizeof d);
    setup("{\"cast\":[]}", aggregate);
    fotosDoElenco(&d, "tt9561862", enabled ? 0 : 1, 0);
    assert(d.nElenco == 0);
    assert(nPedidos == (enabled ? 3 : 2));
    assert(!pediu("/aggregate_credits?"));
  }
  limparCacheMeta();
  puts("cast_empty: PASS (offline fallback, aggregate roles, person IDs, cap, existing cast, settings)");
  return 0;
}
