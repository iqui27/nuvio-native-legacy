// Corte cabeca+cauda do envio automatico (#203): extrai cortarCabecaCauda de
// src/avisos.c e confere teto, marcador, cabeca e cauda.
//   bash tests/avisos_corte.sh
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/avisos_corte.inc"

int main(void) {
  size_t n = 0, cap = 300 * 1024, m;
  char *t = malloc(cap + 1);
  int i;
  for (i = 0; n < 200 * 1024; i++) n += (size_t)sprintf(t + n, "L%05d %s\n", i, "yyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyyy");
  char fim[64];
  sprintf(fim, "L%05d ", i - 1);
  m = cortarCabecaCauda(t, n, 64 * 1024);
  assert(m <= 64 * 1024 && m == strlen(t));
  assert(strncmp(t, "L00000 ", 7) == 0);
  assert(strstr(t, " bytes omitidos ...\n") != NULL);
  assert(strstr(t, fim) != NULL);
  assert(t[m - 1] == '\n');
  { char curto[] = "abc"; assert(cortarCabecaCauda(curto, 3, 64 * 1024) == 3); }
  free(t);
  puts("avisos_corte: ok");
  return 0;
}
