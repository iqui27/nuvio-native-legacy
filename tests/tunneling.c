// Exercise the real settings tables and getter, without a player or device.
#include "../src/ajustes.c"
#include <assert.h>

int main(void) {
  assert(valor[AJ_TUNNELING] == 0 && valorPadrao[AJ_TUNNELING] == 0);
  assert(!ajustes_tunneling());
  assert(!strcmp(CHAVE[AJ_TUNNELING], "tunneledPlaybackLocal"));
  assert(somenteDesteAparelho(AJ_TUNNELING));
  assert(OPCOES[AJ_TUNNELING].n == 2);
  assert(!strcmp(OPCOES[AJ_TUNNELING].valores[0], "Desligado"));
  valor[AJ_TUNNELING] = 1;
#ifdef NV_ANDROID
  assert(ajustes_tunneling());
  assert(!foraDestaPlataforma(AJ_TUNNELING));
  int row = -1, decoder = -1;
  for (int i = 0; i < AJ_N_TELA; i++) {
    if (TELA[i].op == AJ_TUNNELING) row = i;
    if (TELA[i].op == AJ_DTS_AC3) decoder = i;
  }
  assert(row == decoder + 1);
#else
  assert(!ajustes_tunneling());
  assert(foraDestaPlataforma(AJ_TUNNELING));
#endif
  valor[AJ_TUNNELING] = 0;
  assert(!ajustes_tunneling());
}
