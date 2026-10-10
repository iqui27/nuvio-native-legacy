// Regressao com IDs e metadados sinteticos, nao reproducao dos dados da TV.
// Inclui a montagem real; nao precisa de GL, rede ou dados pessoais.
#ifndef BIB_FONTE
#define BIB_FONTE "../src/biblioteca.c"
#endif
#include BIB_FONTE
#include <assert.h>
#include <time.h>

static CatItem catalogoTeste[127];
static int ocultar;
int cat_n(void) { return 127; }
const CatItem *cat_item(int i) { return i >= 0 && i < 127 ? &catalogoTeste[i] : NULL; }
int cat_indice_por_imdb(const char *id) {
  for (int i = 0; i < 127; i++)
    if (salvos_mesmo_titulo(id, catalogoTeste[i].imdb)) return i;
  return -1;
}
int ajustes_ocultar_nao_lancados(void) { return ocultar; }
void ajustes_area_conteudo(float e, float d, float *x, float *w) {
  if (x) *x = e;
  if (w) *w = 1920 - e - d;
}
int perfis_ativo(void) { return 1; }
int lst_n(void) { return 0; }
int lst_itens_n(void) { return 0; }
char *dados_ler(const char *nome) {
  char b[2048] = "";
  if (strcmp(nome, "salvos-p1.txt")) return NULL;
  // 9 locais ja no catalogo, 3 fora; 1 dos de fora tem meta.
  for (int i = 0; i < 12; i++) {
    int id = i < 9 ? i : 118 + i;
    size_t k = strlen(b);
    snprintf(b + k, sizeof b - k, "tt%07d\tmovie\t0\t0\t%s\t\tLocal %d\n",
             id, i == 9 ? "2020" : "", i);
  }
  return strdup(b);
}
int dados_gravar(const char *n, const char *s) { (void)n; (void)s; return 1; }
int dados_apagar(const char *n) { (void)n; return 1; }

static void confere(int esperado) {
  reconstruir();
  int total = modo == MODO_SALVOS ? 129 : 126;
  assert(contaModo[MODO_SALVOS] == 129);
  assert(contaModo[MODO_NUVEM] == 126);
  assert(totalModo == total && nFiltro == esperado && nCelulas == esperado);
  int navegaveis = 0;
  for (int r = BIB_FIL_GRADE; r < foco.nFileiras; r++) navegaveis += foco.nColunas[r];
  assert(navegaveis == esperado);
  char num[16];
  textoModo(MODO_SALVOS, num, sizeof num);
  assert(!strcmp(num, "129"));
  printf("fixture: Salvos=%s grade=%d navegaveis=%d ocultar=%d tipo=%d exibicao=%d\n",
         num, nFiltro, navegaveis, ocultar, tipo, exibicao);
}

int main(void) {
  // 126 titulos unicos do catalogo, mais uma copia de episodio.
  // 7 deles com meta; 8 filmes ao todo. Os numeros nao sao dados da TCL.
  for (int i = 0; i < 126; i++) {
    snprintf(catalogoTeste[i].imdb, sizeof catalogoTeste[i].imdb, "tt%07d", i);
    snprintf(catalogoTeste[i].tipo, sizeof catalogoTeste[i].tipo, "%s", i < 8 ? "movie" : "series");
    catalogoTeste[i].naLista = 1;
    catalogoTeste[i].naColecao = 1;
    if (i < 7) strcpy(catalogoTeste[i].meta, "2020");
  }
  catalogoTeste[126] = catalogoTeste[125];
  strcat(catalogoTeste[126].imdb, ":1:2");
  salvos_iniciar();
  modo = MODO_SALVOS; tipo = TIPO_TODOS;
  ocultar = 0; confere(129);
  ocultar = 1; confere(129);
  exibicao = VIS_LISTA; confere(129);
  tipo = TIPO_FILME; confere(11);
  tipo = TIPO_SERIE; confere(118);
  tipo = TIPO_TODOS;

  // Mesmo criterio da Home: desconhecido/passado/ano atual ficam; futuro sai.
  time_t agora = time(NULL);
  struct tm tmv;
  assert(gmtime_r(&agora, &tmv));
  char atual[16], futuro[32];
  snprintf(atual, sizeof atual, "%d", tmv.tm_year + 1900);
  snprintf(futuro, sizeof futuro, "%d · 120 min", tmv.tm_year + 1901);
  const char *metas[] = { "", "2020", atual, "Sem data", "99999", futuro };
  CatItem local = {0};
  strcpy(local.imdb, "tt0000129");
  strcpy(local.tipo, "movie");
  for (int i = 0; i < 6; i++) {
    strcpy(catalogoTeste[7].meta, metas[i]);
    strcpy(local.meta, metas[i]);
    assert(salvos_definir(&local, 0));
    assert(salvos_definir(&local, 1));
    for (exibicao = VIS_CARTAZ; exibicao <= VIS_LISTA; exibicao++) {
      modo = MODO_SALVOS;
      ocultar = 1; confere(i == 5 ? 127 : 129);
      ocultar = 0; confere(129);
      modo = MODO_NUVEM;
      ocultar = 1; confere(i == 5 ? 125 : 126);
      ocultar = 0; confere(126);
    }
  }
  modo = MODO_SALVOS; exibicao = VIS_LISTA;
  ocultar = 0; confere(129);
  tipo = TIPO_FILME; confere(11);
  tipo = TIPO_SERIE; confere(118);
  puts("biblioteca filtro: PASS (regressao sintetica; sem prova da TV)");
  return 0;
}
