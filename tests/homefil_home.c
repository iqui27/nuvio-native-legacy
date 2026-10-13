// Real Home/catalogue fixture; no renderer or network.
#define main home_layout_main
#include "home_layout.c"
#undef main

static int row(const char *key) {
  for (int i = 0; i < nFileiras; i++)
    if (!strcmp(fileiras[i].chave, key)) return i;
  return -1;
}

int main(int argc, char **argv) {
  static CatItem items[9];
  CatFileira rows[3] = {0};
  assert(argc == 2);
  homeCom(HOME_LAYOUT_MODERNA, 1);
  fil_definir_limite(3);
  for (int i = 0; i < 3; i++) {
    snprintf(rows[i].chave, sizeof rows[i].chave, "cat%d", i);
    snprintf(rows[i].titulo, sizeof rows[i].titulo, "Catalog %d", i);
    snprintf(rows[i].base, sizeof rows[i].base, "https://example.invalid/addon");
    snprintf(rows[i].tipo, sizeof rows[i].tipo, "movie");
    snprintf(rows[i].catId, sizeof rows[i].catId, "id%d", i);
    rows[i].ini = i * 3; rows[i].n = 3;
  }
  for (int i = 0; i < 9; i++) {
    snprintf(items[i].imdb, sizeof items[i].imdb, "tt%d", i);
    snprintf(items[i].tipo, sizeof items[i].tipo, "movie");
  }
  cat_definir_tudo(items, 9, rows, 3);
  if (!strcmp(argv[1], "quota")) {
    assert(col_definir_json("{\"collections\":[{\"id\":\"group\",\"title\":\"Group\",\"folders\":["
      "{\"id\":\"pin\",\"title\":\"Pinned\",\"sources\":[{\"addonBaseUrl\":\"https://missing.invalid\",\"type\":\"movie\",\"catalogId\":\"missing\"}]}]}]}"));
    assert(col_n() == 1 && cf_alternar(col_folder(0)));
    sincronizarFileiras();
    int pin = homefil_indice("colpin_pin");
    assert(pin >= 0);
    while (pin > 0) pin = fil_mover(pin, -1);
    sincronizarFileiras();
    assert(row("colpin_pin") == 0);
    assert(row("cat0") >= 0 && row("cat1") >= 0 && row("cat2") >= 0);
    assert(fileiras[row("cat2")].n == 3);
  } else if (!strcmp(argv[1], "repin")) {
    assert(col_definir_json("{\"collections\":[{\"id\":\"group\",\"title\":\"Group\",\"folders\":["
      "{\"id\":\"pin\",\"title\":\"Pinned\",\"sources\":[{\"addonBaseUrl\":\"https://missing.invalid\",\"type\":\"movie\",\"catalogId\":\"missing\"}]}]}]}"));
    const ColFolder *p = col_folder(0);
    assert(p && cf_alternar(p));
    sincronizarFileiras();
    assert(row("colpin_pin") >= 0);
    assert(homefil_ocultar("colpin_pin"));
    sincronizarFileiras();
    assert(row("colpin_pin") == -1 && fil_oculta("colpin_pin"));
    assert(!cf_alternar(p));
    sincronizarFileiras();
    assert(cf_alternar(p));
    sincronizarFileiras();
    assert(cf_fixada("pin") && !fil_oculta("colpin_pin"));
    assert(row("colpin_pin") >= 0);
  } else if (!strcmp(argv[1], "watched")) {
    cat_historico_definir_id("tt0", "movie", 1);
    assert(ov_alternar("cat0"));
    sincronizarFileiras();
    assert(fileiras[row("cat0")].n == 2);
    assert(fileiras[row("cat0")].vistosOcultos);
    sincronizarFileiras(); sincronizarFileiras();
    unsigned rev = cat_revisao();
    cat_historico_definir_id("tt1", "movie", 1);
    assert(cat_revisao() == rev);
    sincronizarFileiras();
    assert(fileiras[row("cat0")].n == 1);
    assert(fileiraItemIndice(&fileiras[row("cat0")], 0) == 2);
    cat_historico_definir_id("tt2", "movie", 1);
    sincronizarFileiras();
    assert(fileiras[row("cat0")].n == 3 && !fileiras[row("cat0")].vistosOcultos);
    cat_historico_definir_id("tt1", "movie", 0);
    sincronizarFileiras();
    assert(fileiras[row("cat0")].n == 1 && fileiras[row("cat0")].vistosOcultos);
  } else if (!strcmp(argv[1], "local")) {
    // Source order differs from the catalogue order; repeat source dedupes.
    assert(col_definir_json("{\"collections\":[{\"id\":\"group\",\"title\":\"Group\",\"folders\":["
      "{\"id\":\"pin\",\"title\":\"Pinned\",\"sources\":["
      "{\"addonBaseUrl\":\"https://example.invalid/addon\",\"type\":\"movie\",\"catalogId\":\"id1\"},"
      "{\"addonBaseUrl\":\"https://example.invalid/addon\",\"type\":\"movie\",\"catalogId\":\"id0\"},"
      "{\"addonBaseUrl\":\"https://example.invalid/addon\",\"type\":\"movie\",\"catalogId\":\"id1\"}]}]}]}"));
    assert(col_n() == 1 && cf_alternar(col_folder(0)));
    cat_historico_definir_id("tt3", "movie", 1);
    sincronizarFileiras();
    Fileira *f = &fileiras[row("colpin_pin")];
    assert(f->tipo != FILEIRA_CATALOGOS && f->n == 6 && f->usaItens);
    const int order[] = {3, 4, 5, 0, 1, 2};
    for (int i = 0; i < 6; i++) assert(fileiraItemIndice(f, i) == order[i]);
    assert(cat_visto(cat_item_exato(fileiraItemIndice(f, 0))));
    assert(!cat_visto(cat_item_exato(fileiraItemIndice(f, 1))));
    // A source absent locally falls back to the collection shortcut.
    ColSource *s = col_folder(0)->sources;
    snprintf(s[0].catId, sizeof s[0].catId, "missing");
    cf_definir_perfil(2); cf_definir_perfil(1);
    assert(cf_alternar(col_folder(0)));
    sincronizarFileiras();
    f = &fileiras[row("colpin_pin")];
    assert(f->tipo == FILEIRA_CATALOGOS && f->n == 1);
  } else assert(0);
  printf("homefil_home %s: PASS\n", argv[1]);
  return 0;
}
