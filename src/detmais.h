// "MAIS OPCOES" DA PAGINA DO TITULO (dono, 06/10/2026: "muitos botoes na
// pagina de titulos. Vamos deixar o Trailer, Explorar e Trocar background em
// outro lugar, mas de facil acesso").
//
// A linha de acoes do detalhe chegava a DEZ pecas num filme (Reproduzir, +,
// olho, fontes, recomendar, trocar arte, trailer, explorar...). Agora ela
// guarda so o que se usa a cada visita — reproduzir/retomar, salvar, assistido,
// fontes (e o lembrete da serie) — e o resto mora num circular "..." no fim
// dela. O OK nele ABRE ESTA ILHA ancorada no proprio botao: o circular cresce
// ate virar o cartao (mesmo material e mesma linha do menu do cartaz, ctxmenu.c
// e plrui.h), com icone + nome em cada linha.
//
// POR QUE UM POPOVER E NAO "SEGURAR OK NO PLAY". O hold no primario ja e a
// folha de fontes, e um gesto escondido nao e descobrivel. O "..." esta a duas
// teclas (DIREITA ate ele, OK) e a terceira escolhe; o nome "Mais opcoes" sobe
// numa dica quando ele recebe foco, como o "Assistir trailer" fazia.
//
// Este modulo so sabe a lista, o foco, a animacao e o desenho. QUEM EXECUTA a
// acao e detail.c: detmais_evento devolve qual foi escolhida.
#ifndef NV_DETMAIS_H
#define NV_DETMAIS_H
#include <SDL2/SDL.h>
#include "gfx.h"

// Ordem FIXA das linhas (a do uso: o trailer e o que mais se procura).
enum { DMAIS_TRAILER = 0, DMAIS_EXPLORAR, DMAIS_ARTE, DMAIS_RECOMENDAR, DMAIS_N };

// Abre com as acoes que existem neste titulo (`disp[i]` != 0), foco na
// primeira. Sem nenhuma disponivel, nao abre.
void detmais_abrir(const int disp[DMAIS_N]);
void detmais_fechar(void);
// Fecha sem animar (a pagina abriu outro titulo).
void detmais_zerar(void);
int  detmais_aberto(void);      // tem o teclado
float detmais_visivel(void);    // 0..1, segue animando depois de fechar
int  detmais_n(void);
int  detmais_acao(int linha);   // DMAIS_* da linha, -1 fora
int  detmais_foco(void);

// Come TODO evento enquanto aberta. CIMA/BAIXO andam, VOLTAR/ESQUERDA fecham,
// OK (descer e soltar) fecha e devolve a acao escolhida. -1 = nada a fazer.
int  detmais_evento(const SDL_Event *e);

// Desenha ancorada no circular `ancora` (o retangulo dele na tela): abaixo
// quando cabe, senao acima. Registra os alvos do ponteiro (uma camada nova).
void detmais_desenhar(GfxRect ancora, float a);

// Nome (chave i18n em portugues) e icone de cada acao.
const char *detmais_rotulo(int acao);
const char *detmais_icone(int acao);

// Retangulo final da ilha para a ancora (teste e desenho usam a mesma conta).
GfxRect detmais_caixa(GfxRect ancora);

#endif
