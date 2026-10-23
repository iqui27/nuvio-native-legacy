// PECAS DE DESENHO DO SOCIAL, comuns a fileira da home (amigosfil.c), ao painel
// (salvospainel.c, abas Atividade e Amigos) e ao perfil (amigoperfil.c).
//
// TUDO AQUI E BARATO POR QUADRO: cor de texto fixa (a cor entra na chave do
// cache de text.c — ver a nota em salvospainel.c, desenhaAbas) e o foco faz
// crossfade de alfa entre duas linhas ja rasterizadas. O unico "movimento"
// proprio e o ponto ao vivo, que pulsa so no alfa de um disco.
#ifndef NV_SVDESENHO_H
#define NV_SVDESENHO_H
#include "gfx.h"
#include "text.h"
#include "socialvis.h"
#include "amigostitulo.h"
#include <SDL2/SDL.h>

// As cores de estado. A novidade e LARANJA fixo e o ao vivo VERDE fixo, e
// nao a cor de realce: o realce e o FOCO, e com o tema padrao (branco) um anel
// de novidade na cor de realce seria o anel de foco.
#define SVD_NOVO_R 0.949f
#define SVD_NOVO_G 0.635f
#define SVD_NOVO_B 0.361f
#define SVD_VIVO_R 0.314f
#define SVD_VIVO_G 0.827f
#define SVD_VIVO_B 0.490f

// A superficie de linha/cartao do painel: neutra em repouso, realce cheio no
// foco (o degrade do tema dinamico vem sozinho de gfx_rect), vidro quando o
// vidro esta ligado. A mesma receita de salvospainel.c (superficieItem).
void  svd_superficie(GfxRect r, float raio, float f, float a);
float svd_foco_visual(float f);
// Quanto o TEXTO inverte para a tinta do realce (0 no vidro: nao inverte).
float svd_foco_texto(float f);
void  svd_txt_foco(TxtLinha repouso, TxtLinha foco, float x, float y, float f, float a);

// O ponto verde de "assistindo agora", pulsando no alfa (parado com
// animacoes reduzidas). `borda` > 0 desenha o aro escuro por fora.
void  svd_ponto_vivo(float cx, float cy, float d, float borda, float a, Uint32 t);

// O ROSTO: foto (ou inicial no disco colorido), anel de estado (laranja =
// novidade, verde = ao vivo, cinza = nada) e, com foco, o anel de foco na
// cor de realce. `r` e o quadrado da FOTO; os aneis ficam por fora dele.
void  svd_rosto(GfxRect r, const SvAmigo *am, float foco, float a, Uint32 t);
// Um rosto de acao ("+ Adicionar"): disco neutro com o icone.
void  svd_rosto_acao(GfxRect r, const char *icone, float foco, float a);

// SO A FOTO (ou a inicial no disco colorido), sem anel de estado. `r` e o
// quadrado/disco. O mesmo desenho dos rostos da fileira "Amigos assistindo".
void  svd_avatar(GfxRect r, const char *url, const char *nome, const char *id, float a);
// AMIGOS DE UM TITULO (amigostitulo.h). Pilha de ate `n` rostos de diametro `d`
// sobrepostos, comecando em (x, y) topo; `anel` e a cor do aro que separa um
// rosto do outro (a do fundo sobre o qual a pilha esta). `selos` = 1 poe o
// coracao (gostou) ou o check (viu) no canto de baixo de cada rosto. Devolve a
// largura gasta.
float svd_amigos_pilha(float x, float y, float d, const AmigosTitulo *t, int n,
                       int selos, const float anel[3], float a);
// O chip de vidro no canto de cima do cartaz: ate 2 rostos + "+N". `maxW` limita
// a largura (o selo "Assistido" fica do outro lado): se nao cabe, 1 rosto, ou so
// o numero. Devolve a largura gasta (0 = nao coube nem assim).
float svd_amigos_chip(float x, float y, float h, float maxW, const AmigosTitulo *t, float a);

// Barra fina de progresso: trilho e preenchimento na cor de realce.
void  svd_barra(GfxRect r, int pct, float a);

// O CARTAO DEITADO (E1): arte 16:9, veu embaixo, titulo, status e barra.
// `foco` acende o aro de foco da home por fora.
// Home card focus ring (accent outline / glass), shared by every home-style card.
void  svd_aro_foco(GfxRect r, float raio, float f, float a);
void  svd_cartao(GfxRect r, const SvEvento *ev, float foco, float a, Uint32 t);
// Cartaz 2:3 (ou arte) com o esqueleto enquanto nao chega.
void  svd_poster(GfxRect r, const char *url, float raio, float a);
int   svd_arte_card(GfxRect r, const char *url, float raio, float a);

// UMA PILULA DA CADEIA ("Voce mandou", "viu", "gostou") e o separador. Devolve
// a largura gasta. estilo: 0 neutro, 1 feito (verde), 2 gostou (realce quente).
float svd_chip(float x, float y, const char *texto, int estilo, int escuro, float a);
float svd_seta(float x, float y, int escuro, float a);
#define SVD_CHIP_H 30.0f

#endif
