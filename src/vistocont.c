#include <stdlib.h>
#include <string.h>
#include "vistocont.h"

// Id base: corta o ":temp:ep".
static int mesmoImdb(const char *a, const char *b) {
  size_t la, lb;
  if (!a || !b || !a[0] || !b[0]) return 0;
  la = strcspn(a, ":");
  lb = strcspn(b, ":");
  return la == lb && !strncmp(a, b, la);
}

static int mesmoTipo(const char *a, const char *b) {
  return !strcmp(a ? a : "movie", b ? b : "movie");
}

int vistocont_contar(const VcEntrada *e, int n, int *vistos) {
  int i, j, m = 0, v = 0;
  int *grupo, *visto;
  if (vistos) *vistos = 0;
  if (!e || n <= 0) return 0;
  grupo = malloc((size_t)n * sizeof *grupo);
  visto = calloc((size_t)n, sizeof *visto);
  if (!grupo || !visto) { free(grupo); free(visto); return 0; }
  // grupo[i] = indice da primeira entrada do mesmo titulo. O titulo pode ligar
  // por imdb numa entrada e por tmdb noutra, entao a ligacao e transitiva.
  for (i = 0; i < n; i++) {
    grupo[i] = i;
    for (j = 0; j < i; j++) {
      if (mesmoImdb(e[i].imdb, e[j].imdb) ||
          (e[i].tmdb && e[i].tmdb == e[j].tmdb && mesmoTipo(e[i].tipo, e[j].tipo))) {
        int a = i, b = j;
        while (grupo[a] != a) a = grupo[a];
        while (grupo[b] != b) b = grupo[b];
        grupo[a] = b;
      }
    }
  }
  for (i = 0; i < n; i++) {
    j = i;
    while (grupo[j] != j) j = grupo[j];
    if (e[i].visto) visto[j] = 1;
  }
  for (i = 0; i < n; i++)
    if (grupo[i] == i) { m++; v += visto[i]; }
  free(grupo);
  free(visto);
  if (vistos) *vistos = v;
  return m;
}

int vistocont_pct(int vistos, int total) {
  if (total <= 0 || vistos <= 0) return 0;
  if (vistos >= total) return 100;
  return vistos * 100 / total;
}
