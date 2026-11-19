// #775: espelha prazo de metadados P2P (ajustes.c + p2p.c).
#include <assert.h>
#include <stdio.h>
enum { AJ_P2P_PRAZO_META = 123 };
static int valor[256];
static int ajustes_p2p_prazo_metadados_s(void) {
  static const unsigned prazos[] = {30, 60, 120, 0};
  int v = valor[AJ_P2P_PRAZO_META];
  return (v >= 0 && v < 4) ? (int)prazos[v] : 30;
}
static int p2p_prazo_metadados_s(void) {
  int s = ajustes_p2p_prazo_metadados_s();
  return s > 0 ? s : 0;
}
int main(void) {
  valor[AJ_P2P_PRAZO_META] = 0;
  assert(ajustes_p2p_prazo_metadados_s() == 30);
  assert(p2p_prazo_metadados_s() == 30);
  valor[AJ_P2P_PRAZO_META] = 2;
  assert(ajustes_p2p_prazo_metadados_s() == 120);
  valor[AJ_P2P_PRAZO_META] = 3;
  assert(p2p_prazo_metadados_s() == 0);
  puts("d1_p2p_prazo_meta: ok");
  return 0;
}
