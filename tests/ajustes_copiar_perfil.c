// #679: copiar personalizacao do perfil origem para o destino.
#include "../src/ajustes.c"
#include "nvtmp.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>

int main(void) {
  char dir[NVT_MAX];
  nvt_set(dir, sizeof dir, "/nuvio-copiar-perfil-XXXXXX");
  assert(mkdtemp(dir));
  setenv("NUVIO_DADOS", dir, 1);
  dados_iniciar(dir);
  ajustes_dir(dir);

  valor[AJ_HERO] = 1;
  valor[AJ_MENU_TOPO] = 1;
  valor[AJ_RAIL] = 1;
  ajustes_perfil_guardar(1);

  valor[AJ_HERO] = 0;
  valor[AJ_MENU_TOPO] = 0;
  valor[AJ_RAIL] = 0;
  ajustes_perfil_guardar(2);

  assert(ajustes_copiar_perfil(1, 2) >= 3);
  assert(ajustes_perfil_restaurar(2) == 1);
  assert(valor[AJ_HERO] == 1 && valor[AJ_MENU_TOPO] == 1 && valor[AJ_RAIL] == 1);

  puts("ok ajustes_copiar_perfil");
  return 0;
}
