// Tetos do guia de canais e a CONTA DO QUE FICOU DE FORA.
//
// O guia guarda no maximo GUIACORTE_MAX_CANAL canais em GUIACORTE_MAX_CAT
// categorias, na ordem em que o servidor manda. Um provedor com milhares de
// canais era cortado sem aviso e a pessoa so via o comeco da lista (um usuario
// de Shield achou que o app so tinha Reino Unido e EUA). Aqui se conta quem
// entrou e quem nao, e por que, para a tela dizer "Mostrando N de M".
#ifndef NV_GUIACORTE_H
#define NV_GUIACORTE_H
#include <stdio.h>

// TV: 900 canais / 48 categorias (GCanal tem ~2,4 KB e o guia mantem tres
// vetores dele: 900 x 2,4 KB x 3 = ~6,5 MB, numa TV de 2016 que ainda guarda
// cache de texturas). Android TV: 3000 / 128. Os vetores sao estaticos (BSS) e
// o Linux so aloca a pagina que e tocada: custa ~2,4 KB x canais lidos x 3,
// e no teto 3000 x 2,4 KB x 3 = ~21,6 MB, em aparelhos de 2 GB ou mais. O
// cache em disco do guia tem o mesmo tamanho (~7 MB no teto).
#ifdef NV_ANDROID
#define GUIACORTE_MAX_CANAL 3000
#define GUIACORTE_MAX_CAT    128
#else
#define GUIACORTE_MAX_CANAL  900
#define GUIACORTE_MAX_CAT     48
#endif

typedef struct {
  int porCanais;   // perdidos porque o teto de canais encheu
  int porCats;     // perdidos porque o teto de categorias encheu
} GuiaCorte;

// Um canal chega: `nAtual` ja guardados, `catOk` = a categoria dele coube.
// Devolve 1 se entra; se nao, soma aos perdidos pelo motivo certo.
static inline int guiacorte_entra(GuiaCorte *k, int nAtual, int catOk) {
  if (nAtual >= GUIACORTE_MAX_CANAL) { k->porCanais++; return 0; }
  if (!catOk) { k->porCats++; return 0; }
  return 1;
}
// Canais que a fonte tinha mas o corte da fonte ja descartou (o Xtream conta o
// total que o servidor mandou alem do que cabia no pedido).
static inline void guiacorte_perdeu_canais(GuiaCorte *k, int n) { if (n > 0) k->porCanais += n; }

static inline int guiacorte_por_canais(const GuiaCorte *k) { return k->porCanais > 0; }
static inline int guiacorte_por_cats(const GuiaCorte *k)   { return k->porCats > 0; }
static inline int guiacorte_cortou(const GuiaCorte *k)     { return k->porCanais + k->porCats > 0; }
static inline int guiacorte_total(const GuiaCorte *k, int guardados) {
  return guardados + k->porCanais + k->porCats;
}
// Para o log: "canais", "categorias", "canais+categorias" ou "nenhum".
static inline const char *guiacorte_motivo(const GuiaCorte *k) {
  return k->porCanais && k->porCats ? "canais+categorias"
       : k->porCanais ? "canais" : k->porCats ? "categorias" : "nenhum";
}
// "Mostrando %d de %d canais ..." com o modelo ja traduzido (dois %d).
static inline void guiacorte_linha(const GuiaCorte *k, int guardados, const char *modelo,
                                   char *dst, size_t n) {
  snprintf(dst, n, modelo, guardados, guiacorte_total(k, guardados));
}
#endif
