// FUNDO ATRAS DOS PAINEIS (Ajustes › Aparência › Fundo, Ajustes v2).
//
// Um modulo so para quem pinta o pano de fundo de uma tela inteira com a arte
// do titulo: Ajustes (e as telas que usam ajustes_ui_fundo) e, depois do
// merge, a pagina do titulo (detail.c). O modo e a escolha LOCAL desta TV:
//   FUNDO_ARTE    (0, padrao) a arte nitida em "cover" com o veu escuro do
//                 mockup (linear 90deg: 62%, 40% no meio, 50%), em degrade
//                 pelo nv_dither;
//   FUNDO_BORRADA (1) a arte DESFOCADA: a copia de 96x54 de gfx_desfocado
//                 assada em "cover" num quadro de 320x180 e ampliada, com um
//                 veu de 28%. Nenhum desfoque por quadro; o assado so refaz
//                 quando a arte muda. Sem a arte ainda, o fundo liso;
//   FUNDO_FROST   (2) sem arte: cinza frio com tres luzes largas na cor de
//                 ajustes_acento() (58%, 38%, 14%), tudo por GFX_VEU_CSS e
//                 GFX_LUZ (nv_dither: painel de 8 bits nao faz faixa).
#ifndef NV_FUNDO_H
#define NV_FUNDO_H
#include "gfx.h"

#define FUNDO_ARTE    0
#define FUNDO_BORRADA 1
#define FUNDO_FROST   2

// O modo escolhido nesta TV (ajustes_fundo).
int  fundo_modo(void);
// Pinta o fundo no modo escolhido em `area` (coordenadas de layout; a tela
// inteira e o caso normal), com a arte `arteUrl` (a mesma chave do cache de
// texturas; NULL ou "" = sem arte, so a cor de fundo e o veu) e alfa `a`.
void fundo_desenhar(GfxRect area, const char *arteUrl, float a);
// O mesmo, num modo pedido (as previas de Ajustes › Fundo). `raioPx` arredonda
// os cantos (0 na tela cheia); fora da tela cheia a Borrada estica a copia
// desfocada no retangulo, sem o assado, que e de tela inteira.
void fundo_desenhar_modo(int modo, GfxRect area, float raioPx, const char *arteUrl, float a);


// O veu escuro da Borrada de tela cheia nos proximos desenhos (0..1; < 0 volta
// aos 28% de sempre). A pagina do titulo usa mais: o texto fica sobre ela.
void fundo_borrada_veu(float forca);

// A conferencia de uma vez do fundo de tela cheia `modo` (FUNDO_BORRADA ou
// FUNDO_FROST, fundo.c): -1 ainda nao, 1 o pixel da tela bateu com a conta do
// CPU, 0 errou (o fundo segue no desenho direto pela sessao).
int fundo_conferencia(int modo);

#endif
