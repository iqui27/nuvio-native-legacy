// OS ATALHOS DA CENTRAL DE CONTROLE (central.h): quais opcoes de Ajustes viram
// botao na central e em que ordem. Sem SDL nem arquivo: tests/central.sh.
//
// O CATALOGO e uma lista de CHAVES do ajustes.txt ("dolbyVision") e o icone
// de cada uma. Rotulo, valores e o que muda ao trocar continuam em ajustes.c
// (ajustes_rapido_*): aqui nao ha copia de valor nenhum. Chave que o build
// nao tem na tela de Ajustes fica de fora sozinha (ajustes_rapido_op = -1).
//
// A LISTA da pessoa mora em central-p<N>.txt (por perfil, nesta TV), uma
// chave por linha, na ordem dos botoes. "-" sozinho = lista vazia de
// proposito (sem o arquivo valem os de fabrica). Chave desconhecida e pulada:
// um arquivo de versao mais nova nao derruba a mais velha.
#ifndef NV_CENTRALLISTA_H
#define NV_CENTRALLISTA_H
#include <stddef.h>

#define CENTRAL_MAX 27   // One slot per catalog tile; checked against CATALOGO.

// `curto`: o nome no botao (o rotulo inteiro de Ajustes nao cabe num botao de
// ~120 px; o inteiro continua na lista de edicao). Chave em portugues, i18n.
typedef struct { const char *chave, *icone, *curto; } CentralItem;
typedef struct { int n; int item[CENTRAL_MAX]; } CentralLista;   // indices do catalogo

int  central_catalogo_n(void);
const CentralItem *central_catalogo(int i);
int  central_catalogo_achar(const char *chave);   // -1 se nao esta no catalogo

void centrallista_padrao(CentralLista *l);
// Le o texto do arquivo. NULL ou vazio = os de fabrica.
void centrallista_ler(CentralLista *l, const char *texto);
void centrallista_escrever(const CentralLista *l, char *dst, size_t n);
int  centrallista_tem(const CentralLista *l, int item);
// Poe no fim ou tira. 1 = pos, 0 = tirou, -1 = cheia (nada muda).
int  centrallista_alternar(CentralLista *l, int item);

#endif
