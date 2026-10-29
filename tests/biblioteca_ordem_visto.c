// Ordenacao "Assistidos primeiro" / "Nao assistidos primeiro" da Biblioteca
// (#352): duas faixas por cat_visto, ESTAVEL (a ordem de adicao segue dentro de
// cada faixa). Inclui o .c para ler o `filtro` estatico.
#include "../src/biblioteca.c"
#include <assert.h>

static void ordenar(int o) { ordem = o; reconstruir(); }
static int ids(char *dst) {
  int i, k = 0;
  for (i = 0; i < nFiltro; i++) dst[k++] = (char)('0' + (filtro[i] % 10));
  dst[k] = 0;
  return k;
}

int main(void) {
  CatItem v[6];
  char s[16];
  int i;
  dados_iniciar(getenv("NUVIO_DADOS"));
  memset(v, 0, sizeof v);
  for (i = 0; i < 6; i++) {
    snprintf(v[i].imdb, sizeof v[i].imdb, "tt77000%d", i);
    snprintf(v[i].titulo, sizeof v[i].titulo, "T%d", i);
    snprintf(v[i].tipo, sizeof v[i].tipo, "movie");
    snprintf(v[i].meta, sizeof v[i].meta, "20%02d", 10 + i);
    v[i].naLista = 1;
    v[i].listaFontes = 1u << AJ_SALVOS_LOCAL;
  }
  cat_definir(v, 6);
  cat_historico_definir_id("tt770001", "movie", 1);
  cat_historico_definir_id("tt770004", "movie", 1);
  assert(cat_visto(cat_item(1)) && cat_visto(cat_item(4)) && !cat_visto(cat_item(0)));

  ordenar(ORD_ADICIONADOS); ids(s); assert(!strcmp(s, "012345"));
  ordenar(ORD_VISTO);       ids(s); assert(!strcmp(s, "140235"));
  ordenar(ORD_NAO_VISTO);   ids(s); assert(!strcmp(s, "023514"));
  // Ninguem visto: a ordem original, intacta.
  cat_historico_definir_id("tt770001", "movie", 0);
  cat_historico_definir_id("tt770004", "movie", 0);
  ordenar(ORD_VISTO);       ids(s); assert(!strcmp(s, "012345"));
  assert(BIB_N_ORD == 5);
  printf("biblioteca_ordem_visto: ok\n");
  return 0;
}
