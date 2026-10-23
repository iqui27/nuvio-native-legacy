// SEGURAR OK NUM TITULO DE UMA LISTA/GRADE = O MENU DO CARTAZ (ctxmenu.h).
//
// Na home o gesto ja existia (home.c). Faltava em toda tela que abre uma lista
// de titulos: "Ver tudo", pagina de colecao, filmografia, lista da saga,
// recomendacoes do detalhe, resultados da Busca e do Spotlight (dono, 06/10:
// "nao tem o menu contextual quando abre uma lista"). Cada tela repetir a
// mecanica do KEYDOWN/KEYUP/limiar seria a quarta copia do mesmo defeito, entao
// ela mora aqui: a tela diz so (1) se a celula em foco aceita o menu e (2) qual
// titulo/retangulo abrir.
//
// PROTOCOLO (o mesmo da home, de Salvos e da Biblioteca):
//   KEYDOWN de OK arma o relogio e NAO age; KEYUP curto = toque (a tela faz o
//   que o OK fazia no KEYDOWN); segurar NV_HOLD_MS abre o menu com o dedo ainda
//   no botao (ctxmenu.c ignora o OK afundado), e o KEYUP seguinte vai ao menu.
//   Qualquer outra tecla cancela. Enquanto o dedo segura (depois de 150 ms, que
//   sao toque) a ILHA mostra "Segure para opcoes" com a barra enchendo, como no
//   detalhe.
//
// USO NA TELA:
//   static CtxHold hold;
//   evento:     switch (ctxhold_evento(&hold, e, celulaAceita)) {
//                 case CTXH_CONSUMIDO: return;
//                 case CTXH_TOQUE:     <acao do OK>; return;
//                 default: break; }
//   atualizar:  if (ctxhold_passo(&hold, agora, 1)) { if (abrir()) ctxhold_cancelar(&hold); }
// Se `abrir` falhar (titulo sem indice no catalogo) a marca fica e o KEYUP
// vira toque: o gesto nunca some sem resposta.
#ifndef NV_CTXLISTA_H
#define NV_CTXLISTA_H
#include <SDL2/SDL.h>
#include "gfx.h"

typedef struct {
  Uint32 desde;   // SDL_GetTicks() do KEYDOWN
  int armado;     // OK afundado numa celula que aceita o menu
  int longo;      // limiar ja cruzado (a tela tentou abrir)
} CtxHold;

enum { CTXH_NADA = 0, CTXH_CONSUMIDO = 1, CTXH_TOQUE = 2, CTXH_LONGO = 3 };

// Alimente com TODO evento de teclado da tela. `celulaAceita` = a celula em
// foco pode abrir o menu agora. Nao-OK e KEYDOWN cancelam. Devolve
//   CTXH_CONSUMIDO  o evento era do gesto (armar, repeticao, soltura apos o
//                   menu): a tela nao faz mais nada com ele;
//   CTXH_TOQUE      OK soltou antes do limiar: faca a acao normal do OK;
//   CTXH_LONGO      dedo confirmou pressao longa: abra o menu da celula;
//   CTXH_NADA       nao e do gesto (ou a celula nao aceita o menu: a tela
//                   segue o caminho de antes, inclusive o OK no KEYDOWN).
int  ctxhold_evento(CtxHold *h, const SDL_Event *e, int celulaAceita);
// Por quadro. 1 UMA vez quando cruza NV_HOLD_MS (a tela abre o menu). Com
// `feedback`, mostra a barra na ilha enquanto o dedo segura.
int  ctxhold_passo(CtxHold *h, Uint32 agora, int feedback);
void ctxhold_cancelar(CtxHold *h);
// 0..1, para teste e para quem desenha a propria barra.
float ctxhold_progresso(const CtxHold *h, Uint32 agora);

// Indice no catalogo global do titulo pelo IMDb (com ou sem ":temp:ep") ou
// pelo id do TMDB; -1 se o catalogo ainda nao o tem.
int  ctxlista_indice(const char *imdb, long tmdb);
// Abre o menu do cartaz para `idx` ao lado do retangulo `r` (tela virtual
// 1920x1080) com a `arte` dele. 1 se o menu abriu.
int  ctxlista_abrir(int idx, GfxRect r, const char *arte);

// TITULO FORA DO CATALOGO (credito de ator, parte da saga, recomendacao): pede o
// meta pelo mesmo caminho de abrir um titulo (desc_pedir_titulo_semente) e,
// quando ele chega, o roteador (app.c) o entrega a ctxlista_tomar, que abre o
// MENU em vez da pagina. 1 se o pedido saiu (o menu vem depois); 0 se a
// descoberta esta ocupada ou nao ha como pedir — a tela faz o toque normal.
int  ctxlista_pedir(const char *imdb, long tmdb, const char *tipo,
                    const char *titulo, const char *ano, const char *poster,
                    GfxRect r, const char *arte);
// app.c, quando desc_titulo_pronto() devolve `idx`: 1 se o pedido era de um
// menu (ele abriu; nao abra a pagina), 0 se nao ha pedido vivo.
int  ctxlista_tomar(int idx);
#endif
