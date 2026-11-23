// Approved picker geometry; existing TV/phone geometry must remain unchanged.
#include "../src/layout.c"
#include "../src/perfilsel.c"
#include <assert.h>
static int avatarSize = 1;
int ajustes_ps_tamanho(void) { return avatarSize; }
int main(void) {
  layout_janela_definir(1440, 900);
  layout_tela_definir(2880, 1800);
  assert(layout_desktop_ativo());
  assert(fabsf(diametro(5) - 140 * NV_TELA_W / 1440) < .01f);
  assert(fabsf(vaoDe(5, diametro(5)) - 76 * NV_TELA_W / 1440) < .01f);
  assert(fabsf(filaY() - 352 * NV_TELA_H / 900) < .01f);
  // All eight real profiles fit, even with the largest preference.
  avatarSize = 2;
  assert(8 * diametro(8) + 7 * vaoDe(8, diametro(8)) <= NV_TELA_W - 96);
  avatarSize = 1;
  layout_janela_definir(800, 1000);
  assert(!layout_desktop_ativo());
  assert(filaY() == 396);
  assert(diametro(5) > 200);
  puts("PASS: desktop profile board geometry, eight profiles, narrow layout unchanged");
}
