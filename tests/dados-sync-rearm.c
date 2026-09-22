#define NV_DADOS_TEST 1
#include "../src/dados.c"

int main(void) {
  if (sujo || sujoLeve || dados_sync_falhas || dados_sync_sucessos) return 1;
  dados_sync_aplicar_resultado(-1, 2);
  if (!sujoLeve || sujo || dados_sync_falhas != 1) return 2;
  dados_sync_aplicar_resultado(-1, 1);
  if (!sujoLeve || !sujo || dados_sync_falhas != 2) return 3;
  dados_sync_aplicar_resultado(1, 1);
  if (dados_sync_sucessos != 1 || dados_sync_falhas != 2) return 4;
  puts("PASS syncfs callback failure rearms the in-flight dirty class; success is counted separately");
  return 0;
}
