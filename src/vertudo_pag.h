#ifndef VERTUDO_PAG_H
#define VERTUDO_PAG_H
// Quando a tela Ver tudo pede a proxima pagina do catalogo.
// `cronologico`: exibicao ordenada por ano (Linha do tempo, Saga, Decadas).
static inline int vertudo_quer_mais(int cronologico, int n, int foco, int max,
                                    int carregando, int fim, int erro) {
  if (carregando || fim || erro) return 0;
  // Ordenada por ano, a tela so mostra o comeco certo depois de ter tudo.
  if (cronologico) return n < max;
  return foco >= n - 12;
}
#endif
