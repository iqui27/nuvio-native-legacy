#include "saidaandroid.h"
#include <stdlib.h>
#include <string.h>

int saida_android_nao_foi_queda(const char *saida) {
  const char *p, *par;
  char *fim;
  long cod;
  if (!saida || !(p = strstr(saida, "motivo="))) return 0;
  par = strchr(p, '(');
  if (!par || (size_t)(par - p) > 40) return 0;
  cod = strtol(par + 1, &fim, 10);
  if (fim == par + 1 || *fim != ')') return 0;
  // ApplicationExitInfo.REASON_*: PERMISSION_CHANGE 8, USER_REQUESTED 10,
  // USER_STOPPED 11, OTHER 13 (no D1: SwipeUpClean, REQUEST_INSTALL_PACKAGES
  // changed), PACKAGE_STATE_CHANGE 15, PACKAGE_UPDATED 16.
  switch (cod) {
    case 8: case 10: case 11: case 13: case 15: case 16: return 1;
    default: return 0;
  }
}
