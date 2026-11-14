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
  } else if (!strcmp(argv[1], "genres")) {
    // #700: two pinned folders on the SAME base/type/catalog, distinct genres,
    // plus one with a plain source. The discovery mount publishes one
    // synthesized CatFileira per pinned folder, keyed colpin_<id>; the Home
    // merges it with the pseudo-row cf_itens builds.
    assert(col_definir_json("{\"collections\":[{\"id\":\"group\",\"title\":\"Group\",\"folders\":["
      "{\"id\":\"ac\",\"title\":\"Action\",\"tileShape\":\"POSTER\",\"sources\":[{\"addonBaseUrl\":\"https://example.invalid/addon\",\"type\":\"movie\",\"catalogId\":\"id0\",\"genre\":\"Action\"}]},"
      "{\"id\":\"co\",\"title\":\"Comedy\",\"sources\":[{\"addonBaseUrl\":\"https://example.invalid/addon\",\"type\":\"movie\",\"catalogId\":\"id0\",\"genre\":\"Comedy\"},"
      "{\"addonBaseUrl\":\"https://example.invalid/addon\",\"type\":\"movie\",\"catalogId\":\"id1\"}]}]}]}"));
    assert(col_n() == 2);
    const ColFolder *fac = NULL, *fco = NULL;
    // col_folder(i) iterates ALL folders of the collection; locate the two.
    for (int i = 0; i < col_n(); i++) {
      const ColFolder *g = col_folder(i);
      if (!g) continue;
      if (!strcmp(g->id, "ac")) fac = g;
      if (!strcmp(g->id, "co")) fco = g;
    }
    assert(fac && fco);
    assert(cf_alternar(fac) && cf_alternar(fco));
    // The genre-bearing folders have no usable window in the plain store:
    // cf_itens returns 0 (never serves the unfiltered catalogue as theirs)
    // and the Home keeps the collection card for each.
    int idx[24];
    assert(cf_itens(fac, idx, 24) == 0);
    assert(cf_itens(fco, idx, 24) == 0);
    sincronizarFileiras();
    assert(fileiras[row("colpin_ac")].tipo == FILEIRA_CATALOGOS);
    assert(fileiras[row("colpin_co")].tipo == FILEIRA_CATALOGOS);
    // Now the discovery-side synthesized rows arrive: one CatFileira per
    // pinned folder, colpin_<id>, with its own genre window (and, for "co",
    // the plain-source window merged in). The catalog block becomes 9 old
    // items + 2 (action) + 3 (comedy + plain row tt3-tt5... see below).
    {
      static CatItem it2[32];
      CatFileira rf[6] = {0};
      memcpy(it2, items, sizeof items);
      // action genre window: two new ids
      snprintf(it2[9].imdb,  sizeof it2[9].imdb,  "tta0");
      snprintf(it2[10].imdb, sizeof it2[10].imdb, "tta1");
      for (int i = 9; i <= 10; i++) snprintf(it2[i].tipo, sizeof it2[i].tipo, "movie");
      // comedy genre window (ttc0) + plain id1 row window (tt3..tt5)
      snprintf(it2[11].imdb, sizeof it2[11].imdb, "ttc0");
      snprintf(it2[11].tipo, sizeof it2[11].tipo, "movie");
      // keep the three old catalog rows as they were
      memcpy(rf, rows, sizeof rows);
      // synthesized colpin rows, appended by montar
      snprintf(rf[3].chave, sizeof rf[3].chave, "colpin_ac");
      snprintf(rf[3].titulo, sizeof rf[3].titulo, "Action");
      rf[3].ini = 9; rf[3].n = 2;
      snprintf(rf[4].chave, sizeof rf[4].chave, "colpin_co");
      snprintf(rf[4].titulo, sizeof rf[4].titulo, "Comedy");
      rf[4].ini = 11; rf[4].n = 1 + 3;   // ttc0 + tt3..tt5 copied from row id1
      memcpy(&it2[12], &items[3], sizeof(CatItem) * 3);
      cat_definir_tudo(it2, 15, rf, 5);   // synthetic rebuild/resync
      filsAplicadas = -1; sincronizarFileiras();
      // Each pinned row draws its OWN window once; no pseudo-row remains
      // and the two folders never share items.
      int ra = row("colpin_ac"), rc = row("colpin_co");
      assert(ra >= 0 && rc >= 0 && ra != rc);
      assert(fileiras[ra].tipo != FILEIRA_CATALOGOS && fileiras[ra].n == 2);
      assert(!strcmp(cat_item_exato(fileiraItemIndice(&fileiras[ra], 0))->imdb, "tta0"));
      assert(fileiras[rc].tipo != FILEIRA_CATALOGOS && fileiras[rc].n == 4);
      assert(!strcmp(cat_item_exato(fileiraItemIndice(&fileiras[rc], 0))->imdb, "ttc0"));
      // Exactly ONE row per colpin key (dedupe dropped the pseudo-rows).
      int dup = 0;
      for (int i = 0; i < nFileiras; i++)
        if (!strcmp(fileiras[i].chave, "colpin_ac") || !strcmp(fileiras[i].chave, "colpin_co")) dup++;
      assert(dup == 2);
      assert(!fileiras[ra].verTudo && !fileiras[rc].verTudo);
      // The kept genre row draws in its folder's layout, like any pinned row.
      assert(fileiras[ra].forma == COL_FORMA_POSTER && fileiras[rc].forma == COL_FORMA_QUADRADO);
      // The genre rows survive a republish (synthetic resync): same block.
      cat_definir_tudo(it2, 15, rf, 5);
      filsAplicadas = -1; sincronizarFileiras();
      assert(row("colpin_ac") >= 0 && row("colpin_co") >= 0);
      assert(fileiras[row("colpin_co")].n == 4);
    }
  } else assert(0);
  printf("homefil_home %s: PASS\n", argv[1]);
  return 0;
}
