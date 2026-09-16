// Cartao de NOVIDADES DA 1.1 — sete paginas. Ver novidades11.c para o porque.
#ifndef NV_NOVIDADES11_H
#define NV_NOVIDADES11_H
#include <SDL2/SDL.h>

// Chama uma vez por quadro; decide sozinho (arquivo-marca na pasta de dados).
// So abre de fato na primeira execucao em que a home esta pronta.
void novidades11_primeira_vez(void);
int  novidades11_aberto(void);
void novidades11_evento(const SDL_Event *e);
void novidades11_atualizar(float dt, Uint32 agora);
void novidades11_desenhar(Uint32 agora);

// Abre o cartao sem consultar a marca. Existe para a captura de tests/ e para
// um futuro "ver de novo" nos Ajustes; o app nao chama isto.
void novidades11_abrir(int pagina);

#endif
