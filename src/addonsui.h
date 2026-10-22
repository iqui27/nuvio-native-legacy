// Lista de addons da conta: o que cada um fornece, e ligar/desligar aqui.
//
// POR QUE ESTA TELA EXISTE. A lista vinha da conta e era usada em silencio:
// nao havia como ver quais addons estao valendo, nem descobrir que um deles
// nao fornece legenda, nem desligar um que esta quebrando a busca de fontes —
// tudo isso exigia pegar o celular e abrir o app web.
//
// O QUE ELA MOSTRA E MEDIDO, NAO SUPOSTO: as capacidades saem do manifesto de
// cada addon (addons_sondar_manifestos). Enquanto a sonda nao respondeu, a
// linha diz "conferindo" em vez de afirmar o que ainda nao sabe.
#ifndef NV_ADDONSUI_H
#define NV_ADDONSUI_H
#include <SDL2/SDL.h>

void addonsui_abrir(void);
void addonsui_evento(const SDL_Event *e);
void addonsui_atualizar(float dt, Uint32 agora);
void addonsui_desenhar(Uint32 agora);
int  addonsui_quer_sair(void);
// Ponteiro (#99): foca a linha i (PonteiroFn). addonsui_foco para os testes.
void addonsui_ponteiro(int i, int b);
int  addonsui_foco(void);

#endif
