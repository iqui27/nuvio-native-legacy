// TRAILER DENTRO DO APP (Samsung). Pedido do dono, 20/09/2026, e o #82 do
// rawldon: "when I open a trailer it takes me into a browser view and I
// can't get back". No Tizen o app e uma pagina, e o player do YouTube existe
// como <iframe> embed com a IFrame API — controlavel por postMessage e
// posicionavel ATRAS do canvas do app. O canvas abre um furo (gfx_furo, o
// mesmo do AVPlay) onde o trailer deve aparecer, e o texto do app continua
// por cima. Nao ha download nem player proprio: e o embed oficial.
//
// DOIS USOS:
//   autoplay   mudo, na area do fundo da pagina de titulo, depois de a
//              pagina assentar (ajuste "Trailer automático");
//   tela cheia com som, pelo botao de trailer — OK pausa/continua, Voltar
//              fecha. Antes o botao abria o navegador da TV.
//
// LG: nao ha pagina nem iframe; trailer_suportado() e 0 e o botao continua
// abrindo o navegador do webOS (extras_trailer_abrir). Trazer o trailer para
// dentro na LG exige um resolvedor de URL do YouTube fora do aparelho.
#ifndef NV_TRAILER_H
#define NV_TRAILER_H
#include <SDL2/SDL.h>
#include "gfx.h"

int  trailer_suportado(void);
// Abre (ou reposiciona) o embed do video `yt` no retangulo `r` (coordenadas
// da tela 1920x1080). `som` 0 = mudo (autoplay). `cheia` marca o modo de
// tela cheia com teclado proprio.
void trailer_abrir(const char *yt, GfxRect r, int som, int cheia);
void trailer_rect(GfxRect r);
void trailer_fechar(void);
int  trailer_aberto(void);
int  trailer_cheia(void);
// 1 quando ha video de fato tocando (o iframe carregou e disse PLAYING):
// so ai a pagina abre o furo — antes disso mostrar um buraco preto seria
// pior que a arte.
int  trailer_tocando(void);
// Retangulo atual, para quem desenha o furo.
GfxRect trailer_retangulo(void);
// Teclado do modo de tela cheia. 1 quando consumiu.
int  trailer_evento(const SDL_Event *e);
void trailer_atualizar(Uint32 agora);

#endif
