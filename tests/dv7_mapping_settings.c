#include "../src/ajustes.c"
#include <assert.h>
int main(void) {
  assert(AJ_DV7_MAPPING==AJ_MP4_BD_TEST+1 && AJ_N>AJ_DV7_MAPPING);
  assert(valor[AJ_DV7_MAPPING]==1 && valorPadrao[AJ_DV7_MAPPING]==1);
  assert(!ajustes_dv7_mapping());
  assert(!strcmp(CHAVE[AJ_DV7_MAPPING],"dv7PreserveMappingTestLocal"));
  assert(ajustes_escopo(AJ_DV7_MAPPING)==ESC_TV);
  assert(!ajustePortavel(AJ_DV7_MAPPING) && !ajusteAparelho(AJ_DV7_MAPPING));
  valor[AJ_DV7_MAPPING]=0;
#if defined(NV_DV7_MAPPING_TEST) && defined(NV_WEBOS) && defined(NV_DV7_CONVERSION)
  assert(ajustes_dv7_mapping() && !foraDestaPlataforma(AJ_DV7_MAPPING));
  int found=0;for(unsigned i=0;i<sizeof TELA/sizeof *TELA;i++)if(TELA[i].op==AJ_DV7_MAPPING)found++;
  assert(found==1);
#else
  assert(!ajustes_dv7_mapping() && foraDestaPlataforma(AJ_DV7_MAPPING));
#endif
  valor[AJ_DV7_MAPPING]=99;assert(!ajustes_dv7_mapping());
  puts("PASS P7 mapping setting: OFF default, append-only, local scope and platform/build gates");
  return 0;
}
