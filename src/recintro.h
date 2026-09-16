// Explicador de primeira vez do SOCIAL — tres paginas. Ver recintro.c.
#ifndef NV_RECINTRO_H
#define NV_RECINTRO_H
#include <SDL2/SDL.h>

// Chama uma vez por quadro, com a home de pe; decide sozinho (arquivo-marca na
// pasta de dados) e nunca abre num pacote sem NUVIO_REC_URL compilada.
void recintro_primeira_vez(void);
int  recintro_aberto(void);
void recintro_evento(const SDL_Event *e);
void recintro_atualizar(float dt, Uint32 agora);
void recintro_desenhar(Uint32 agora);

#endif
