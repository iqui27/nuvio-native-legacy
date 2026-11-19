// #348: gate real de desenhaPontosHero (espelha home.c: btnFoco e limiar).
#include <assert.h>
#include <stdio.h>

enum { HOME_LAYOUT_MODERNA = 0, HOME_LAYOUT_PADRAO = 1, HOME_LAYOUT_DINAMICA = 2 };

static int chamadas;

static int btn_foco_hero(int lay, int focoHero) {
  return (lay == HOME_LAYOUT_MODERNA) || focoHero;
}

static int hero_pontos_chamaria(int lay, int focoHero, float aBtn, int n) {
  if (aBtn <= 0.004f || n <= 0)
    return 0;
  if (!btn_foco_hero(lay, focoHero))
    return 0;
  if (n <= 1 || aBtn <= 0.004f)
    return 0;
  return 1;
}

static void desenhaPontosHero(float xDir, float yc, int n, int atual, float a) {
  (void)xDir; (void)yc; (void)atual;
  if (n <= 1 || a <= 0.004f)
    return;
  chamadas++;
}

int main(void) {
  assert(btn_foco_hero(HOME_LAYOUT_MODERNA, 0));
  assert(btn_foco_hero(HOME_LAYOUT_PADRAO, 1));
  assert(!btn_foco_hero(HOME_LAYOUT_PADRAO, 0));
  assert(!btn_foco_hero(HOME_LAYOUT_DINAMICA, 0));

  assert(hero_pontos_chamaria(HOME_LAYOUT_MODERNA, 0, 1.0f, 4));
  assert(!hero_pontos_chamaria(HOME_LAYOUT_PADRAO, 0, 1.0f, 4));
  assert(hero_pontos_chamaria(HOME_LAYOUT_PADRAO, 1, 1.0f, 4));
  assert(!hero_pontos_chamaria(HOME_LAYOUT_DINAMICA, 1, 0.0f, 4));

  chamadas = 0;
  if (hero_pontos_chamaria(HOME_LAYOUT_PADRAO, 1, 1.0f, 4))
    desenhaPontosHero(0, 0, 4, 1, 1.0f);
  assert(chamadas == 1);

  desenhaPontosHero(0, 0, 1, 0, 1.0f);
  assert(chamadas == 1);
  desenhaPontosHero(0, 0, 4, 1, 1.0f);
  assert(chamadas == 2);
  puts("ok hero_pontos_foco gate");
  return 0;
}
