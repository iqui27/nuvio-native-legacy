/* Settings home geometry, against the live 2.1 layout code.
 * "Resolver um problema" must keep its full Portuguese title even when it
 * cannot fit the last fixed column; the Panel|List switch measures icons only. */
#include "../src/ajustes.c"
#include <assert.h>

int main(void) {
  GfxRect g, resolver, tour;
  float cw, wMin;
  g = aj2GradeR();
  cw = aj2GradeColW();
  resolver = aj2TopoR(AJ2_T_RESOLVER);
  tour = aj2TopoR(AJ2_T_TOUR);
  wMin = aj2DescobertaLargura(AJ2_T_RESOLVER);

  assert(resolver.w >= wMin - 0.5f);
  assert(resolver.w <= g.w - 2 * AJ2_G_PAD - cw - AJ2_G_VAO + 0.5f);
  assert(tour.w >= cw - 0.5f);
  assert(fabsf((tour.w + AJ2_G_VAO + resolver.w) - (g.w - 2 * AJ2_G_PAD)) < 0.5f);
  assert(aj2LayoutSegW(TXT_V3_CHIP_R, 30.0f) < 200.0f);
  assert(aj2LayoutSegLado(TXT_V3_CHIP_R, 30.0f, 0) == aj2LayoutSegLado(TXT_V3_CHIP_R, 30.0f, 1));

  /* Every blocked row explains why. These requirements were dropped by the
   * 2.1 rewrite and came back from the lost settings branch. */
  char aviso[400];
  assert(uxRequisito(AJ_MANTER_PRAZO) == AJ_MANTER_VIDEO);
  assert(uxRequisito(AJ_DESCANSO_ESTILO) == AJ_ESMAECER);
  assert(uxRequisito(AJ_EP_TAMANHO) == AJ_HOME_LAYOUT);
  valor[AJ_FONTE_PRIORIDADE] = 3;
  assert(uxRequisito(AJ_FONTE_AUTO) == AJ_FONTE_PRIORIDADE);
  assert(uxRequisito(AJ_FONTE_GRUPO) == AJ_FONTE_PRIORIDADE);
  assert(uxRequisito(AJ_LINK_PRAZO) == AJ_FONTE_PRIORIDADE);
  ajAvisoTexto(AJ_DESCOBRIR, aviso, sizeof aviso);
  assert(aviso[0] && (strstr(aviso, "Descobrir") || strstr(aviso, "Discover")));
  puts("settings home discovery cards and icon layout pills: PASS");
  return 0;
}
