// O ANO DE UM TITULO, lido da linha de meta do catalogo ("2022 · 3 temporadas").
//
// Header so com uma funcao inline: a pagina de colecao (vertudo.c) e a fileira
// "Linha do tempo" da home (home.c) ordenam pelo mesmo numero, e uma regra
// repetida em dois arquivos diverge no primeiro "1999–2004" que aparecer.
// 0 = nao ha ano. Quatro digitos soltos entre 1880 e 2099; o primeiro vence
// ("2008–2013" e 2008, a estreia).
#ifndef NV_ANOS_H
#define NV_ANOS_H
#include <stdlib.h>

static inline int nv_ano_meta(const char *p) {
  for (; p && p[0]; p++)
    if (p[0] >= '0' && p[0] <= '9' && p[1] >= '0' && p[1] <= '9' &&
        p[2] >= '0' && p[2] <= '9' && p[3] >= '0' && p[3] <= '9' &&
        !(p[4] >= '0' && p[4] <= '9')) {
      int a = atoi(p);
      if (a > 1880 && a < 2100) return a;
      p += 3;
    }
  return 0;
}
#endif
