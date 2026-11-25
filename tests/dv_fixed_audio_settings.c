/* Real settings table/getter: the experimental choice must never silently
 * enable on upgrade, move existing preferences or propagate via the account. */
#include "ajustes.h"
#define NV_WEBOS 1
#include "../src/ajustes.c"
#include <assert.h>
int main(void) {
  assert(AJ_DV_AUDIO_FIXO==AJ_MP4_BD_TEST-4 && valor[AJ_DV_AUDIO_FIXO]==0);
  assert(!strcmp(CHAVE[AJ_DV_AUDIO_FIXO],"dvFixedAudioTestLocal"));
  assert(ajustes_escopo(AJ_DV_AUDIO_FIXO)==ESC_TV);
  assert(!ajustePortavel(AJ_DV_AUDIO_FIXO) && !ajusteAparelho(AJ_DV_AUDIO_FIXO));
  assert(!ajustes_dv_audio_fixo());
  for(int mode=1;mode<=2;mode++) {
    valor[AJ_DV_AUDIO_FIXO]=mode;
#ifdef NV_DV_FIXED_AUDIO_TEST
    assert(ajustes_dv_audio_fixo()==mode && !foraDestaPlataforma(AJ_DV_AUDIO_FIXO));
#else
    assert(!ajustes_dv_audio_fixo() && foraDestaPlataforma(AJ_DV_AUDIO_FIXO));
#endif
  }
  valor[AJ_DV_AUDIO_FIXO]=77;assert(!ajustes_dv_audio_fixo());
  assert(!strcmp(OPCOES[AJ_DV_AUDIO_FIXO].rotulo,"Dolby Vision audio output"));
  puts("Fixed DV audio settings: opt-in, invalid value, append-only ID, device-only scope and build gate passed");
  return 0;
}
