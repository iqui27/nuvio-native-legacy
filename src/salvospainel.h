// PAINEL "SALVOS": a camada que desliza da direita na tecla AZUL.
//
// Ele SUBSTITUI o painel "Sua atividade" que perfil.c desenhava no mesmo lugar
// (perfil_abrir_lateral / perfil_lateral, removidos). A troca foi pedida assim:
// o atalho mais rapido do controle passa a responder "o que eu guardei para
// ver?" em vez de "quantos minutos assisti este mes?" — a primeira e uma
// pergunta que leva a uma acao, a segunda e um relatorio, e relatorio tem tela
// propria (MENU_PERFIL, agora abrindo TELA_PERFIL direto).
//
// Ele nao e uma tela: e uma camada por cima do conteudo, e enquanto esta
// visivel toma o D-pad. Mesma disciplina de menu.h — sem encerrar (usa os
// caches de text.c/tex_cache.c) e sem quer_sair (fechar o painel nunca fecha o
// app).
#ifndef NV_SALVOSPAINEL_H
#define NV_SALVOSPAINEL_H
#include <SDL2/SDL.h>

void spainel_abrir(void);
void spainel_fechar(void);
// 1 enquanto a camada e dona do D-pad. Cai para 0 no instante da escolha, com a
// animacao de saida ainda rodando — e esse o sinal para o conteudo voltar a
// responder as teclas, senao o D-pad fica morto durante o recolhimento (a mesma
// regra de menu_aberto).
int  spainel_aberto(void);
// 1 enquanto ainda ha pixel para desenhar, incluindo a saida.
int  spainel_visivel(void);

void spainel_evento(const SDL_Event *e);
void spainel_atualizar(float dt, Uint32 agora);
void spainel_desenhar(Uint32 agora);

// IMDb do titulo que o dono escolheu, ou NULL. Consumido uma vez. Quem resolve
// o id no catalogo e abre o detalhe e o roteador (app.c) — o painel nao conhece
// nem detail.c nem a descoberta, exatamente como perfil.c nao conhecia.
const char *spainel_pediu_abrir(void);

#endif
