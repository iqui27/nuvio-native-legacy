// TOCAR ENQUANTO CONFERE (2.0.2): o player abre com a primeira candidata do
// automatico no mesmo instante em que a conferencia dela comeca, em vez de
// esperar o veredito. Aqui mora so o ESTADO COMPARTILHADO entre o fio que
// confere (streams.c) e o fio principal que liga o player (app.c), sem rede,
// sem SDL e sem a lista de streams: tests/fonteantecipa.c exercita os recuos
// sem TV.
//
// A conferencia continua sendo a mesma, uma candidata por vez, e quem decide
// a ordem continua sendo a fila de fonteauto.c. Esta e so a PRIMEIRA da fila e
// so quando ela ja tem um link tocavel (sem resolver torrent antes). Se ela
// nao serve — nao e video, endereco de aviso, playlist vazia, morta — ou se o
// player errou antes do veredito, o app volta a esperar e a fila segue para a
// proxima exatamente como seguia. Nunca baixa a qualidade por causa disto.
#ifndef NV_FONTEANTECIPA_H
#define NV_FONTEANTECIPA_H

enum { FA_NADA = 0, FA_CONFERINDO, FA_OK, FA_RUIM };

// Fio principal, ANTES de pedir uma escolha: abre uma rodada nova e apaga o que
// a anterior publicou. Um fio velho que ainda esteja conferindo perde o direito
// de publicar (o token dele ja nao e o atual).
unsigned fa_nova_rodada(void);
// Fio que confere, no comeco da escolha: o token da rodada em curso.
unsigned fa_rodada_atual(void);

// Fio que confere: `indice` e a candidata que o player ja pode abrir, e
// `geracao` a da lista (streams.c descarta se a lista trocou). Sem efeito se
// a rodada ja nao for a atual.
void fa_publicar(unsigned rodada, int indice, unsigned geracao);
// O veredito da conferencia daquela candidata. Sem efeito fora da rodada.
void fa_concluir(unsigned rodada, int indice, int servi);

// Fio principal: o estado da rodada atual. Devolve o indice publicado (ou -1)
// e o estado em *estado; *geracao recebe a da lista na publicacao.
int fa_ver(int *estado, unsigned *geracao);

// Fio principal: o player errou na candidata aberta antes do veredito. A
// conferencia, ao terminar, trata a candidata como ruim mesmo que ela diga
// que serve. fa_player_falhou_foi() e a pergunta do outro lado.
void fa_player_falhou(int indice);
int  fa_player_falhou_foi(unsigned rodada, int indice);

// A DECISAO DO FIO PRINCIPAL, por quadro, enquanto a escolha esta em curso.
//   abertaIdx  a candidata que o player abriu antes do veredito (-1 = nenhuma)
//   pubIdx/estado  o que o fio que confere publicou (fa_ver, ja conferido com a
//              geracao da lista)
//   playerFalhou  o player marcou erro
enum { FA_ACAO_NADA = 0, FA_ACAO_ABRIR, FA_ACAO_DESFAZER_VEREDITO, FA_ACAO_DESFAZER_PLAYER };
int fa_acao(int abertaIdx, int pubIdx, int estado, int playerFalhou);
// A escolha final saiu: 1 = o player ja toca essa mesma fonte (nao reabre nem
// recomeca a contagem); 0 = a aberta antes (se houver) tem de ser desfeita.
int fa_ja_tocando(int abertaIdx, int escolhida);

// FONTE PREPARADA AO ABRIR O TITULO (opcional): quando a escolha feita na pagina
// do titulo ainda serve a um Play. Sem estado nem rede: o fio principal passa o
// que sabe. 1 = serve (abre sem conferir de novo); 0 = descarta e o Play escolhe
// como sempre. So serve para o MESMO alvo (titulo e episodio), a MESMA lista, a
// MESMA fonte lembrada, enquanto o link vale e a fonte ainda esta no automatico.
#define FA_PREPARADA_VALE_MS 45000u
#define FA_LINK_VALE_MS 60000u
typedef struct { const char *alvo; unsigned lista; int lembrada, idx; unsigned idadeMs; } FaPreparada;
typedef struct { const char *alvo; unsigned lista; int lembrada, n, idxDisponivel; unsigned idadeListaMs; } FaAgora;
int fa_preparada_vale(const FaPreparada *p, const FaAgora *a);

#endif
