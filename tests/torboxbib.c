#include "torboxbib.h"
#include <stdio.h>
int main(void) {
  if (!torboxbib_disponivel()) { puts("torboxbib: skip (sem chave)"); return 0; }
  char b[4096];
  if (!torboxbib_carregar_pagina(0, b, sizeof b)) return 1;
  puts("torboxbib: ok");
  return 0;
}
