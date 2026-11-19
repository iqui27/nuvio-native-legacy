#include "barraspretas.h"
#include "ajustes.h"
#include "dados.h"
#include <stdio.h>
#include <stdlib.h>

static int lig = -1;

int barraspretas_ativo(void) {
  if (lig < 0)
    lig = ajustes_barras_pretas() ? 1 : 0;
  return lig;
}

void barraspretas_definir(int on) {
  lig = on ? 1 : 0;
  dados_gravar("barras-pretas.txt", on ? "1" : "0");
}

void barraspretas_invalidar(void) { lig = -1; }
