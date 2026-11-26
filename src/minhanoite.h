// MINHA NOITE: o canal pessoal do "Ligou, tocou" (pesquisa de mercado, 4.1).
//
// REGRA PURA, sem rede, sem relogio proprio e sem estado: recebe o que a
// pessoa tem em andamento (Continuar assistindo, series em andamento, lista) e
// um instante, e devolve o que "esta no ar" naquele instante. A grade do dia
// sai so da data e dos titulos, entao duas TVs da mesma conta mostram a mesma
// programacao e reabrir o app nao embaralha nada. Quem toca e app.c; quem
// decide se a sessao conta como assistida e o player (player_passivo).
#ifndef NV_MINHANOITE_H
#define NV_MINHANOITE_H

#include <time.h>

// O id do canal na ordem de zapping. Nao e id de addon nem de lista.
#define MN_ID  "nuvio:minha-noite"
#define MN_MAX 32

// A origem decide a prioridade na grade: o que esta pela metade vem antes.
enum { MN_CONTINUAR = 0, MN_SERIE = 1, MN_LISTA = 2 };

// Duracao nominal quando o catalogo nao diz a real (so para montar a grade; o
// ponto de entrada e uma FRACAO do programa, nunca segundos absolutos).
#define MN_DUR_FILME_S    (110 * 60)
#define MN_DUR_EPISODIO_S (45 * 60)
#define MN_DUR_MIN_S      (10 * 60)
#define MN_DUR_MAX_S      (240 * 60)
// Entrar nos creditos nao serve a ninguem: a entrada nunca passa disto.
#define MN_ENTRADA_MAX    0.85

typedef struct {
  char id[64];
  char tipo[8];          // "movie" | "series"
  char titulo[96];
  int  temporada, episodio;   // serie; 0 em filme
  int  durSeg;                // 0 = nominal pelo tipo
  int  origem;                // MN_*
} MnEntrada;

typedef struct { int n; MnEntrada e[MN_MAX]; } MnFonte;

typedef struct {
  int    indice;         // em MnFonte.e
  time_t ini, fim;       // o bloco na grade
  double fracao;         // 0..MN_ENTRADA_MAX: onde o programa esta agora
} MnPrograma;

void mn_fonte_limpar(MnFonte *f);
// Junta uma entrada. O mesmo id fica uma vez so, com a origem mais prioritaria.
// 1 = entrou ou atualizou; 0 = invalida ou sem espaco.
int  mn_fonte_juntar(MnFonte *f, const MnEntrada *e);

// O numero do dia local (dias desde 1970) de `t` com o fuso `fusoSeg`.
long mn_dia(time_t t, long fusoSeg);
// O programa no ar em `t`. 0 = canal vazio.
int  mn_programa(const MnFonte *f, time_t t, long fusoSeg, MnPrograma *p);
// O k-esimo programa DEPOIS do que esta no ar (k = 0 e o proximo).
int  mn_proximo(const MnFonte *f, time_t t, long fusoSeg, int k, MnPrograma *p);

// A ORDEM DE ZAPPING: Minha noite fica entre o ultimo e o primeiro canal.
//   naMinha  1 = a pessoa esta na Minha noite
//   temMinha 1 = a Minha noite tem programacao
//   daVolta  1 = o CH+/- deste canal de IPTV passaria da ponta da lista
enum { MN_ZAP_IPTV = 0,     // zap normal dentro da lista
       MN_ZAP_MINHA,        // sintoniza a Minha noite
       MN_ZAP_PONTA };      // sai da Minha noite para a ponta da lista
int mn_zap(int naMinha, int temMinha, int daVolta);

// NADA CONTA COMO ASSISTIDO ATE UM GESTO: a sessao aberta pelo canal comeca
// passiva e so um destes gestos a torna uma sessao comum.
enum { MN_GESTO_NENHUM = 0, MN_GESTO_DO_INICIO, MN_GESTO_DAQUI };
int mn_conta(int passiva, int gesto);   // 1 = a sessao passa a contar

#endif
