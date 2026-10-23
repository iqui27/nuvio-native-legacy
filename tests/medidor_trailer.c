// MEDIDOR DE DESEMPENHO SOME COM TRAILER TOCANDO (#338, 2.0.6): enquanto o
// trailer do destaque da Home ou da pagina do titulo toca, desempenho_forma()
// responde DS_DESLIGADO e a ilha nao desenha o medidor. Quando o trailer para,
// a forma escolhida em Ajustes volta.
#include "../src/ajustes.c"
#include "../src/desempenho.h"
#include <assert.h>

int main(void) {
  desempenho_amostra(60.0f, 4.0f, 0, 0, 0.0f, 0, 0, 0, 120.0f);
  valor[AJ_MEDIDOR] = DS_GRANDE;
  assert(desempenho_forma() == DS_GRANDE);
  desempenho_trailer(1);
  assert(desempenho_forma() == DS_DESLIGADO);
  assert(desempenho_linha_w(desempenho_forma()) == 0.0f);
  desempenho_trailer(0);
  assert(desempenho_forma() == DS_GRANDE);
  printf("medidor_trailer: ok\n");
  return 0;
}
