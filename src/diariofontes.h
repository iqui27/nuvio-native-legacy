// FONTES DO DIARIO: quem alimenta diario.c com o que o app ja sabe.
//   player  fim da vez (90% ou creditos), com seg tocado, hora de inicio, cor
//           da arte e duracao; tambem o acompanhamento do "Pegou no sono"
//   conta   os vistos de sync_pull_watched_items (contalib), quando mudam
//   nomes   titulo/ano/capa do CatItem e da biblioteca da conta
// Fio principal.
#ifndef NV_DIARIOFONTES_H
#define NV_DIARIOFONTES_H
#include "catalogo.h"

// Mesma condicao do atividade_player_passo (video pronto, duracao >= 120 s).
void diariofontes_player_passo(const CatItem *ci, int temporada, int episodio,
                               double posSeg, double durSeg, int tocando,
                               int concluiu, float dt);
void diariofontes_player_saiu(double posSeg, double durSeg, int concluiu);
// Laco principal, uma vez por quadro (barato: le uma revisao).
void diariofontes_passo(void);
// Um titulo apareceu na tela (pagina do titulo): guarda nome e capa.
void diariofontes_nome(const CatItem *ci);
// "2022 · 3 temporadas" -> 2022; 0 se nao comeca com ano.
int  diariofontes_ano(const char *meta);
#endif
