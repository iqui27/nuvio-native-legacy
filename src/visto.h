// "MARCAR COMO ASSISTIDO" PARA TODOS OS DESTINOS VINCULADOS.
//
// O pedido do dono: "o mark as watch tem que funcionar tanto no nuvio, como no
// simkl. parece que sem o traktv nao ta dando o watched". Era verdade em dois
// pontos, medidos no codigo de 226af57:
//   - ctxmenu.c (OP_ASSISTIDO) chamava so trakt_assistido_tipo, que sem Trakt
//     devolve 0 ("[trakt] historico recusado: Trakt desligado") e o modal ia
//     para CTX_FALHA: nada local, nada na conta, nada no Simkl;
//   - o lote de episodios (episodios.c) ia a Trakt e conta, nunca ao Simkl, e
//     o "temporada inteira" saia do mapa vistoep — que sem Trakt so conhece o
//     que ja foi visto, entao a temporada virava "0 episodios".
//
// Este modulo e o unico lugar que sabe QUAIS destinos existem. Cada tela muda
// o local na hora e chama daqui; o envio vai num fio porque as tres escritas
// sao sincronas e cada uma pode levar 20 s de timeout.
//
// DESTINOS:
//   Trakt  POST /sync/history[/remove]           (trakt.c, se ligado)
//   Simkl  POST /sync/history[/remove]           (simkl.c, se vinculado)
//   Conta  sync_push|delete_watched_items          (syncprog.c, se logada)
// Com mais de um vinculado vai a todos. O Trakt do TITULO inteiro continua em
// trakt_assistido_tipo (ctxmenu.c depende do estado dele); aqui o Trakt so
// entra no lote de episodios, com a mesma funcao e o mesmo corpo de antes.
#ifndef NV_VISTO_H
#define NV_VISTO_H
#include "vistoep.h"

enum { VISTO_TRAKT = 1, VISTO_SIMKL = 2, VISTO_CONTA = 4 };

// Bits do que esta vinculado AGORA.
int visto_destinos(void);

// Lote de episodios, em fio. 1 quando o fio saiu (ou nao havia destino); 0 em
// argumento invalido. `destinos` e uma mascara de VISTO_*.
int visto_episodios(const char *imdb, const char *tipo, const VistoPar *pares,
                    int n, int visto, int destinos);
// O mesmo, SINCRONO — para o teste e para quem ja esta num fio.
int visto_episodios_ja(const char *imdb, const char *tipo, const VistoPar *pares,
                       int n, int visto, int destinos);

// Titulo inteiro (filme ou serie), em fio. VISTO_TRAKT e IGNORADO aqui: ver o
// cabecalho. `temporadas` (numeros) so importa para desmarcar serie no Simkl.
int visto_titulo(const char *imdb, const char *tipo, const int *temporadas,
                 int nt, int visto, int destinos);
int visto_titulo_ja(const char *imdb, const char *tipo, const int *temporadas,
                    int nt, int visto, int destinos);

#endif
