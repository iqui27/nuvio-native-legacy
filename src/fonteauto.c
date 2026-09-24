#include "fonteauto.h"
#include <stddef.h>

static int naFila(const int *fila, int n, int i) {
  int j;
  for (j = 0; j < n; j++) if (fila[j] == i) return 1;
  return 0;
}

int fonteauto_fila(int modo, int total, int preferida, const long *pontos,
                   const unsigned char *acimaTeto, const unsigned char *excluida,
                   int max, int *fila) {
  int n = 0;
  if (!fila || max < 1 || total < 1) return 0;

  // A PREFERIDA ENTRA PRIMEIRO nos dois modos: e a fonte que a pessoa escolheu
  // a mao neste titulo (issues #56 e #57). Continua sendo UMA candidata — no
  // modo PRIMEIRA ela e a unica conferida, e nenhuma outra e tocada.
  if (preferida >= 0 && preferida < total && !(excluida && excluida[preferida]))
    fila[n++] = preferida;

  if (modo == FONTEAUTO_PRIMEIRA) {
    // Ordem do addon, com as de dentro do teto antes das de fora. O teto e
    // preferencia, nao filtro (ver cabeNoTeto em streams.c): uma lista so de
    // 4K com teto de 1080p continua tocando.
    int passo, i;
    for (passo = 0; passo < 2 && n < max; passo++)
      for (i = 0; i < total && n < max; i++) {
        int acima = acimaTeto && acimaTeto[i];
        if ((passo == 0) == acima) continue;
        if ((excluida && excluida[i]) || naFila(fila, n, i)) continue;
        fila[n++] = i;
      }
    return n;
  }

  // MELHOR: as de maior pontuacao, em ordem — a mesma ordem que o laco em
  // serie de antes percorria. `>` e nao `>=`: empate fica o primeiro da lista.
  while (n < max) {
    int melhor = -1, i;
    long maior = 0;
    for (i = 0; i < total; i++) {
      long p;
      if ((excluida && excluida[i]) || naFila(fila, n, i)) continue;
      p = pontos ? pontos[i] : 0;
      if (melhor < 0 || p > maior) { melhor = i; maior = p; }
    }
    if (melhor < 0) break;
    fila[n++] = melhor;
  }
  return n;
}

int fonteauto_tentativas(int modo, int pedidas) {
  if (modo == FONTEAUTO_PRIMEIRA) return 1;
  return pedidas < 1 ? 1 : pedidas;
}

int fonteauto_primeira(const int *fila, int n, FonteVerificar verificar,
                       FonteFalhou falhou, void *u, int *tocadas) {
  int q, conta = 0, escolhida = -1;
  if (fila && verificar)
    for (q = 0; q < n; q++) {
      conta++;
      if (verificar(fila[q], u)) { escolhida = fila[q]; break; }
      if (falhou) falhou(fila[q], u);
    }
  if (tocadas) *tocadas = conta;
  return escolhida;
}
