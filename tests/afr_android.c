// Exercise the real settings tables and getter for #641, without a player or
// device: the AFR option is Android-only, device-local, off by default.
#include "../src/ajustes.c"
#include <assert.h>

int main(void) {
  assert(valor[AJ_AFR] == 0 && valorPadrao[AJ_AFR] == 0);
  assert(!ajustes_afr());
  assert(!strcmp(CHAVE[AJ_AFR], "afrLocal"));
  assert(somenteDesteAparelho(AJ_AFR));
  assert(OPCOES[AJ_AFR].n == 2);
  assert(!strcmp(OPCOES[AJ_AFR].valores[0], "Desligado"));
  assert(!strcmp(OPCOES[AJ_AFR].valores[1], "Automático"));
  valor[AJ_AFR] = 1;
#ifdef NV_ANDROID
  assert(ajustes_afr());
  assert(!foraDestaPlataforma(AJ_AFR));
  int row = -1, decoder = -1, tunnel = -1;
  for (int i = 0; i < AJ_N_TELA; i++) {
    if (TELA[i].op == AJ_AFR) row = i;
    if (TELA[i].op == AJ_DTS_AC3) decoder = i;
    if (TELA[i].op == AJ_TUNNELING) tunnel = i;
  }
  assert(row == decoder + 1 && tunnel == row + 1);
#else
  assert(!ajustes_afr());
  assert(foraDestaPlataforma(AJ_AFR));
#endif
  valor[AJ_AFR] = 0;
  assert(!ajustes_afr());
}
