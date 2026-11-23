// F61 / biblioteca com item NULL (PR734AB-r2, pendente): em montarBiblioteca o
// laco de posicao das ordenacoes A-Z e Ano lia outro->titulo/outro->meta sem
// checar NULL — cat_item devolve NULL no meio de uma troca do catalogo (o
// comentario do laco de cima ja dizia isso). A-Z na base: rc 139.
//
// cat_item vai a NULL a partir de nuloDesde (o catalogo publica n=0 entre um
// item e outro), como na troca real.
//
//   bash tests/bib_null.sh
#include "../src/home.c"
#include <assert.h>
#include <stdio.h>

const CatItem *__real_cat_item(int i);
static int chamadas, nuloDesde = 1 << 30;
const CatItem *__wrap_cat_item(int i) { return ++chamadas >= nuloDesde ? NULL : __real_cat_item(i); }
int __wrap_fil_ordem(const char *c) { (void)c; return ORDEM_PROBE; }
int __wrap_ajustes_fonte_biblioteca(void) { return 0; }
int __wrap_ov_ativa(const char *c) { (void)c; return 0; }
int __wrap_ajustes_itens_fileira(void) { return 20; }

int main(void) {
  static CatItem c[2]; static Fileira f;
  snprintf(c[0].imdb, sizeof c[0].imdb, "tt0000001"); snprintf(c[0].titulo, sizeof c[0].titulo, "B");
  snprintf(c[1].imdb, sizeof c[1].imdb, "tt0000002"); snprintf(c[1].titulo, sizeof c[1].titulo, "A");
  for (int i = 0; i < 2; i++) { snprintf(c[i].tipo, sizeof c[i].tipo, "movie"); c[i].naLista = 1; c[i].listaFontes = 1; }
  cat_definir_tudo(c, 2, NULL, 0);
  montarBiblioteca(&f); printf("normal: n=%d\n", f.n); fflush(stdout);
  if (f.n != 2) return 1;
  chamadas = 0; nuloDesde = 3;   // item 0 ok, item 1 ok, depois a troca publica n=0
  montarBiblioteca(&f); printf("swap: n=%d (sobreviveu)\n", f.n);
  return 0;
}
