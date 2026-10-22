// O CARTAO DAS ESTATISTICAS AO VIVO no player (mockup stats-velocidade, telas
// 2 e 3, aprovado em 07/10). Abre pelo atalho "Estatisticas" da Central de
// controle; a conta e a origem de cada numero sao de estatvivo.h.
//
// Aberto ele e uma camada MODAL como a central: OK fixa no canto (uma pilula
// pequena que nao rouba tecla nenhuma), Voltar fecha. A amostra corre 1x/s
// enquanto houver video no player, aberto ou nao — e o que da o pico, a media e
// as reconexoes "desde que o filme abriu".
//
// CUSTO NO PLAYER (TCL/Mali sao limitados pela GPU): retangulos solidos, sem
// desfoque nem sombra grande; os textos mudam 1x/s e moram no cache de texto.
#ifndef NV_ESTATVIVOUI_H
#define NV_ESTATVIVOUI_H
#include <SDL2/SDL.h>
#include "estatvivo.h"

void estatvivo_abrir(void);
void estatvivo_fechar(void);
int  estatvivo_aberto(void);       // o cartao grande (modal)
int  estatvivo_fixado(void);       // a pilula do canto
// Teclado do cartao aberto. 1 = consumiu.
int  estatvivo_evento(const SDL_Event *e);
// Uma vez por quadro (barato: so trabalha 1x/s).
void estatvivo_atualizar(Uint32 agora);
void estatvivo_desenhar(Uint32 agora);
// A velocidade mais recente (kbps) para a linha "Tocando agora" da central;
// -1 sem numero. `*estimada` = 1 quando nao e medida.
int  estatvivo_kbps(int *estimada);

#ifdef ESTATVIVO_TESTE
// Capturas: o estado e as linhas do cartao, sem player nem video.
typedef struct {
  char titulo[160], sub[160], addon[64], resolucao[32], codecHdr[64], taxa[24], audio[96], legenda[64];
  int plataforma;   // PtvPlataforma
} EvTelaTeste;
void estatvivo_teste(const EvEstado *e, const EvTelaTeste *t);
#endif

#endif
