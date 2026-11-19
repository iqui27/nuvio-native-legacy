// #774: espelha brilho de fontes carregando.
#include <assert.h>
#include <stdio.h>
enum { AJ_FONTE_CARGA_BRILHO = 125 };
static int valor[256];
static int ajustes_fonte_carga_brilho_alto(void) { return valor[AJ_FONTE_CARGA_BRILHO] != 0; }
int main(void) {
  valor[AJ_FONTE_CARGA_BRILHO] = 0;
  assert(!ajustes_fonte_carga_brilho_alto());
  valor[AJ_FONTE_CARGA_BRILHO] = 1;
  assert(ajustes_fonte_carga_brilho_alto());
  puts("d1_fonte_carga_brilho: ok");
  return 0;
}
