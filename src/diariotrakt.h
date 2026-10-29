// O TRAKT COMO FONTE DO DIARIO (plano 4.2, item 3). Barato de proposito:
//   1. /sync/watched/movies e /sync/watched/shows (um pedido cada): a
//      COMPLETUDE — o que ja foi visto, com a data da ultima vez;
//   2. /sync/history?start_at=<1 de janeiro> so do ano corrente: as DATAS de
//      cada vez (Retrospectiva, Relogio), paginado de 100 em 100 (teto 20);
//   3. depois, incremental: start_at=<ultima vez importada> (cursor em
//      diario-trakt-p<N>.txt). No maximo uma volta a cada 6 h por perfil.
// Um fio de baixa prioridade; nada no quadro. So com o Trakt ligado.
#ifndef NV_DIARIOTRAKT_H
#define NV_DIARIOTRAKT_H
#include "diario.h"

// Laco principal: dispara a volta quando cabe (barato: um relogio).
void diariotrakt_passo(void);

// --- puro, para teste ---------------------------------------------------------
// Corpo de /sync/history: cada item vira uma entrada 't'. Devolve quantas;
// `maisNovo` recebe o maior watched_at (epoch), para o cursor.
int diariotrakt_ler_historico(const char *json, DiarioEntrada *v, int max,
                              long long *maisNovo);
// Corpo de /sync/watched/movies (`series` 0) ou /shows (1).
int diariotrakt_ler_vistos(const char *json, int series, DiarioEntrada *v, int max);
#endif
