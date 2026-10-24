// Saved public TVPlay responses through real discovery, catalog and metadata code.
#define NV_TEST_SESSAO_PROPRIA
#define NV_TEST_ADDONS_PROPRIOS
#define NV_TEST_SOCIAL_PROPRIO
#define main montagem_cedo_main
#include "montagem_cedo.c"
#undef main

static int catalogRequests, metaRequests;
static char *fixture(const char *name) {
  char path[160];
  snprintf(path, sizeof path, "tests/fixtures/tvplay/%s.json", name);
  FILE *f = fopen(path, "rb"); assert(f);
  assert(!fseek(f, 0, SEEK_END)); long n = ftell(f); assert(n > 0);
  rewind(f); char *s = malloc(n + 1); assert(s);
  assert(fread(s, 1, n, f) == (size_t)n); fclose(f); s[n] = 0; return s;
}
static char *download(const char *url) {
  if (strstr(url, "/manifest.json")) return fixture("manifest");
  if (strstr(url, "/catalog/tv/tvplay.json")) { catalogRequests++; return fixture("catalog"); }
  if (strstr(url, "/catalog/tv/")) return strdup("{\"metas\":[]}");
  if (strstr(url, "/meta/")) {
    assert(!strcmp(url, BASE "/meta/tv/ecipfs-aee.json")); metaRequests++;
    return strdup("{\"meta\":{\"id\":\"ecipfs-aee\",\"type\":\"tv\",\"name\":\"A&E\",\"description\":\"Live channel metadata\",\"poster\":\"https://fixture.invalid/logo.png\"}}");
  }
  return NULL;
}
int main(void) {
  fixtureRede = download; fixtureMeta = 1;
  snprintf(dirDados, sizeof dirDados, "%snuvio-tvplay-%d", nvt("/"), (int)getpid());
  assert(mkdir(dirDados, 0700) == 0);
  desc_tmdb(dirDados); homeestado_iniciar(); desc_iniciar(); esperarQuieto();
  assert(catalogRequests > 0 && temFileira("tvplay"));
  int index = -1;
  for (int r = 0; r < cat_n_fileiras(); r++) {
    const CatFileira *row = cat_fileira(r);
    if (!strcmp(row->catId, "tvplay")) { assert(row->n > 0 && !strcmp(row->tipo, "tv")); index = row->ini; }
  }
  assert(index >= 0);
  // Parse all 156 entries; catalog projection deduplicates the repeated TNT ID.
  CatItem *all = calloc(156, sizeof *all); int responded = 0;
  assert(lerCatalogo(BASE, "tv", "tvplay", all, 156, 156, &responded) == 156);
  assert(responded); free(all);
  desc_vertudo_abrir(BASE, "tv", "tvplay");
  while (desc_vertudo_carregando()) usleep(2000);
  assert(desc_vertudo_n() == 155);
  CatItem item; assert(desc_vertudo_item(0, &item));
  assert(!strcmp(item.tipo, "tv") && !strcmp(item.imdb, "ecipfs-aee"));
  assert(cat_copiar_item(index, &item)); item.sinopse[0] = 0;
  cat_atualizar_item(index, &item);
  desc_episodios(index, 0);
  while (desc_episodios_carregando(index)) { desc_episodios_pendente(); usleep(2000); }
  assert(metaRequests == 1);
  assert(cat_copiar_item(index, &item));
  assert(!strcmp(item.tipo, "tv") && !strcmp(item.sinopse, "Live channel metadata"));
  char command[PATH_MAX + 16]; snprintf(command, sizeof command, "rm -rf '%s'", dirDados);
  assert(system(command) == 0);
  puts("PASS: TVPlay Home row, 156 entries / 155 unique catalog items, detail meta/tv identity");
}
