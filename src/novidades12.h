// Cartao de NOVIDADES DA 1.2 — tres paginas. Ver novidades12.c para o porque.
#ifndef NV_NOVIDADES12_H
#define NV_NOVIDADES12_H
#include <SDL2/SDL.h>

// Chama uma vez por quadro; decide sozinho (arquivo-marca na pasta de dados).
// So abre de fato na primeira execucao em que a home esta pronta.
void novidades12_primeira_vez(void);
int  novidades12_aberto(void);
void novidades12_evento(const SDL_Event *e);
void novidades12_atualizar(float dt, Uint32 agora);
void novidades12_desenhar(Uint32 agora);

// Abre o cartao sem consultar a marca. Existe para a captura de tests/ e para
// um futuro "ver de novo" nos Ajustes; o app nao chama isto.
void novidades12_abrir(int pagina);

#endif
