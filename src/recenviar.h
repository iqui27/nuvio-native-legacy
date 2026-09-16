// RECOMENDAR A UM AMIGO — a superficie compartilhada do envio e dos amigos.
//
// POR QUE ELA SAIU DE ctxmenu.c. O fluxo "escolho o amigo, escolho a frase"
// nasceu dentro do menu de contexto do cartaz porque era a unica porta que
// existia. Quando o botao circular da tela de detalhe pediu o MESMO fluxo, as
// opcoes eram duas: chamar ctxmenu de dentro de detail.c (e o menu do cartaz
// nao e o menu do detalhe — ele fala de um indice da home, tem "Ver detalhes" e
// abre por pressao longa), ou duplicar as duas telas. A terceira saida e esta:
// as telas viraram modulo, e ctxmenu.c e detail.c passaram a abrir a MESMA
// modal. Nao ha duas listas de contatos, nem dois avisos de "Enviado para X",
// nem duas formas de voltar um passo.
//
// SEM URL COMPILADA ELA NAO ABRE. recenviar_abrir devolve 0 quando
// recomenda_ativo() e 0, e quem desenha o botao ja o esconde antes disso — a
// mesma regra de todo o resto desta funcao.
#ifndef NV_RECENVIAR_H
#define NV_RECENVIAR_H

#include "catalogo.h"
#include <SDL2/SDL.h>

// Abre em "Para quem?" com o titulo a recomendar. O CatItem e COPIADO: o vetor
// de catalogo.c troca de bloco a cada republicacao da descoberta, e guardar o
// ponteiro por varios segundos e exatamente o defeito que derrubou
// salvospainel.c na TV (ver a nota longa la). 1 quando abriu.
int  recenviar_abrir(const CatItem *ci);

// Abre direto na tela de amigos, sem titulo nenhum a enviar. E a porta da aba
// Social e do estado vazio: "nao tenho amigos" nao se resolve escolhendo um
// filme primeiro.
int  recenviar_abrir_amigos(void);

int  recenviar_aberto(void);
void recenviar_evento(const SDL_Event *e);
void recenviar_atualizar(float dt, Uint32 agora);
void recenviar_desenhar(Uint32 agora);

#endif
