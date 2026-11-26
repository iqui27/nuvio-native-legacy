// MARCAR CENA no app: guarda as marcas do perfil, abre o envio a um amigo e
// arma a abertura "naquele instante". A regra pura esta em marcacena.h.
#ifndef NV_CENAS_H
#define NV_CENAS_H
#include "marcacena.h"

// A tecla de marcar foi apertada no player (so no fio principal). A primeira vez
// guarda a marca; apertar de novo em CENAS_REENVIO_MS abre "Para quem?".
void cenas_tecla(void);
int  cenas_n(void);
int  cenas_item(int i, MarcaCena *saida);   // 0 = a mais antiga

// Arma a abertura de `imdb` em `seg` (episodio 0 = qualquer). Vale para o
// proximo Play desse titulo, por CENAS_ARMADA_MS.
void cenas_armar(const char *imdb, int temporada, int episodio, int seg, int durOrigem);
// Chamar logo apos player_abrir + episodio definido: se a abertura armada e
// deste titulo, o player comeca naquele instante. Devolve 1 se aplicou.
int  cenas_aplicar_ao_abrir(void);
// Com a duracao local conhecida, avisa uma vez se a marca veio de outra versao.
void cenas_atualizar(void);

#endif
