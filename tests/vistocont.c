// Contagem "voce viu N de M" da filmografia: pura, sem rede nem catalogo.
#include <stdio.h>
#include <stdlib.h>
#include "vistocont.h"

static int falhas;
#define CHECA(c) do { if (!(c)) { printf("FALHOU %s:%d %s\n", __FILE__, __LINE__, #c); falhas++; } } while (0)

int main(void) {
  int n;
  // Vazio.
  CHECA(vistocont_contar(NULL, 0, &n) == 0 && n == 0);

  // Simples: 3 titulos, 1 visto.
  VcEntrada a[] = { { "tt1", 0, "movie", 1 }, { "tt2", 0, "movie", 0 }, { "tt3", 0, "movie", 0 } };
  CHECA(vistocont_contar(a, 3, &n) == 3 && n == 1);

  // Mesmo titulo por tt e por tmdb (fonte sem imdb): conta uma vez, visto se
  // qualquer das duas formas diz visto.
  VcEntrada b[] = { { "tt1", 100, "movie", 0 }, { "", 100, "movie", 1 }, { "tt2", 200, "movie", 1 } };
  CHECA(vistocont_contar(b, 3, &n) == 2 && n == 2);

  // Mesmo tmdb, tipos diferentes (filme x serie): sao titulos distintos.
  VcEntrada c[] = { { "", 7, "movie", 1 }, { "", 7, "series", 0 } };
  CHECA(vistocont_contar(c, 2, &n) == 2 && n == 1);

  // Id de episodio ("tt9:1:2") e o mesmo titulo que "tt9".
  VcEntrada d[] = { { "tt9:1:2", 0, "series", 0 }, { "tt9", 0, "series", 1 } };
  CHECA(vistocont_contar(d, 2, &n) == 1 && n == 1);

  // Sem nenhuma identidade: cada entrada e propria.
  VcEntrada e[] = { { "", 0, "movie", 1 }, { "", 0, "movie", 0 } };
  CHECA(vistocont_contar(e, 2, &n) == 2 && n == 1);

  // Identidades transitivas, incluindo ponte que chega depois dos grupos.
  VcEntrada f[] = { { "tt1", 0, "movie", 0 }, { "tt1", 100, "movie", 0 }, { "", 100, "movie", 1 } };
  CHECA(vistocont_contar(f, 3, &n) == 1 && n == 1);
  VcEntrada g[] = { { "tt1", 0, "movie", 1 }, { "", 100, "movie", 1 }, { "tt1", 100, "movie", 0 } };
  CHECA(vistocont_contar(g, 3, &n) == 1 && n == 1);
  VcEntrada h[] = { { "tt1", 0, "movie", 0 }, { "", 100, "movie", 0 }, { "tt1", 100, "movie", 0 } };
  CHECA(vistocont_contar(h, 3, &n) == 1 && n == 0);

  // Percentual para o anel.
  CHECA(vistocont_pct(1, 3) == 33 && vistocont_pct(0, 0) == 0 && vistocont_pct(5, 5) == 100);

  if (falhas) { printf("vistocont: %d falha(s)\n", falhas); return 1; }
  printf("vistocont: tudo ok\n");
  return 0;
}
