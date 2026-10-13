// watch_state v2: a ponta com rede e disco. O contrato (corpo, id, fila, retry)
// vive em watchstate.c e e testado sem rede; aqui so ha o diario em disco e um
// fio de fundo que entrega. Quem toca o video nunca espera por ele.
#ifndef NV_WATCHSTATE_ENVIO_H
#define NV_WATCHSTATE_ENVIO_H

#include "watchstate.h"

// Le o diario e acorda o fio para o que ficou pendente da sessao anterior.
// Idempotente; chamar no arranque (e o primeiro wsenvio_emitir tambem chama).
void wsenvio_iniciar(void);

// Entrega `e` a todo addon instalado cujo manifesto pede esse evento. Volta na
// hora: so enfileira. Nao faz nada sem rede com POST (WGT), em perfil que usa os
// addons de outro, nem para evento invalido.
void wsenvio_emitir(const WsEvento *e);

// Tira do diario o que a lista DE QUEM CHAMA nao autoriza mais. So quem aplica
// a lista certa do usuario pode chamar: addons_definir_lista (a lista da CONTA)
// e addons_esquecer (logout, lista vazia). A lista do PACOTE (addons_carregar,
// art/addons.txt) NAO pode: no arranque ela substitui a da conta e podar contra
// ela apagava o backlog de todo boot — quem a lista de agora nao conhece fica
// segurado no envio (WS_AUSENTE) ate a lista da conta voltar.
void wsenvio_podar(void);

// O pendente DESTE addon sai do diario — e so dele. Para o desligar/remover de
// um addon especifico (addons_alternar), onde a poda por ausencia contra a
// lista de agora pegaria mais do que deve (a lista pode ser a de exemplo).
void wsenvio_podar_addon(const char *base);

#endif
