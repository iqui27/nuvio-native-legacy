// #775: P2P metadata timeout follows Ajustes.
#include "ajustes.h"
#include "p2p.h"
#include <assert.h>
static int valorStub[4096];
#define valor valorStub
#include "../src/ajustes.c"
#undef valor
int main(void) {
  memcpy(valor, valorPadrao, sizeof valor);
  assert(ajustes_p2p_prazo_metadados_s() == 30);
  assert(p2p_prazo_metadados_s() == 30);
  valor[AJ_P2P_PRAZO_META] = 2;
  assert(ajustes_p2p_prazo_metadados_s() == 120);
  valor[AJ_P2P_PRAZO_META] = 3;
  assert(p2p_prazo_metadados_s() == 0);
  puts("d1_p2p_prazo_meta: ok");
  return 0;
}
