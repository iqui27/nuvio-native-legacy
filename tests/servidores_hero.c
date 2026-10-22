// A server refresh can replace a row without changing catalogue/row counts.
#define main previous_fixture_main
#include "cwordem_home.c"
#undef main

int addons_n(void) { return 0; }
const char *addons_base(int i) { (void)i; return ""; }
const char *addons_id_manifesto(int i) { (void)i; return ""; }
const char *sessao_usuario(void) { return ""; }

int main(void) {
  static CatItem items[3];
  static CatFileira row;
  for (int i = 0; i < 3; i++) {
    snprintf(items[i].imdb, sizeof items[i].imdb, "tt%d", i + 1);
    snprintf(items[i].titulo, sizeof items[i].titulo, "Fixture %d", i + 1);
    snprintf(items[i].tipo, sizeof items[i].tipo, "movie");
  }
  snprintf(row.chave, sizeof row.chave, "jellyfin_recent");
  snprintf(row.titulo, sizeof row.titulo, "Recent fixture");
  snprintf(row.tipo, sizeof row.tipo, "movie");
  row.ini = 1; row.n = 1;
  cat_definir_tudo(items, 3, &row, 1);
  fil_definir_hero_fonte(row.chave);
  sincronizarFileiras();
  assert(heroNLista() == 1 && heroIdxEm(0) == 1);
  row.ini = 2;
  cat_definir_tudo(items, 3, &row, 1);
  sincronizarFileiras();
  if (heroNLista() != 1 || heroIdxEm(0) != 2) {
    fputs("FAIL: server refresh leaves previous recent title in hero\n", stderr);
    return 1;
  }
  puts("servers hero: recent-row refresh with unchanged counts: OK");
  return 0;
}
