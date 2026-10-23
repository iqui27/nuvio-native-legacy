// Ordem dos amigos e corte das sugestoes da aba Amigos (src/amigoslista.h).
#include "../src/amigoslista.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
  unsigned char vivo[5] = { 0, 1, 0, 1, 0 };
  int o[5], i;
  const int esp[5] = { 1, 3, 0, 2, 4 };
  amigoslista_ordem(vivo, 5, o);
  for (i = 0; i < 5; i++) assert(o[i] == esp[i]);
  assert(amigoslista_sug_visiveis(0, 0) == 0);
  assert(amigoslista_sug_visiveis(4, 0) == 4);   // sobra 1: mostra tudo
  assert(amigoslista_sug_visiveis(6, 0) == AMIGOSLISTA_SUG_CAP);
  assert(amigoslista_sug_visiveis(6, 1) == 6);
  puts("amigoslista: ok");
  return 0;
}
