// Exercise the selected Settings row with real fonts, controls and drawing.
#include <assert.h>
#include "text.h"
#include <stdio.h>
#include <string.h>
static int clipped;
static TxtLinha checked_line(TxtEstilo e, const char *s, int r, int g, int b, int a, float w) {
  if (!strcmp(s, "Layout da home") && txt_largura(e, s) > w + 1) clipped++;
  return txt_linha_corta(e, s, r, g, b, a, w);
}
#define txt_linha_corta checked_line
#include "../src/ajustes.c"
#undef txt_linha_corta

extern void rev3_record(int enabled);
extern int rev3_overlaps(void);
int test_settings_rev3(void) {
  valor[AJ_TAMANHO_AJUSTES] = 2;
  valor[AJ_LAYOUT_AJUSTES] = 0;
  valor[AJ_IDIOMA] = IDIOMA_PT + 1;
  focoIndice = 0;
  int item = -1;
  for (int i = 0; i < AJ_N_TELA; i++) if (TELA[i].tipo == IT_OPC && TELA[i].op == AJ_HOME_LAYOUT) item = i;
  assert(item >= 0);
  focoItem = item; focoOp = AJ_HOME_LAYOUT;
  float old = gfx_escala(); gfx_escala_sair(ajustes_tamanho_ajustes());
  rev3_record(1);
  ajDesenharLinha(item, 200, 1, 1);
  rev3_record(0);
  int overlap = rev3_overlaps();
  printf("%s Settings 130%%: clipped=%d overlapping text=%d\n", clipped || overlap ? "FAIL" : "PASS", clipped, overlap);
  gfx_escala_sair(old);
  return clipped || overlap;
}
