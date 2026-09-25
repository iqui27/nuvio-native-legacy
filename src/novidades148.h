// Cartao de NOVIDADES DA 1.4.8 — a previa da cor viva (tres titulos do pacote
// trocando de cor) e as mudancas da versao em cinco grupos curtos.
#ifndef NV_NOVIDADES148_H
#define NV_NOVIDADES148_H
#include <SDL2/SDL.h>

// Pasta da arte do pacote (a mesma de app_iniciar): a previa usa fundo e logo
// de tres titulos dela, sem rede.
void novidades148_dir(const char *dirArte);
void novidades148_primeira_vez(void);
int  novidades148_aberto(void);
void novidades148_evento(const SDL_Event *e);
void novidades148_atualizar(float dt, Uint32 agora);
void novidades148_desenhar(Uint32 agora);
void novidades148_abrir(void);

// O que o cartao pediu ao fechar, uma unica vez: quem chama abre a tela.
#define N148_PEDIU_NADA        0
#define N148_PEDIU_COR         1   // Ajustes, na linha "Cor de destaque"
#define N148_PEDIU_VELOCIDADE  2   // Diagnostico, direto no teste de velocidade
int  novidades148_pedido(void);

// Para o teste: 1 quando as paletas dos tres titulos ja chegaram.
int  novidades148_previa_pronta(void);

#endif
