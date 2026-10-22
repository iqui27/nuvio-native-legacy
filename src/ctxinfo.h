// A EXTENSAO DE INFORMACOES DO MENU DO CARTAZ (dono, 06/10/2026: "ao inves de
// abrir a pagina de detalhes, vamos fazer uma extensao com informacoes uteis,
// notas etc, bem bonito ao lado"). O "Ver detalhes" saiu do menu — o toque ja
// abre o titulo — e no lugar dele, colada ao menu, uma segunda ilha do mesmo
// vidro com o que se quer saber antes de decidir:
//
//   - a arte do titulo com o logo (ou o nome), ano · tipo · duracao, generos
//     e a classificacao etaria;
//   - a linha de notas da pagina de titulo (notasui.c: IMDb, Rotten Tomatoes,
//     Trakt... as fontes que a pessoa escolheu em Ajustes);
//   - a sinopse em ate quatro linhas;
//   - onde a pessoa parou (serie: "T2E5 · 24 min restantes"), os amigos que
//     viram/gostaram (amigostitulo.h), a agenda da serie (agenda.h) e os selos
//     "Salvo"/"Assistido".
//
// NADA BLOQUEIA O MENU: o que o titulo ja traz aparece no primeiro quadro; o
// que falta (notas, duracao, sinopse) vem do resumo de extras.h, pedido em
// fundo, e entra quando chega. Sem dado, a linha some — nada de reserva.
//
// CUSTO: nenhuma passada de borrao nova (o vidro le a textura ja assada), o
// texto e o de txt_linha (cacheado) e a sinopse e quebrada UMA vez por titulo
// e largura, nao a cada quadro.
#ifndef NV_CTXINFO_H
#define NV_CTXINFO_H
#include <stddef.h>
#include "catalogo.h"
#include "gfx.h"

#define CTXI_W      500.0f   // largura de sempre
#define CTXI_W_MIN  380.0f   // menos que isto, colada ao menu, nao cabe
#define CTXI_W_SOLTA 320.0f  // separada do menu (do outro lado do cartaz)
#define CTXI_GAP     12.0f   // entre o menu e a extensao

// ONDE FICAM O MENU E A EXTENSAO. Funcao pura (testada em tests/ctxinfo.c).
//   `cartaz`      a caixa do cartaz focado (tela virtual) ou NULL;
//   `menuXPadrao` onde o menu ficaria sozinho (ctxXCartaz, ou centrado);
//   `centroX`     sem cartaz: o centro pedido (tela ou painel de Salvos).
// Com cartaz: menu e extensao juntos a DIREITA do cartaz quando cabem, senao
// juntos a ESQUERDA; apertado, a extensao encolhe ate CTXI_W_MIN; sem lugar nem
// assim, o menu fica onde ficaria e a extensao vai para o outro lado do cartaz
// (`separada`). Sem cartaz: o grupo centrado na tela, ou o menu no centro
// pedido e a extensao do lado com mais espaco. `lado` +1 = a extensao a
// direita do menu, -1 = a esquerda; infoW 0 = nao ha lugar para ela.
typedef struct { float menuX, infoX, infoW; int lado, separada; } CtxInfoGeo;
void ctxinfo_geometria(const GfxRect *cartaz, float menuXPadrao, float centroX,
                       float menuW, CtxInfoGeo *g);

// O CARTAZ VIRA O CARTAO (dono, 06/10/2026, segunda rodada): com um cartaz
// focado nao ha segunda ilha nem painel solto — o PROPRIO cartaz cresce no
// lugar dele ate o cartao de informacoes (CTXI_CARTAO_W x altura do conteudo) e
// o menu fica colado ao lado. Funcao pura (testada em tests/ctxinfo_shot.c).
//   `poster`  a caixa do cartaz (tela virtual); `h` a altura do cartao (nunca
//             menor que a do cartaz); `menuW` a largura do menu.
// O cartao cresce a partir da borda ESQUERDA do cartaz e o menu vai a direita
// (lado = +1); sem lugar a direita, cresce a partir da borda DIREITA e o menu
// vai a esquerda (lado = -1); se nenhum dos dois cabe, o grupo e preso na tela.
// Verticalmente o cartao nasce no topo do cartaz e e preso entre as margens.
#define CTXI_CARTAO_W 500.0f
typedef struct { GfxRect cartao; float menuX; int lado; } CtxCartaoGeo;
void ctxinfo_cartao_geo(const GfxRect *poster, float h, float menuW, CtxCartaoGeo *g);
// A mesma, com a largura do cartao pedida (cartao grande: a do proprio cartaz).
void ctxinfo_cartao_geo_w(const GfxRect *poster, float w, float h, float menuW, CtxCartaoGeo *g);

// O titulo do menu mudou (ou abriu): pede o resumo em fundo quando falta algo.
void ctxinfo_abrir(const CatItem *ci);

// O estado de quem o menu conhece (salvo / historico) entra por aqui.
typedef struct { int salvo, visto; } CtxInfoEstado;

// Altura do conteudo para a largura `w` (sem desenhar).
float ctxinfo_altura(const CatItem *ci, const CtxInfoEstado *st, float w);
// Desenha o conteudo na caixa (x, y, w): `a` e o alfa da superficie,
// `ca` o do conteudo (entra um pouco depois). A superficie (ilha) e de quem
// chama. Devolve a altura usada.
float ctxinfo_desenhar(const CatItem *ci, const CtxInfoEstado *st,
                       float x, float y, float w, float a, float ca);

// O QUE A EXTENSAO MOSTRA, em texto, uma linha por bloco ("meta: ...",
// "notes: 1=84 0=780" (ExFonte=cru), "synopsis: ...", "progress: ...",
// "friends: ...", "schedule: ...", "badges: saved watched"). Para os testes;
// os rotulos sao em ingles para nao parecerem texto de tela.
void ctxinfo_texto(const CatItem *ci, const CtxInfoEstado *st, char *dst, size_t cap);
// Versao compacta (linha expandida do painel de Salvos): meta, notas e sinopse
// em duas linhas, em (x, y) com largura w. Devolve a altura; `desenhar` 0 so mede.
float ctxinfo_compacto(const CatItem *ci, const CtxInfoEstado *st, float x, float y,
                       float w, float ca, int desenhar);
// CARTOES GRANDES (paisagem): sem cartao novo — a informacao vai SOBRE a arte do
// cartao (degrade na base, logo, meta, notas, sinopse em 2 linhas). `r` e o
// retangulo do cartao; ctxinfo_sobre_altura e a altura que isso pede.
float ctxinfo_sobre_altura(const CatItem *ci, const CtxInfoEstado *st, float w);
void  ctxinfo_sobre_arte(const CatItem *ci, const CtxInfoEstado *st, GfxRect r, float raioPx, float ca);
#endif
