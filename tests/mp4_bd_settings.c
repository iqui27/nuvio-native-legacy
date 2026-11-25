#define NV_WEBOS 1
#include "../src/ajustes.c"
#include <assert.h>
int main(void) {
  assert(AJ_MP4_BD_TEST==AJ_DV7_MAPPING-1 && AJ_MP4_BD_TEST==AJ_RAM_VERDE+1);
  assert(valor[AJ_MP4_BD_TEST]==1 && valorPadrao[AJ_MP4_BD_TEST]==1);
  assert(!ajustes_mp4_bd_test());
  assert(!strcmp(CHAVE[AJ_MP4_BD_TEST],"mp4BdRemuxTestLocal"));
  assert(ajustes_escopo(AJ_MP4_BD_TEST)==ESC_TV);
  assert(!ajustePortavel(AJ_MP4_BD_TEST) && !ajusteAparelho(AJ_MP4_BD_TEST));
  valor[AJ_MP4_BD_TEST]=0;
#ifdef NV_MP4_BD_TEST
  assert(ajustes_mp4_bd_test() && !foraDestaPlataforma(AJ_MP4_BD_TEST));
#else
  assert(!ajustes_mp4_bd_test() && foraDestaPlataforma(AJ_MP4_BD_TEST));
#endif
  valor[AJ_MP4_BD_TEST]=77;assert(!ajustes_mp4_bd_test());
  puts("PASS MP4 test settings: explicit OFF default, opt-in getter, invalid values, append-only and device-local build gate");
  return 0;
}
