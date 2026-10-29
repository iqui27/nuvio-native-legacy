// Real settings tables/getters; no SDL runtime or services.
#include "../src/ajustes.c"
#include <assert.h>

int main(void) {
  int oldPriority = valor[AJ_FONTE_PRIORIDADE];
  int oldFirst = valor[AJ_FONTE_AUTO], oldGroup = valor[AJ_FONTE_GRUPO];
  assert(OPCOES[AJ_FONTE_PRIORIDADE].n == 4);
  assert(!strcmp(V_FONTE_PRIORIDADE[3], "Do addon"));
  valor[AJ_FONTE_AUTO] = 0;
  valor[AJ_FONTE_GRUPO] = 0; // On; the saved preference must survive mode changes.
  for (int i = 0; i < 3; i++) {
    valor[AJ_FONTE_PRIORIDADE] = i;
    assert(ajustes_fonte_prioridade() == i);
    assert(!ajustes_fonte_primeira() && ajustes_fonte_grupo());
  }
  valor[AJ_FONTE_PRIORIDADE] = 3;
  assert(ajustes_fonte_primeira() && !ajustes_fonte_grupo());
  assert(valor[AJ_FONTE_AUTO] == 0 && valor[AJ_FONTE_GRUPO] == 0);
  valor[AJ_FONTE_PRIORIDADE] = 0;
  assert(!ajustes_fonte_primeira() && ajustes_fonte_grupo());
  valor[AJ_FONTE_AUTO] = 1;
  assert(ajustes_fonte_primeira()); // Existing first-in-list behavior stays available.
  valor[AJ_FONTE_PRIORIDADE] = 99;
  assert(ajustes_fonte_prioridade() == 0);
  valor[AJ_FONTE_PRIORIDADE] = oldPriority;
  valor[AJ_FONTE_AUTO] = oldFirst; valor[AJ_FONTE_GRUPO] = oldGroup;
  puts("add-on order settings: PASS (mode, stored preferences, existing priorities)");
  return 0;
}
