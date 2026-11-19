// #797: mapeamento heroListaLocal 0/1/2 -> 10/5/20 (espelha ajustes_hero_lista).
#include <assert.h>
#include <stdio.h>

static int hero_lista(int v) {
  static const int n[] = { 10, 5, 20 };
  return (v >= 0 && v < 3) ? n[v] : 10;
}

int main(void) {
  assert(hero_lista(0) == 10);
  assert(hero_lista(1) == 5);
  assert(hero_lista(2) == 20);
  assert(hero_lista(9) == 10);
  puts("ok hero_lista 5/10/20");
  return 0;
}
