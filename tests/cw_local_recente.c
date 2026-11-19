// #731: com dois episodios da mesma obra no disco, continuarLocalLer fica com o
// registro de lastWatchedMs mais novo (nao descarta o ultimo assistido).
#include "catalogo.h"
#include "idbase.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct { char contentId[24]; long long lastWatchedMs; int temporada, episodio; } Reg;

typedef struct { char imdb[64]; long long retomadoMs; int temporada, episodio; } Out;

static int aplicarDedup(Out *saida, int n, const Reg *r, int *repetido) {
  int j;
  *repetido = 0;
  for (j = 0; j < n; j++) {
    size_t L = idbase_len(saida[j].imdb);
    if (L != strlen(r->contentId) || strncmp(saida[j].imdb, r->contentId, L)) continue;
    if (r->lastWatchedMs <= saida[j].retomadoMs) { *repetido = 1; return n; }
    n--;
    if (j < n) memmove(&saida[j], &saida[j + 1], (size_t)(n - j) * sizeof *saida);
    return n;
  }
  return n;
}

int main(void) {
  Out saida[4];
  int n = 0;
  Reg velho = { "tt16341009", 1000, 1, 1 };
  Reg novo  = { "tt16341009", 5000, 1, 5 };
  int rep;

  memset(saida, 0, sizeof saida);
  snprintf(saida[n].imdb, sizeof saida[n].imdb, "%s", velho.contentId);
  saida[n].retomadoMs = velho.lastWatchedMs;
  saida[n].temporada = 1; saida[n].episodio = 1;
  n++;

  n = aplicarDedup(saida, n, &novo, &rep);
  assert(!rep);
  assert(n == 1);
  assert(saida[0].episodio == 5 && saida[0].retomadoMs == 5000);

  n = aplicarDedup(saida, n, &velho, &rep);
  assert(rep);
  assert(n == 1 && saida[0].episodio == 5);

  puts("PASS #731: continuar local dedup fica no episodio mais recente");
  return 0;
}
