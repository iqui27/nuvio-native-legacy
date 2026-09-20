// Cartao de NOVIDADES DA 1.3.3 — uma pagina: fundos do Xperience, abas da
// colecao e a Samsung sem engasgo ao rolar. Ver novidades133.c.
#ifndef NV_NOVIDADES133_H
#define NV_NOVIDADES133_H
#include <SDL2/SDL.h>

void novidades133_primeira_vez(void);
int  novidades133_aberto(void);
void novidades133_evento(const SDL_Event *e);
void novidades133_atualizar(float dt, Uint32 agora);
void novidades133_desenhar(Uint32 agora);
void novidades133_abrir(void);

#endif
