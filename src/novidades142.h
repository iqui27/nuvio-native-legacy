// Cartao de NOVIDADES DA 1.4.2 — cinco destaques curtos e um convite para
// rodar o diagnostico ali mesmo.
#ifndef NV_NOVIDADES142_H
#define NV_NOVIDADES142_H
#include <SDL2/SDL.h>

void novidades142_primeira_vez(void);
int  novidades142_aberto(void);
// 1 enquanto o cartao ainda vai aparecer nesta sessao (nao visto e nao
// decidido) ou esta aberto. app.c usa para NAO abrir a apresentacao global
// do diagnostico antes dele: o cartao ja convida para o diagnostico, e dois
// avisos seguidos sobre a mesma coisa e um a mais.
int  novidades142_pendente(void);
void novidades142_evento(const SDL_Event *e);
void novidades142_atualizar(float dt, Uint32 agora);
void novidades142_desenhar(Uint32 agora);
void novidades142_abrir(void);
// 1 uma unica vez depois de "Rodar o diagnostico": quem chama abre a tela.
int  novidades142_pediu_diagnostico(void);

#endif
