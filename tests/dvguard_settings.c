#include "../src/ajustes.c"
#include <assert.h>

void dvguard_toggle(int on) {
  assert(definirValorDireto(AJ_DV_MKV, on ? 0 : 1));
  assert(ajustes_dv_mkv() == on);
}
