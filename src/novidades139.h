// Cartao de NOVIDADES DA 1.3.9 — uma pagina: fundos do Xperience, abas da
// colecao e a Samsung sem engasgo ao rolar. Ver novidades139.c.
#ifndef NV_NOVIDADES139_H
#define NV_NOVIDADES139_H
#include <SDL2/SDL.h>

void novidades139_primeira_vez(void);
int  novidades139_aberto(void);
void novidades139_evento(const SDL_Event *e);
void novidades139_atualizar(float dt, Uint32 agora);
void novidades139_desenhar(Uint32 agora);
void novidades139_abrir(void);

#endif
