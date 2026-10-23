// Menu coordinates and content reservation must agree in physical pixels.
#include "../src/menu.c"
#include <assert.h>
#include <stdio.h>

static int layoutTeste = HOME_LAYOUT_MODERNA;
float gfx_tex_aspect_atual;
float gfx_card_forcar_cover_atual;
int ajustes_home_layout(void) { return layoutTeste; }
int ajustes_menu_explorar(void) { return 1; }
int ajustes_menu_guia(void) { return 1; }
int ajustes_menu_agenda(void) { return 1; }
int ajustes_menu_perfil(void) { return 1; }
int ajustes_rail_recolhida(void) { return 0; }
int ajustes_menu_topo(void) { return 0; }
static float escalaTeste = 1.0f;
float gfx_escala_ui(void) { return escalaTeste; }
static float relogioW;
float ilha_relogio_largura(void) { return relogioW; }

static void topoRelogio(void) {
  // Rotulos/perfil longos preenchem o teto da barra centrada. O relogio
  // de 12 h (texto + padding), inclusive ampliado, precisa manter 16 px.
  for (int l = 0; l < 2; l++) {
    layoutTeste = l ? HOME_LAYOUT_PADRAO : HOME_LAYOUT_MODERNA;
    for (int s = 0; s < 2; s++) {
      escalaTeste = s ? 1.5f : 1.0f;
      for (int w = 140; w <= 300; w += 40) {
        relogioW = (float)w * escalaTeste;
        float direita = NV_TELA_W * menuEscala() - TOPO_RELOGIO * menuEscala();
        float esquerdaIlha = NV_TELA_BASE_W - NV_ILHA_MARGEM_D * escalaTeste - relogioW;
        assert(direita + 16.0f <= esquerdaIlha + 0.01f);
      }
    }
  }
  layoutTeste = HOME_LAYOUT_MODERNA;
  escalaTeste = 1.0f;
}

static void moderna(float e) {
  MenuGeo g = geoEm(e, 1.0f);
  float s = menuEscala();
  assert(fabsf(g.x * s - 48.0f) < 0.01f);
  assert(fabsf((g.y + g.h * 0.5f) * s - 540.0f) < 0.01f);
  assert(g.y > 0 && (g.y + g.h) * s < 1080.0f);
}
int main(void) {
  MenuGeo g;
  topoRelogio();
  moderna(0); moderna(0.5f); moderna(1); moderna(1.05f);
  g = geoEm(0, 1);
  assert(fabsf(g.w * menuEscala() - 105.6f) < 0.01f);
  assert(fabsf(menu_barra_borda() - NV_MENU_RAIL_BORDA_MODERNA) < 0.01f);
  g = geoEm(1, 1);
  assert(fabsf(g.w * menuEscala() - 384.0f) < 0.01f);
  layoutTeste = HOME_LAYOUT_PADRAO;
  g = geoEm(0, 1);
  assert(g.x == 0 && fabsf(g.w * menuEscala() - 88.0f) < 0.01f);
  assert(fabsf(menu_barra_borda() - NV_MENU_RAIL_BORDA_PADRAO) < 0.01f);
  layoutTeste = HOME_LAYOUT_DINAMICA;
  assert(fabsf(menuEscala() - 0.9f) < 0.001f);
  assert(menu_barra_borda() == 0);
  puts("PASS: Modern menu centered/enlarged, reserved content and other layouts preserved");
  return 0;
}
