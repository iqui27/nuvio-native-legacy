// O CARTAO DAS ESTATISTICAS AO VIVO no player (mockup stats-velocidade, telas
// 2 e 3, aprovado em 07/10). Abre pelo atalho "Estatisticas" da Central de
// controle; a conta e a origem de cada numero sao de estatvivo.h.
//
// Tres tamanhos: Completo (todas as medidas), Compacto (as principais) e Minimo
// (uma linha fina). O painel usa o vidro e as medidas das laterais do player e
// shares the clock's top/inset and stays fixed at the selected size and side (Ajustes,
// "Lado das estatisticas"). Modal: OK troca o tamanho; Esquerda/Direita trocam o
// side; DOWN pins and returns focus to the player. UP focuses a pinned panel;
// OK resizes, LEFT/RIGHT change side, UP unpins, Back/DOWN return to the player.
// Back closes an unpinned panel; the Central also unpins.
// O tamanho de abertura e o dos Ajustes (ou o ultimo usado). Tudo anda na mola
// (anim.h): abrir/fechar, trocar de tamanho e de lado e os numeros. A amostra corre 1x/s
// enquanto houver video no player, aberto ou nao — e o que da o pico, a media e
// as reconexoes "desde que o filme abriu".
//
// CUSTO NO PLAYER (TCL/Mali sao limitados pela GPU): retangulos solidos, sem
// desfoque nem sombra grande; os textos mudam 1x/s e moram no cache de texto.
#ifndef NV_ESTATVIVOUI_H
#define NV_ESTATVIVOUI_H
#include <SDL2/SDL.h>
#include "estatvivo.h"
#ifdef ESTATVIVO_TESTE
#include "gfx.h"
#endif

enum { EV_TAM_COMPLETO, EV_TAM_COMPACTO, EV_TAM_MINIMO, EV_TAM_N };

void estatvivo_abrir(void);
void estatvivo_fechar(void);
int  estatvivo_aberto(void);       // panel owns focus (modal)
int  estatvivo_fixado(void);       // pinned; UP can focus it without unpinning
int  estatvivo_tamanho(void);      // EV_TAM_*
// Rectangle (x y w h) in the virtual canvas for each size and side: fixed dimensions
// das laterais do player, FIXAS por tamanho (nao dependem de numero nenhum).
void estatvivo_retangulo(int tam, int esquerda, float *g);
void estatvivo_retangulo_atual(float *g);   // do tamanho e do lado de agora
// Teclado do cartao aberto. `cimaDela` = outra camada/foco usa CIMA neste
// quadro (folha, avisos, overlay do guia, barra de tempo, "Pular abertura"):
// o CIMA que focaria o painel fixado fica dela. 1 = consumiu.
int  estatvivo_evento(const SDL_Event *e, int cimaDela);
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
void estatvivo_teste_layout(float *painel, float *dicas);
int estatvivo_teste_tiles(int *ids);
typedef struct {
  GfxRect header, meta, tiles[6], keys[5], separator;
  int nTiles, nKeys;
  float hintTextH, bodyTextH;
  char dicas[256];   // the hint row text, as drawn
  // Every text box drawn this frame and the card (or panel) that owns it;
  // `cortes` counts single lines cut with an ellipsis.
  GfxRect textos[96], donos[96];
  int nTextos, cortes;
  float escala;      // gfx scale the panel was drawn at
} EvGeometriaTeste;
void estatvivo_teste_geometria(EvGeometriaTeste *g);
#endif

#endif
