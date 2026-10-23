// MEDIDOR DE DESEMPENHO: a chave antiga (liga/desliga) migra para a forma na
// ilha (desempenho.h). "medidorDesempenhoLocal 0" (V_LIGA: ligado) continua
// visivel como Grande; "... 1" continua desligado; a chave nova vence a antiga;
// sem nada no arquivo, desligado.
#include "../src/ajustes.c"
#include "nvtmp.h"
#include "../src/desempenho.h"
#include <assert.h>
#include <unistd.h>

static int ler(const char *dir, const char *texto) {
  char c[700];
  FILE *f;
  snprintf(c, sizeof c, "%s/ajustes.txt", dir);
  f = fopen(c, "w");
  assert(f);
  fputs(texto, f);
  fclose(f);
  valor[AJ_MEDIDOR] = valorPadrao[AJ_MEDIDOR];
  ajustes_dir(dir);
  return ajustes_medidor_desempenho();
}

int main(void) {
  char base[600], dir[700];
  snprintf(base, sizeof base, "%s/nuvio-medidor-XXXXXX", nvt_dir());
  snprintf(dir, sizeof dir, "%s", base);
  assert(mkdtemp(dir));
  assert(valorPadrao[AJ_MEDIDOR] == DS_DESLIGADO);
  assert(OPCOES[AJ_MEDIDOR].n == 4);
  assert(ler(dir, "medidorDesempenhoLocal 0\n") == DS_GRANDE);
  assert(ler(dir, "medidorDesempenhoLocal 1\n") == DS_DESLIGADO);
  assert(ler(dir, "medidorDesempenhoLocal 0\nmedidorFormaLocal 1\n") == DS_MINIMO);
  assert(ler(dir, "medidorFormaLocal 2\nmedidorDesempenhoLocal 0\n") == DS_MENOR);
  assert(ler(dir, "medidorFormaLocal 9\n") == DS_DESLIGADO);
  assert(ler(dir, "idioma 1\n") == DS_DESLIGADO);
  printf("medidor_migra: ok\n");
  return 0;
}
