// Exercise the production Search adapter after delayed People row insertion.
#include "../src/busca.c"
#include <assert.h>
static GlanceFonte preview;
int cat_copiar_item(int i, CatItem *out) {
  memset(out, 0, sizeof *out);
  out->tmdb = i;
  return 1;
}
int buscasrec_registrar(const char *text) { (void)text; return 1; }
void glance_abrir(const GlanceFonte *source, int i) { (void)i; preview = *source; }
int main(void) {
  CatItem out;
  nFil = 2;
  fil[0].n = 1; fil[0].itens[0] = 3;
  fil[1].n = 2; fil[1].itens[0] = 7; fil[1].itens[1] = 9;
  focoRes.fileira = 1; focoRes.coluna = 0;
  previaResultado();
  // The People response inserts a row without replacing the catalog.
  fil[2] = fil[1];
  fil[1].pessoas = 1; fil[1].n = 1; fil[1].itens[0] = 0;
  pess[0].tmdb = 100;
  nFil = 3;
  assert(preview.total() == 2);
  assert(preview.item(0, &out) && out.tmdb == 7);
  assert(preview.item(1, &out) && out.tmdb == 9);
  preview.foco(1);
  assert(focoRes.fileira == 2 && focoRes.coluna == 1);
  preview.detalhes(0);
  assert(pedido == 7 && !pedidoPessoa);
  preview.detalhes(1);
  assert(pedido == 9 && !pedidoPessoa);
  puts("glance-search: ok");
  return 0;
}
