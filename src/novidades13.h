// Cartao de NOVIDADES DA 1.3 — uma pagina: Xtream. Ver novidades13.c.
#ifndef NV_NOVIDADES13_H
#define NV_NOVIDADES13_H
#include <SDL2/SDL.h>

void novidades13_primeira_vez(void);
int  novidades13_aberto(void);
void novidades13_evento(const SDL_Event *e);
void novidades13_atualizar(float dt, Uint32 agora);
void novidades13_desenhar(Uint32 agora);
void novidades13_abrir(void);

#endif
