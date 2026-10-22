// Hero preview actions keep their captured target while Home rotates underneath.
#include "../src/home.c"
#include <assert.h>
static GlanceFonte preview;
int amigosfil_indice_cat(int column) { (void)column; return -1; }
int cat_copiar_item(int i, CatItem *out) {
  memset(out, 0, sizeof *out);
  out->tmdb = i + 1;
  return 1;
}
void glance_abrir(const GlanceFonte *source, int i) { (void)i; preview = *source; }
int main(void) {
  CatItem title;
  HomeItem focused;
  focoHero = 1; heroAtual = 0;
  itemFoco = (HomeItem){.indice = 0, .titulo = "Title A"}; temItemFoco = 1;
  previaHome();
  assert(preview.item(0, &title) && title.tmdb == 1);
  // Both Details and Watch use this adapter callback after preview closes.
  for (int action = 0; action < 2; action++) {
    heroAtual = 1;
    itemFoco = (HomeItem){.indice = 1, .titulo = "Title B"};
    preview.detalhes(0);
    assert(home_pediu_abrir());
    assert(home_item_focado(&focused) && focused.indice == 0);
    assert(!strcmp(focused.titulo, "Title A"));
    assert(preview.item(0, &title) && title.tmdb == 1);
  }
  puts("glance-home: ok");
  return 0;
}
