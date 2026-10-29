// A server refresh can replace a row without changing catalogue/row counts.
#define main previous_fixture_main
#include "cwordem_home.c"
unsigned cf_revisao(void) { return 0; }
int cf_n(void) { return 0; }
const ColFolder *cf_pasta(int i) { (void)i; return NULL; }
int cf_itens(const ColFolder *p, int *idx, int max) { (void)p; (void)idx; (void)max; return 0; }
int ov_ativa(const char *chave) { (void)chave; return 0; }
unsigned ov_revisao(void) { return 0; }
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
  row.ini = 0; row.n = 3;
  strcpy(items[0].titulo,"Zulu"); strcpy(items[1].titulo,"Alpha"); strcpy(items[2].titulo,"Beta");
  cat_definir_tudo(items,3,&row,1);
  fil_registrar(row.chave,row.titulo,"Jellyfin","movie",3);
  assert(fil_definir_servidor(row.chave,1,0));
  sincronizarFileiras();
  int found=-1; for(int i=0;i<nFileiras;i++)if(!strcmp(fileiras[i].chave,row.chave))found=i;
  assert(found>=0 && fileiras[found].usaItens && fileiras[found].itens[0]==1 && fileiras[found].itens[2]==0);
  puts("servers hero: recent refresh and persisted server title ordering: OK");
  return 0;
}
