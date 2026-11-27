// Match frame rate setting: real settings tables and getter, no player or device.
#include "../src/ajustes.c"
#include <assert.h>

int main(void) {
  assert(valor[AJ_QUADROS_TAXA] == 0 && valorPadrao[AJ_QUADROS_TAXA] == 0);
  assert(!strcmp(CHAVE[AJ_QUADROS_TAXA], "frameRateMatchLocal"));
  assert(somenteDesteAparelho(AJ_QUADROS_TAXA));
  assert(OPCOES[AJ_QUADROS_TAXA].n == 3);
  assert(!strcmp(OPCOES[AJ_QUADROS_TAXA].valores[0], "Sempre"));
  assert(!strcmp(OPCOES[AJ_QUADROS_TAXA].valores[2], "Desligado"));
#ifdef NV_ANDROID
  assert(ajustes_quadros_taxa() == 0);
  valor[AJ_QUADROS_TAXA] = 1; assert(ajustes_quadros_taxa() == 1);
  valor[AJ_QUADROS_TAXA] = 2; assert(ajustes_quadros_taxa() == 2);
  valor[AJ_QUADROS_TAXA] = 7; assert(ajustes_quadros_taxa() == 0);
  assert(!foraDestaPlataforma(AJ_QUADROS_TAXA));
  int row = -1, tunnel = -1;
  for (int i = 0; i < AJ_N_TELA; i++) {
    if (TELA[i].op == AJ_QUADROS_TAXA) row = i;
    if (TELA[i].op == AJ_TUNNELING) tunnel = i;
  }
  assert(row == tunnel + 1);
#else
  assert(ajustes_quadros_taxa() == 2);
  assert(foraDestaPlataforma(AJ_QUADROS_TAXA));
#endif
  valor[AJ_QUADROS_TAXA] = 0;
  return 0;
}
