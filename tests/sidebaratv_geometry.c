// Apple TV sidebar (layout Dinamica) geometry must match the reference photo
// (fotos1014/appletv-sidebar.jpg, measured from the official app print):
// panel margin/width, the circular icon chip, and the focused pill that grows.
#include "../src/menu.c"
#include <assert.h>
#include <stdio.h>

static int layoutTeste = HOME_LAYOUT_DINAMICA;
float gfx_tex_aspect_atual;
float gfx_card_forcar_cover_atual;
int ajustes_home_layout(void) { return layoutTeste; }
int ajustes_menu_explorar(void) { return 1; }
int ajustes_menu_guia(void) { return 1; }
int ajustes_menu_agenda(void) { return 1; }
int ajustes_menu_perfil(void) { return 1; }
int ajustes_rail_recolhida(void) { return 0; }
int ajustes_menu_topo(void) { return 0; }
float gfx_escala_ui(void) { return 1.0f; }
float ilha_relogio_largura(void) { return 100.0f; }
int home_streaming_barra(const int **pastas) { (void)pastas; return 0; }

int main(void) {
  GfxRect p, base = { 100.0f, 100.0f, 300.0f, TV_PILULA_H }, foco;

  // Panel: margin from the screen edge and width as measured (x 38, W 355).
  p = tvPainel();
  assert(fabsf(p.x - 38.0f) < 0.01f);
  assert(fabsf(p.y - 38.0f) < 0.01f);
  assert(fabsf(p.w - 355.0f) < 0.01f);
  assert(p.w < NV_TELA_W && p.h <= TV_ALTURA_MAX + 0.01f);

  // Focused pill grows TV_FOCO_CRESC per side (clamped past the overshoot).
  foco = tvPillFoco(base, 1.0f);
  assert(fabsf((foco.x - base.x) + TV_FOCO_CRESC) < 0.01f);
  assert(fabsf((foco.y - base.y) + TV_FOCO_CRESC) < 0.01f);
  assert(fabsf((foco.w - base.w) - 2.0f * TV_FOCO_CRESC) < 0.01f);
  assert(fabsf((foco.h - base.h) - 2.0f * TV_FOCO_CRESC) < 0.01f);
  foco = tvPillFoco(base, 5.0f);   // spring overshoot: no more than 1.15x
  assert(fabsf((foco.w - base.w) - 2.0f * TV_FOCO_CRESC * 1.15f) < 0.01f);
  assert(tvPillFoco(base, 0.0f).w == base.w);

  // Icon chip: a circle behind the icon, fading out on focus.
  assert(TV_CHIP > TV_ICONE + 14.0f);
  assert(TV_CHIP_REPOUSO > TV_CHIP_FOCO);
  assert(anim_mistura(TV_CHIP_REPOUSO, TV_CHIP_FOCO, 0.0f) == TV_CHIP_REPOUSO);
  assert(anim_mistura(TV_CHIP_REPOUSO, TV_CHIP_FOCO, 1.0f) == TV_CHIP_FOCO);

  // Focus pill sits inside the row even grown (no overlap beyond a hint).
  assert(TV_PILULA_H + 2.0f * TV_FOCO_CRESC <= TV_LINHA_H + 2.0f);

  puts("PASS: Apple TV sidebar panel, chip and focus pill match the reference");
  return 0;
}
