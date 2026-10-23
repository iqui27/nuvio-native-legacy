// Real manifest capability parser, guide manifest probing, and catalog reader.
#include "stub_perfisessao.h"
#include "../src/guia.c"
#include <assert.h>
int ajustes_livetv_sugerir(void) { return 1; }
const char *livecat_secao(const char *name, int *resolved) { (void)name; *resolved = 0; return ""; }

#define BASE "https://plain-tv.invalid"
static int manifests, catalogs;
static char *fixture(const char *name) {
  char path[200], buffer[4096];
  snprintf(path, sizeof path, "tests/fixtures/plain_tv/%s.json", name);
  FILE *f = fopen(path, "rb");
  assert(f);
  size_t n = fread(buffer, 1, sizeof buffer - 1, f);
  assert(!ferror(f) && feof(f)); fclose(f); buffer[n] = 0;
  return strdup(buffer);
}
char *rede_baixar_medido_controle(const char *url, int seconds,
                                const char *const *headers,
                                const RedeControle *control, RedeMedida *measurement) {
  (void)seconds; (void)headers; (void)control; (void)measurement;
  if (!strcmp(url, BASE "/manifest.json")) { manifests++; return fixture("manifest"); }
  assert(!strcmp(url, BASE "/catalog/tv/x.json"));
  catalogs++; return fixture("catalog");
}
const char *rede_url_publica(const char *url, char *out, unsigned size) {
  snprintf(out, size, "%s", url); return out;
}
void fontecache_vod_limpar(void) {}
const char *sessao_usuario(void) { return ""; }
int perfis_ativo(void) { return 1; }
const char *i18n(const char *text) { return text; }
int xtream_e_id(const char *id) { return id && !strncmp(id, "xtream:", 7); }

int main(void) {
  assert(addons_adicionar("Plain TV", BASE "/manifest.json"));
  char *manifest = fixture("manifest");
  addons_manifesto_lido(0, manifest); free(manifest);
  assert(addons_tem_catalogo(0) && addons_fornece(0, ADD_STREAM));
  assert(addons_aceita_id(0, "tv", "x:1") == 1);
  assert(addons_aceita_id(0, "movie", "x:1") == 0);
  GAddonCarga addon = {0};
  strcpy(addon.base, BASE); strcpy(addon.nome, "Plain TV"); addon.ativo = 1;
  addon.nCatalogos = addons_catalogos_canal(0, addon.catalogos, ADD_CANAL_MAX);
  assert(addon.nCatalogos == 1);
  assert(!strcmp(addon.catalogos[0].tipo, "tv"));
  assert(!strcmp(addon.catalogos[0].id, "x") && !strcmp(addon.catalogos[0].nome, "X"));
  addonsCarga = &addon; nAddonsCarga = 1;
  // Both startup paths: cached manifest and direct manifest probing, no Home rows.
  for (int cached = 1; cached >= 0; cached--) {
    addon.nCatalogos = cached ? 1 : -1;
    cats = NULL;
    stagingZerar(); sTeto = GUIACORTE_MAX_CANAL; sNFontes = 0;
    assert(sFonte(LTV_ADDON, BASE, "Plain TV", BASE) == 0);
    sondaManifestos();
    assert(sNFontes == 1 && sNSabe == 1 && sSabe[0].canal == 1);
    int added = 0;
    assert(lerPagina(&sFontes[0], 0, 0, &added) == 3);
    assert(added == 3 && sNCanais == 3);
    cats = sCats;
    for (int i = 0; i < 3; i++) {
      CatItem item;
      canalParaItem(&sCanais[i], &item);
      assert(!strcmp(item.tipo, "tv"));
      assert(!strcmp(sCanais[i].base, BASE));
    }
    assert(!strcmp(sCanais[0].id, "x:1") && sCanais[0].logo[0]);
    assert(!strcmp(sCanais[1].id, "x:2") && sCanais[1].logo[0]);
    assert(!strcmp(sCanais[2].id, "x:3") && !sCanais[2].logo[0]);
  }
  assert(manifests == 1 && catalogs == 2);
  puts("PASS: plain TV channels reach guide through cached and uncached manifests");
  return 0;
}
