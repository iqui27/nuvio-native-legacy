// Cartao de NOVIDADES DA 1.3.12 — uma pagina celebrando fluidez, historias e
// a comunidade que faz o Nuvio continuar crescendo.
#ifndef NV_NOVIDADES1312_H
#define NV_NOVIDADES1312_H
#include <SDL2/SDL.h>

void novidades1312_primeira_vez(void);
int  novidades1312_aberto(void);
void novidades1312_evento(const SDL_Event *e);
void novidades1312_atualizar(float dt, Uint32 agora);
void novidades1312_desenhar(Uint32 agora);
void novidades1312_abrir(void);

#endif
