// TELA "VER TUDO": um catalogo inteiro em grade, aberto pelo card no fim da
// fileira da home.
//
// Existe porque a fileira mostra 12 itens e o catalogo tem centenas — no web
// isso e o `catalogSeeAllScreen`, aberto pelo card `openCatalogSeeAll`. Sem ela
// o resto de cada catalogo era inalcancavel: nao havia NENHUM caminho no app
// para ver o item 13 de uma fileira.
//
// A grade e de 5 colunas em 1920, medida do web (.seeall-card 248px de largura,
// e o corpo reserva 368px do painel lateral). O painel de detalhe a direita que
// o web tem NAO entra nesta versao — ele repete o que a tela de titulo ja
// mostra, e a grade sozinha ja resolve o que faltava.
#ifndef NV_VERTUDO_H
#define NV_VERTUDO_H
#include <SDL2/SDL.h>
#include "colecoes.h"
void vertudo_colecao(const ColFolder *folder);

// Abre com o catalogo de uma fileira da home.
void vertudo_abrir(const char *base, const char *tipo, const char *catId,
                   const char *titulo);
int  vertudo_aberta(void);
// Some sem animar (o player saiu para a home: ilha_minimizar).
void vertudo_fechar_seco(void);
void vertudo_evento(const SDL_Event *e);
void vertudo_atualizar(float dt, Uint32 agora);
void vertudo_desenhar(Uint32 agora);
// Indice no catalogo global do titulo que o dono abriu, ou -1. Consumido uma
// vez: o roteador chama, abre o detalhe e a tela se fecha.
int  vertudo_pediu_abrir(void);
// 1 uma vez: ESQUERDA na coluna 0 pediu a barra lateral (abre por cima).
int  vertudo_pediu_menu(void);
// Posicao do cartao em foco na grade (testes: o foco volta ao mesmo cartao
// depois do menu do cartaz).
int  vertudo_foco(void);
#endif
