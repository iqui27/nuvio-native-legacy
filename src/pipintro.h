// PiP de canal — a pergunta da primeira saida. Ver pipintro.c para o porque.
#ifndef NV_PIPINTRO_H
#define NV_PIPINTRO_H
#include <SDL2/SDL.h>

// -1 = ainda nao decidiu (mostrar o cartao), 1 = PiP ao sair, 0 = fecha de vez.
int  pipintro_decisao(void);
// Abre o cartao so quando a decisao ainda nao existe.
void pipintro_abrir(void);
int  pipintro_aberto(void);
void pipintro_evento(const SDL_Event *e);
void pipintro_atualizar(float dt, Uint32 agora);
void pipintro_desenhar(Uint32 agora);

#endif
