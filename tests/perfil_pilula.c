// Profile & Stats under the Dinamica layout's bar pill (src/perfil.c), no GL.
// M sweep, 04/10/2026: on the owner's Android TV the "‹ Profile & Stats" pill
// (y 44..104) sat on top of the avatar and the name ("Henrique Rocha" clipped
// at the top). With the pill on screen the whole page has to start 20 px below
// it; without it nothing moves.
#include "../src/perfil.c"
#include "dados.h"
#include <assert.h>

static void layout(int n) {
  char cam[700];
  FILE *f;
  snprintf(cam, sizeof cam, "%s/ajustes.txt", dados_dir());
  f = fopen(cam, "w");
  assert(f);
  fprintf(f, "homeLayoutLocal %d\n", n);
  fclose(f);
  ajustes_dir(dados_dir());
}

int main(void) {
  float px, py, pw, ph;
  dados_iniciar("deploy/app/art");

  layout(2);   // Dinamica: the bar is a pill at the top left
  assert(menu_pilula_rect(&px, &py, &pw, &ph));
  assert(pfDy() > 0.0f);
  assert(PF_TOPO >= py + ph + 20.0f - 0.5f);   // avatar/name start below the pill
  assert(rNumero(0).y == PF_NUM_Y);
  assert(rCartao(0).y == PF_CARD_Y);
  // Header, numbers and cards keep the mockup's spacing: only the whole block moved.
  assert(fabsf(PF_NUM_Y - PF_TOPO - (212.0f - 56.0f)) < .01f);
  assert(fabsf(PF_CARD_Y - PF_NUM_Y - (392.0f - 212.0f)) < .01f);
  // And the block still ends above the footer notice.
  assert(rCartao(0).y + rCartao(0).h == PF_CONTEUDO_H);

  layout(1);   // any other layout: the top left is free
  assert(!menu_pilula_rect(&px, &py, &pw, &ph));
  assert(pfDy() == 0.0f);
  assert(PF_TOPO == 56.0f && PF_NUM_Y == 212.0f && PF_CARD_Y == 392.0f);

  puts("PASS: perfil_pilula");
  return 0;
}
