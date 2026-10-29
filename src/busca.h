// Tela de Busca, no formato do app da Apple TV: teclado na tela a esquerda,
// grade de posteres a direita filtrando enquanto se digita.
//
// A tela existe porque num aparelho de TV nao ha teclado fisico: TODA a entrada
// de texto passa pelo D-pad, e por isso a escolha da forma do teclado (grade
// contra a linha unica do tvOS) muda mais a experiencia do que qualquer detalhe
// visual. A justificativa da escolha esta no topo de busca.c.
#ifndef NV_BUSCA_H
#define NV_BUSCA_H
#include <SDL2/SDL.h>
#include "home.h"
#include "spotlight.h"

int  busca_iniciar(void);
void busca_evento(const SDL_Event *e);
void busca_atualizar(float dt, Uint32 agora);
void busca_desenhar(Uint32 agora);
// 1 quando o Back deve fechar a tela. O Back so chega aqui depois de esgotar o
// que ele tem para desfazer DENTRO da busca (sair dos resultados de volta para
// o teclado); fechar direto do meio dos resultados perde o texto digitado sem
// que o usuario tenha pedido isso.
int  busca_quer_sair(void);
int  busca_pediu_relogio(void);
int  busca_relogio_navegavel(void);
void busca_encerrar(void);

// OK pressionado sobre um poster: devolve 1 UMA vez e escreve em
// `indiceCatalogo` o indice em cat_item(). Consome o pedido, como
// home_pediu_abrir.
int  busca_pediu_abrir(int *indiceCatalogo);

// OK sobre uma PESSOA da fileira de Pessoas: devolve 1 UMA vez e escreve o mesmo
// pedido que o Spotlight devolve (SPOT_PESSOA), para o app abrir a filmografia
// pelo mesmo caminho.
int  busca_pediu_pessoa(SpotPedido *p);

// Poster em foco com o retangulo que ele ocupa na tela NESTE quadro, no mesmo
// formato que a home entrega — e o que detail_abrir precisa para o card voar
// dali em vez de aparecer do nada. 0 se ainda nao houve um quadro desenhado com
// foco nos resultados.
int  busca_item_focado(HomeItem *out);

// Preenche o campo e busca (deeplink nuvio-legacy://search?q=).
void busca_definir_consulta(const char *t);

// O texto do campo (UTF-8), como a pessoa o digitou. Para testes.
const char *busca_consulta(void);
// Foco na barra: 0 teclado, 1 campo, 2 Falar, 3 celular. Para testes.
int  busca_foco_campo(void);
#ifdef NV_TESTE
void busca_teste_layout_recentes(void);
#endif

#endif
