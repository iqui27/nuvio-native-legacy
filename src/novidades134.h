// Cartao de NOVIDADES DA 1.3.4 — uma pagina: fundos do Xperience, abas da
// colecao e a Samsung sem engasgo ao rolar. Ver novidades134.c.
#ifndef NV_NOVIDADES134_H
#define NV_NOVIDADES134_H
#include <SDL2/SDL.h>

void novidades134_primeira_vez(void);
int  novidades134_aberto(void);
void novidades134_evento(const SDL_Event *e);
void novidades134_atualizar(float dt, Uint32 agora);
void novidades134_desenhar(Uint32 agora);
void novidades134_abrir(void);

#endif
