// Novidades da versao — cartao de primeira vez. Ver novidades.c para o porque.
#ifndef NV_NOVIDADES_H
#define NV_NOVIDADES_H
#include <SDL2/SDL.h>

// Chama uma vez por quadro; decide sozinho (arquivo-marca na pasta de dados).
// So abre de fato na primeira execucao em que a home esta pronta.
void novidades_primeira_vez(void);
int  novidades_aberto(void);
void novidades_evento(const SDL_Event *e);
void novidades_atualizar(float dt, Uint32 agora);
void novidades_desenhar(Uint32 agora);

#endif
