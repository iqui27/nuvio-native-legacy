// Cartao de NOVIDADES DA 1.3.2 — uma pagina: velocidade da arte, medida, e
// o ajuste de qualidade de imagem. Ver novidades132.c.
#ifndef NV_NOVIDADES132_H
#define NV_NOVIDADES132_H
#include <SDL2/SDL.h>

void novidades132_primeira_vez(void);
int  novidades132_aberto(void);
void novidades132_evento(const SDL_Event *e);
void novidades132_atualizar(float dt, Uint32 agora);
void novidades132_desenhar(Uint32 agora);
void novidades132_abrir(void);

#endif
