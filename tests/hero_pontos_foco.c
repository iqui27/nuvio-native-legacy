// #348: regressao documental — desenhaPontosHero nao desenha com n<=1.
#include <assert.h>
#include <stdio.h>

static int chamadas;
static void desenhaPontosHero(float xDir, float yc, int n, int atual, float a) {
  (void)xDir; (void)yc; (void)atual;
  if (n <= 1 || a <= 0.004f) return;
  chamadas++;
}

int main(void) {
  desenhaPontosHero(0, 0, 1, 0, 1.0f);
  assert(chamadas == 0);
  desenhaPontosHero(0, 0, 4, 1, 1.0f);
  assert(chamadas == 1);
  puts("ok hero_pontos_foco gate");
  return 0;
}
