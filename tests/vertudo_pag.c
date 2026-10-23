// Filmografia (Linha do tempo) de um diretor: o catalogo vem em paginas de 15 e
// a pagina 2 traz Reservoir Dogs (1992). Com o foco no primeiro cartao a tela
// nunca pedia a pagina 2, entao a linha do tempo comecava em 1994.
#include <assert.h>
#include <stdio.h>
#include "../src/vertudo_pag.h"

#define TOTAL 20
#define PAGINA 15
static const int anos[TOTAL] = {1994,1995,1997,2003,2004,2007,2007,2009,2012,2015,
                                2019,2004,2005,2007,2007,1992,1992,1993,1996,2001};

int main(void) {
  int n = PAGINA, fim = 0, foco = 0;
  for (int quadro = 0; quadro < 50; quadro++) {
    if (vertudo_quer_mais(1, n, foco, 600, 0, fim, 0)) {
      n = TOTAL;     // a pagina 2 chega
      fim = 1;       // e o catalogo acaba
    }
  }
  int min = 9999;
  for (int i = 0; i < n; i++) if (anos[i] < min) min = anos[i];
  printf("titulos=%d primeiro_ano=%d\n", n, min);
  assert(min == 1992);
  // grade comum (nao cronologica) continua so pedindo perto do fim
  assert(!vertudo_quer_mais(0, 15, 0, 600, 0, 0, 0));
  assert(vertudo_quer_mais(0, 15, 5, 600, 0, 0, 0));
  assert(!vertudo_quer_mais(1, 15, 0, 600, 1, 0, 0));
  assert(!vertudo_quer_mais(1, 15, 0, 600, 0, 1, 0));
  assert(!vertudo_quer_mais(1, 600, 0, 600, 0, 0, 0));
  puts("vertudo paginas: PASS");
  return 0;
}
