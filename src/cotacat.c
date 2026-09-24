// Regra da cota de catalogos por addon. Ver cotacat.h.
#include "cotacat.h"
#include <stdlib.h>
#include <string.h>

static int menor(const CotaPrio *pr, int a, int b) {
  if (pr[a].nivel != pr[b].nivel) return pr[a].nivel < pr[b].nivel;
  if (pr[a].pos != pr[b].pos) return pr[a].pos < pr[b].pos;
  return a < b;
}

int cota_escolher(const CotaPrio *pr, int n, int max, char *escolhido) {
  int *ord, i, j, promovidos = 0;
  if (!pr || !escolhido || n < 1) return 0;
  if (max < 0) max = 0;
  if (n <= max) { memset(escolhido, 1, (size_t)n); return 0; }
  memset(escolhido, 0, (size_t)n);
  ord = (int *)malloc(sizeof(int) * (size_t)n);
  if (!ord) {
    // Sem memoria para ordenar: a regra antiga, os primeiros do manifesto.
    memset(escolhido, 1, (size_t)max);
    return 0;
  }
  // Insercao sobre INDICES: no pior caso (o Xperience, ~600 catalogos) sao
  // ~90 mil comparacoes de inteiros, uma vez por volta da descoberta.
  for (i = 0; i < n; i++) {
    for (j = i - 1; j >= 0 && menor(pr, i, ord[j]); j--) ord[j + 1] = ord[j];
    ord[j + 1] = i;
  }
  for (i = 0; i < max; i++) escolhido[ord[i]] = 1;
  free(ord);
  for (i = max; i < n; i++) if (escolhido[i]) promovidos++;
  return promovidos;
}
