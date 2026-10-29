/* Every enum option shows exactly once in TELA, or zero times when it sits in
 * foraDestaPlataforma(). Hidden options stay in the enum (ajustes.txt is
 * positional); this catches a door reshuffle that drops or duplicates one. */
#include "../src/ajustes.c"
#include <stdio.h>

int main(void) {
  int i, vezes[AJ_N] = { 0 }, ruins = 0;
  for (i = 0; i < AJ_N_TELA; i++)
    if (TELA[i].tipo == IT_OPC && TELA[i].op >= 0 && TELA[i].op < AJ_N) vezes[TELA[i].op]++;
  for (i = 0; i < AJ_N; i++) {
    int ok = foraDestaPlataforma(i) ? vezes[i] == 0 : vezes[i] == 1;
    if (!ok) { printf("FAIL: %d %s (\"%s\") in TELA %d time(s)\n", i, CHAVE[i], OPCOES[i].rotulo, vezes[i]); ruins++; }
  }
  if (TELA[0].tipo != IT_SEC) { printf("FAIL: TELA does not start with a category\n"); ruins++; }
  if (ruins) return 1;
  printf("PASS: TELA covers the whole enum (%d options).\n", AJ_N);
  return 0;
}
