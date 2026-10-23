// Home sem add-ons (J1 / J2a(b)): em vez de uma Home em branco, um cartao de
// vidro com o QR do guia "configure no celular, a TV acompanha" e o botao que
// puxa a conta na hora.
#ifndef NV_HOMEVAZIO_H
#define NV_HOMEVAZIO_H
#include <SDL2/SDL.h>

#define HOMEVAZIO_URL "https://github.com/iqui27/nuvio-native-legacy/blob/master/docs/guia/phone-setup.md"

// Sem addons ou sem fileiras e sem addon de catalogo, depois do sync.
int  homevazio_ativo(int fileiras);
// Desenha o cartao; `alfa` em 0..1.
void homevazio_desenhar(float alfa);
// OK no botao "Sincronizar addons". 1 se consumiu a tecla.
int  homevazio_evento(const SDL_Event *e);

#endif
