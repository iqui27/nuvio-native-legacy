#include "barraspretas.h"
#include "dados.h"
#include <stdio.h>
#include <stdlib.h>

static int lig = -1;

int barraspretas_ativo(void) {
  if (lig < 0) {
    char *t = dados_ler("barras-pretas.txt");
    lig = (t && t[0] == '1') ? 1 : 0;
    free(t);
  }
  return lig;
}

void barraspretas_definir(int on) {
  lig = on ? 1 : 0;
  dados_gravar("barras-pretas.txt", on ? "1" : "0");
}
