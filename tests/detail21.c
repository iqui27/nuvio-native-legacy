#include "../src/detail.c"
#include <assert.h>
#include "dados.h"

int main(void) {
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  dados_iniciar("deploy/app/art");
  ajustes_iniciar();
  ajustes_dir(dados_dir());
  assert(ajustes_detalhe_topo() == 0);
  char path[1024];
  snprintf(path, sizeof path, "%s/ajustes.txt", dados_dir());
  for (int mode = 0; mode < 3; mode++) {
    FILE *f = fopen(path, "w"); assert(f);
    fprintf(f, "detalheTopoLocal %d\n", mode); fclose(f);
    ajustes_dir(dados_dir());
    assert(ajustes_detalhe_topo() == mode);
    assert(topoVersao() == (mode != 1));
    assert(topoPessoal() == (mode != 0));
  }
  puts("detail21: top modes OK");
  SDL_Quit();
  return 0;
}
