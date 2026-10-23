// #616: explicit portrait geometry and a stationary row-style picker.
#ifdef I616_PICKER
#include "../src/ctxmenu.c"
#include <assert.h>
static GfxRect anchor = { 100, 550, 212, 322 };
static float uiScale = 1;
int home_ancora_foco(GfxRect *r) { *r = anchor; return 1; }
float gfx_escala_ui(void) { return uiScale; }
int ajustes_animacoes_reduzidas(void) { return 0; }

int main(void) {
  int failures = 0;
  for (int scale = 0; scale < 3; scale++) {
    uiScale = 1 + scale * .25f;
    temAncora = 0;
    anchor = (GfxRect){100, 550, 212, 322};
    estAncorar(0);
    float height = 2 * EST_PAD + 15 * (EST_LINHA + EST_GAP) - EST_GAP;
    float available = NV_TELA_H - 2 * CTX_BORDA;
    if (height > available) height = available;
    GfxRect initial = estIlha(height);
    // Every focus move previews a different card size/position underneath.
    for (int i = 0; i < 15; i++) {
      estFoco = i;
      anchor = (GfxRect){100 + i * 60, 500 - i * 12, 360 + i * 40, 203 + i * 15};
      for (int frame = 0; frame < 60; frame++) estAncorar(1.f / 60);
      GfxRect current = estIlha(height);
      float line = (current.h - 2 * EST_PAD + EST_GAP) / 15 - EST_GAP;
      float focusedY = current.y + EST_PAD + i * (line + EST_GAP);
      if (fabsf(current.x - initial.x) > .01f || fabsf(current.y - initial.y) > .01f)
        failures++;
      assert(focusedY >= current.y && focusedY + line <= current.y + current.h);
      assert(current.y >= CTX_BORDA && current.y + current.h <= NV_TELA_H - CTX_BORDA + .01f);
    }
    // Reopening must capture a fresh anchor.
    estAncorar(0);
    assert(fabsf(estAncora.x - anchor.x / uiScale) < .01f);
  }
  printf("%s picker: focus moves preserve list offset; focused option visible\n", failures ? "FAIL" : "PASS");
  return failures != 0;
}
#else
#define ajustes_posteres_deitados i616_landscape
static int i616_landscape(void) { return 1; }
#define main i616_home_fixture_main
#include "home_layout.c"
#undef main

int main(void) {
  int failures = 0;
  assert(ajustes_posteres_deitados());
  for (testeLayout = HOME_LAYOUT_MODERNA; testeLayout < HOME_LAYOUT_N; testeLayout++) {
    TipoFileira portrait = tipoDaEscolha(FIL_TIPO_CARTAZ);
    // Both catalog rows and collection rows use this shared geometry.
    fileiras[0] = (Fileira){ .tipo = portrait, .escala = 1 };
    fileiras[1] = (Fileira){ .tipo = FILEIRA_CATALOGOS, .estiloCol = portrait, .escala = 1 };
    for (int row = 0; row < 2; row++) {
      float w = larguraFil(row), h = alturaFil(row);
      printf("%s layout=%d row=%d portrait %.1fx%.1f\n", w < h ? "PASS" : "FAIL", testeLayout, row, w, h);
      if (w >= h) failures++;
      assert(w == larguraFil(0) && h == alturaFil(0));
    }
    assert(larguraDe(FILEIRA_NORMAL) > alturaDe(FILEIRA_NORMAL)); // Automatic still follows settings.
    assert(larguraDe(tipoDaEscolha(FIL_TIPO_SERVICO)) > alturaDe(tipoDaEscolha(FIL_TIPO_SERVICO)));
  }
  return failures != 0;
}
#endif
