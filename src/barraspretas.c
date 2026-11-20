#include "barraspretas.h"
#include "ajustes.h"
#include "dados.h"
#include <stdio.h>
#include <stdlib.h>

int barraspretas_ativo(void) {
  /* Sem cache: a primeira janela pode consultar antes de ajustes_dir() ler
     o ajustes.txt, e o valor salvo so chega a valor[] depois. O getter le
     o vetor direto — custo de um acesso a inteiro. */
  return ajustes_barras_pretas() ? 1 : 0;
}

void barraspretas_definir(int on) {
  dados_gravar("barras-pretas.txt", on ? "1" : "0");
}

void barraspretas_invalidar(void) { }
