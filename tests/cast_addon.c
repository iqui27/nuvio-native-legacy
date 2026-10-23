// #528: addon portraits must survive unavailable optional TMDB enrichment.
#define main detail_main
#include "detalheanime.c"
#undef main


static int sawTmdb;
static void beforeTmdb(const char *url) {
  if (!strstr(url, "api.themoviedb.org")) return;
  sawTmdb = 1;
  const CatItem *d = cat_item(0);
  assert(d->nElenco == 8);
  assert(strstr(d->elenco[0].foto, "2nOPPPxPjz11N1RcpDXiTiyDYv2.jpg"));
}

int main(void) {
  FILE *f = fopen("tests/fixtures/farah/nuvio_meta_tt21874046.json", "rb");
  assert(f && !fseek(f, 0, SEEK_END));
  long size = ftell(f);
  assert(size > 0 && !fseek(f, 0, SEEK_SET));
  char *meta = malloc((size_t)size + 1);
  assert(meta && fread(meta, 1, (size_t)size, f) == (size_t)size);
  meta[size] = 0;
  fclose(f);
  static CatItem d;
  fichaDe(&d, meta, 0);
  assert(d.nElenco == 8);
  assert(!strcmp(d.elenco[0].nome, "Demet Özdemir"));
  assert(!strcmp(d.elenco[0].papel, "Farah Ershadi"));
  assert(!strcmp(d.elenco[0].foto,
    "https://image.tmdb.org/t/p/w500/2nOPPPxPjz11N1RcpDXiTiyDYv2.jpg"));
  assert(d.elenco[0].tmdb == 0); // Never invent a person ID from a title ID.

  // Complement names are reordered; match portraits by name, preserve primary.
  memset(&d, 0, sizeof d);
  fichaDe(&d, "{\"cast\":[\"Engin Akyürek\",\"Demet Özdemir\",\"Unknown\"]}", 0);
  fichaDe(&d, meta, 1);
  assert(!strcmp(d.elenco[0].nome, "Engin Akyürek"));
  assert(strstr(d.elenco[0].foto, "oynWT3byuD2t6DUaCkS9dr84cqQ.jpg"));
  assert(!d.elenco[2].foto[0]);
  fichaDe(&d, "{\"app_extras\":{\"cast\":[{\"name\":\"Engin Akyürek\","
    "\"photo\":\"https://other.test/portrait.jpg\"}]}}", 1);
  assert(strstr(d.elenco[0].foto, "oynWT3byuD2t6DUaCkS9dr84cqQ.jpg"));

  // Full detail: metadata provider succeeds, every TMDB request fails offline.
  fakeMetaExterno = 1;
  snprintf(tmdbChave, sizeof tmdbChave, "%s", "01234567890123456789012345678901");
  nFake = 1;
  fake[0] = (FakeAddon){"Metadata", "https://metadata.test", "metadata", "tt", 0};
  rota("metadata.test/meta/series/tt21874046.json", meta);
  const char *namesOnly = "{\"meta\":{\"id\":\"tt21874046\",\"type\":\"series\","
    "\"name\":\"Farah\",\"cast\":[\"Demet Özdemir\",\"Engin Akyürek\",\"Fırat Tanış\","
    "\"Feyyaz Duman\",\"Burcu Türünz\",\"Hatice Aslan\",\"Sera Kutlubey\",\"Burak Tamdoğan\"]}}";
  rota("catalog.nuvio.tv", namesOnly);
  pedidoMetaHook = beforeTmdb;
  for (int origin = 0; origin < 2; origin++) {
    limparCacheMeta(); metaprov_zerar_pausa(); sawTmdb = 0;
    catalogoDe("tt21874046", "series", "Farah", origin ? "metadata" : "cinemeta");
    abrir();
    assert(sawTmdb);
    assert(cat_item(0)->nElenco == 8);
    assert(strstr(cat_item(0)->elenco[0].foto, "2nOPPPxPjz11N1RcpDXiTiyDYv2.jpg"));
  }
  pedidoMetaHook = NULL;
  limparCacheMeta(); free(meta);
  puts("cast_addon: PASS (portraits, name matching, early publication, TMDB unavailable)");
  return 0;
}
