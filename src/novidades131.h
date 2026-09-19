// Cartao de NOVIDADES DA 1.3.1 — uma pagina: velocidade da arte, medida, e
// o ajuste de qualidade de imagem. Ver novidades131.c.
#ifndef NV_NOVIDADES131_H
#define NV_NOVIDADES131_H
#include <SDL2/SDL.h>

void novidades131_primeira_vez(void);
int  novidades131_aberto(void);
void novidades131_evento(const SDL_Event *e);
void novidades131_atualizar(float dt, Uint32 agora);
void novidades131_desenhar(Uint32 agora);
void novidades131_abrir(void);

#endif
