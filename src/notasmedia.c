// Ver notasmedia.h.
#include "notasmedia.h"
#include <stddef.h>

int nm_media(const int *notas, int n, int *comNota) {
  int i, k = 0;
  long soma = 0;
  for (i = 0; notas && i < n; i++)
    if (notas[i] > 0) { soma += notas[i]; k++; }
  if (comNota) *comNota = k;
  if (k < NM_MIN_EPS) return 0;
  return (int)((soma * 2 + k) / (2 * k));
}

void nm_extremos(const int *medias, int nt, int *melhor, int *pior) {
  int t, m = -1, p = -1, com = 0;
  for (t = 0; medias && t < nt; t++) {
    if (medias[t] <= 0) continue;
    com++;
    if (m < 0 || medias[t] > medias[m]) m = t;
    if (p < 0 || medias[t] < medias[p]) p = t;
  }
  if (com < 2 || medias[m] == medias[p]) m = p = -1;
  if (melhor) *melhor = m;
  if (pior) *pior = p;
}
