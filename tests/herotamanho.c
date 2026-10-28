// Exercise the actual persisted setting and its scale mapping without GL.
#include "../src/ajustes.c"
#include <assert.h>

int main(void) {
  assert(!strcmp(CHAVE[AJ_HERO_TAMANHO], "heroSizeLocal"));
  assert(somenteDesteAparelho(AJ_HERO_TAMANHO));
  assert(OPCOES[AJ_HERO_TAMANHO].n == 3);
  assert(valorPadrao[AJ_HERO_TAMANHO] == 0);
  const float logo[] = {1.0f, 1.25f, 1.5f};
  const float info[] = {1.0f, 1.15f, 1.3f};
  for (int size = 0; size < 3; size++) {
    valor[AJ_HERO_TAMANHO] = size;
    assert(ajustes_hero_tamanho() == size);
    assert(ajustes_hero_escala_logo() == logo[size]);
    assert(ajustes_hero_escala_info() == info[size]);
    assert(ajustes_hero_sinopse_grande() == (size > 0));
  }
  valor[AJ_HERO_TAMANHO] = -1;
  assert(ajustes_hero_tamanho() == 0);
  valor[AJ_HERO_TAMANHO] = 9;
  assert(ajustes_hero_tamanho() == 0);
  puts("ok: hero size Normal/Large/Extra large maps to logo and info scales; invalid values use Normal");
  return 0;
}
