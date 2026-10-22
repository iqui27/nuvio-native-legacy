// A LISTA "O QUE OS AMIGOS ACHARAM" da pagina do titulo (dono, 06/10/2026):
// OK na ilha de amigos abre, por cima da pagina, uma folha de vidro com uma
// linha por amigo — a opiniao (gostou / mais ou menos / nao gostou / nota), o
// progresso numa serie (viu ate T2E5, terminou a serie, parou no T1E3), a
// recomendacao que ele me mandou e a resposta a uma rec minha. Antes o OK
// mandava a pessoa para a aba Atividade, que mostrava de novo so "viu".
//
// So mostra o que amigostitulo.h ja tem — o que o servidor entregou dentro do
// alcance de cada pessoa. Nada e pedido daqui.
//
// A pagina do titulo (detail.c) e dona do quando: abre com OK na ilha,
// entrega as teclas enquanto `amtui_aberta()` e desenha por cima de tudo.
#ifndef NV_AMIGOSTITULO_UI_H
#define NV_AMIGOSTITULO_UI_H
#include "amigostitulo.h"
#include <SDL2/SDL.h>

// Copia `t` (a lista nao muda debaixo do foco). `ultT/ultE` = ultimo
// episodio da serie, 0 = nao se sabe. `titulo` vai no topo da folha.
void amtui_abrir(const AmigosTitulo *t, int ultT, int ultE, const char *titulo);
int  amtui_aberta(void);
void amtui_fechar(void);
// CIMA/BAIXO andam; VOLTAR e OK fecham. Come toda tecla enquanto aberta.
void amtui_evento(const SDL_Event *e);
void amtui_desenhar(Uint32 agora);
// TESTE (#99): a linha em foco; -1 com a folha fechada.
int  amtui_teste_foco(void);
#endif
