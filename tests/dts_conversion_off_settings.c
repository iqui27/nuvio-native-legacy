/* LG DTS off must not change existing modes, buffer selectors or DV settings. */
#define NV_WEBOS 1
#include "../src/ajustes.c"
#include <assert.h>
#include <string.h>
int main(void) {
  assert(N_DTS_SAIDA == 3);
  assert(!strcmp(V_DTS_SAIDA[2], "Desligado (nativo)"));
  assert(valorPadrao[AJ_DTS_AC3] == 0);
  assert(!ajustes_dts_desligado() && !ajustes_dts_ac3());
  valor[AJ_DTS_AC3]=1;
  assert(!ajustes_dts_desligado() && ajustes_dts_ac3());
  int native=ajustes_buffer_native_connections();
  int conversion=ajustes_buffer_conversion_connections();
  int dv7=ajustes_dv7();
  int mp4=ajustes_mp4_bd_test();
  valor[AJ_DTS_AC3]=2;
  assert(ajustes_dts_desligado() && !ajustes_dts_ac3());
  assert(ajustes_buffer_native_connections()==native);
  assert(ajustes_buffer_conversion_connections()==conversion);
  assert(ajustes_dv7()==dv7);
  assert(ajustes_mp4_bd_test()==mp4);
  valor[AJ_DTS_AC3]=0;
  assert(!ajustes_dts_desligado());
  return 0;
}
