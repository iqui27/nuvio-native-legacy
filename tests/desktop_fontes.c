// Exercise the production source-sheet geometry without GL, network or account data.
#include "../src/streams.c"
#include <assert.h>
float gfx_escala_ui(void) { return 1; }
float gfx_escala(void) { return 1; }
int player_aberto(void) { return 0; }
int main(void) {
  layout_janela_definir(1440,900);
  layout_tela_definir(2880,1800);
  assert(fabsf(FOLHA_W-1056)<.01f);
  assert(fabsf(folhaEscala()-1)<.01f);
  // Sources must occupy the board's central 792x728pt sheet, not the TV rail.
  GfxRect panel=folhaDesktopRect();
  assert(fabsf(panel.x-608)<.01f && fabsf(panel.y-126)<.01f);
  assert(panel.w==1056 && panel.h==972);
  assert(FOLHA_LINHA_GAP==18 && FOLHA_SEC_H==48);
  layout_janela_definir(1024,768);
  panel=folhaDesktopRect();
  assert(panel.x>=40 && panel.x+panel.w<=NV_TELA_W-40);
  assert(panel.y>=48 && panel.y+panel.h<=NV_TELA_H-48);
  layout_janela_definir(700,900);
  assert(!folhaDesktop());
  puts("PASS: approved desktop Sources width, scale and centered geometry");
  return 0;
}
