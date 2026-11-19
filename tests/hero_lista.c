// #797: ajustes_hero_lista mapeia 0/1/2 para 10, 5 e 20 titulos no carrossel.
#include "../src/ajustes.c"
#include <assert.h>

int main(void) {
  valor[AJ_HERO_LISTA] = 0;
  assert(ajustes_hero_lista() == 10);
  valor[AJ_HERO_LISTA] = 1;
  assert(ajustes_hero_lista() == 5);
  valor[AJ_HERO_LISTA] = 2;
  assert(ajustes_hero_lista() == 20);
  valor[AJ_HERO_LISTA] = 9;
  assert(ajustes_hero_lista() == 10);
  puts("ok hero_lista 5/10/20");
  return 0;
}
