// Resolver em retrato (celular 1080x2400, NV_TOUCH_UI): a grade 1920x1080 tem de
// ser encolhida para caber. Sem janela nem GL. Ver resolver_retrato.sh.
#include "layout.h"
#include "resolver.h"
#include "gfx.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
  float k;
  layout_modo_definir(1);
  layout_tela_definir(1080, 2400);
  k = resolver_escala();
  assert(k > 0.0f && k < 1.0f);
  assert(1920.0f * k <= 1080.0f && 1080.0f * k <= 2400.0f);   // nenhum cartao sai da tela
  layout_tela_definir(1920, 1080);
  layout_modo_definir(0);
  assert(resolver_escala() >= 1.0f);                          // TV: nao encolhe
  puts("resolver retrato: ok");
  return 0;
}
