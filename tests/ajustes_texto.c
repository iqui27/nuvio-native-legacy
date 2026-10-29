/* M2: a row reads at 3 m. Every option outside "Mais opções" has a short
 * label (at most 32 characters in Portuguese) and a one-line benefit (at most
 * 90) under it when focused; the long text stays in the inspector. */
#include "../src/ajustes.c"
#include <stdio.h>

static int letras(const char *s) {
  int n = 0;
  for (; *s; s++) if (((unsigned char)*s & 0xC0) != 0x80) n++;
  return n;
}

int main(void) {
  int i, ruins = 0, total = 0;
  valor[AJ_IDIOMA] = IDIOMA_PT + 1;   // the limits are for the Portuguese text
  for (i = 0; i < AJ_N_TELA; i++) {
    int op = TELA[i].op, nr, nb;
    if (TELA[i].tipo != IT_OPC || op < 0 || !ajOpcaoDisponivel(op) || uxAvancada(op)) continue;
    total++;
    nr = letras(rotuloOpcao(op)); nb = letras(uxBeneficio(op));
    if (nr > 32) { printf("FAIL: label %d chars: %s \"%s\"\n", nr, CHAVE[op], rotuloOpcao(op)); ruins++; }
    if (nb > 90 || nb == 0) { printf("FAIL: benefit %d chars: %s \"%s\"\n", nb, CHAVE[op], uxBeneficio(op)); ruins++; }
  }
  if (ruins) { printf("%d of %d rows too long\n", ruins, total); return 1; }
  printf("PASS: %d rows with short label and one-line benefit.\n", total);
  return 0;
}
