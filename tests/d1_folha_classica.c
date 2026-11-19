// #782: espelha folha classica em ajustes.c.
#include <assert.h>
enum { AJ_FOLHA_CLASSICA = 124 };
static int valor[256];
static int ajustes_folha_classica(void) { return valor[AJ_FOLHA_CLASSICA] != 0; }
int main(void) {
  valor[AJ_FOLHA_CLASSICA] = 0;
  assert(!ajustes_folha_classica());
  valor[AJ_FOLHA_CLASSICA] = 1;
  assert(ajustes_folha_classica());
  puts("d1_folha_classica: ok");
  return 0;
}
