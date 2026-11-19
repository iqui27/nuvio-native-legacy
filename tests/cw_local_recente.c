// #731: dois episodios da mesma obra — fica o lastWatchedMs mais novo.
#include "idbase.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  char contentId[24];
  long long lastWatchedMs;
  int temporada, episodio;
  double pos, dur;
} Reg;

typedef struct {
  char imdb[64];
  long long retomadoMs;
  int temporada, episodio;
} Out;

static int inserir(Out *saida, int *pn, const Reg *r) {
  int j, repetido = 0;
  for (j = 0; j < *pn; j++) {
    size_t L = idbase_len(saida[j].imdb);
    if (L != strlen(r->contentId) || strncmp(saida[j].imdb, r->contentId, L)) continue;
    if (r->lastWatchedMs <= saida[j].retomadoMs) { repetido = 1; break; }
    (*pn)--;
    if (j < *pn) memmove(&saida[j], &saida[j + 1], (size_t)(*pn - j) * sizeof *saida);
    break;
  }
  if (repetido) return 0;
  Out *d = &saida[*pn];
  snprintf(d->imdb, sizeof d->imdb, "%s:%d:%d", r->contentId, r->temporada, r->episodio);
  d->retomadoMs = r->lastWatchedMs;
  d->temporada = r->temporada;
  d->episodio = r->episodio;
  (*pn)++;
  return 1;
}

int main(void) {
  Out saida[4];
  int n = 0;
  Reg velho = { "tt16341009", 1000, 1, 1 };
  Reg novo  = { "tt16341009", 5000, 1, 5 };

  memset(saida, 0, sizeof saida);
  assert(inserir(saida, &n, &velho));
  assert(n == 1 && saida[0].episodio == 1);
  assert(inserir(saida, &n, &novo));
  assert(n == 1 && saida[0].episodio == 5 && saida[0].retomadoMs == 5000);
  assert(!inserir(saida, &n, &velho));
  assert(n == 1 && saida[0].episodio == 5);
  puts("PASS #731: continuar local dedup fica no episodio mais recente");
  return 0;
}
