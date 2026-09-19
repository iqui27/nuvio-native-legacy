// GUIA DE TV — a lista de canais organizada como guia, nao como grade de
// cartazes.
//
// O QUE E: linhas por categoria (a ordem em que o addon as declara), cada uma
// com os canais daquela categoria; em cada card, o programa QUE ESTA NO AR e o
// proximo, vindos da grade EPG (epg.c); um painel a direita com a descricao do
// canal e os tres programas seguintes. A primeira linha e "Favoritos", montada
// dos canais marcados — e a parte "personalizada" do pedido.
//
// DOIS MODOS, um modulo so:
//   - TELA: guia_abrir(), fundo opaco, voltar sai para a home.
//   - OVERLAY: guia_overlay_abrir(), por cima do player — e o "aperta para
//     baixo / botao azul enquanto o canal toca". Mesma navegacao, mesmo
//     conteudo; OK troca o canal sem sair do player.
//
// MODO SALTA-CATEGORIA: segurar CIMA ou BAIXO por ~600 ms troca a navegacao de
// "linha a linha" para "categoria a categoria" — nos 768 canais do FrostView,
// descer linha a linha ate "Canais Sportv" seria minutos de D-pad. Parou de
// apertar por 2 s, volta ao normal sozinho. O modo aparece na tela: os
// cabecalhos acendem e uma coluna de categorias sobe na esquerda.
#ifndef NV_GUIA_H
#define NV_GUIA_H
#include <SDL2/SDL.h>
#include "catalogo.h"

void guia_abrir(void);           // tela cheia
void guia_overlay_abrir(void);   // por cima do player (video continua atras)
int  guia_aberta(void);          // tela cheia em pe
int  guia_overlay_aberta(void);  // overlay em pe
int  guia_visivel(void);         // qualquer um dos dois

// Foca o canal deste id se ele existir na lista. Usado pela home ("ver tudo"
// da fileira de canais ja abre no canal focado) e pelo player (o overlay abre
// no canal que esta tocando).
void guia_focar_id(const char *id);

// Dispara a carga de canais sem abrir nada — o CH+/- do controle funciona
// mesmo com o guia nunca aberto, e precisa da lista para saber quem vem depois.
void guia_carregar(void);

// ZAP: o canal `dir` posicoes depois (1) ou antes (-1) de `idAtual` na ordem do
// guia, ja como CatItem pronto para tocar. 0 = lista ainda nao carregada —
// chame guia_carregar e tente de novo na proxima tecla.
int  guia_zap(const char *idAtual, int dir, CatItem *saida);

void guia_evento(const SDL_Event *e);
void guia_atualizar(float dt, Uint32 agora);
void guia_desenhar(Uint32 agora);

// 1 uma unica vez quando o usuario pediu para sair da tela cheia.
int  guia_quer_sair(void);

// 1 uma unica vez quando o OK escolheu um canal: `saida` recebe o CatItem
// pronto para cat_acrescentar/player_abrir (id completo, tipo "channel").
int  guia_pediu_canal(CatItem *saida);
// Base do addon que publicou o canal recem-pedido ("" para portal Stalker).
// app.c passa a addons_definir_origem antes de buscar a fonte.
const char *guia_canal_origem(void);

// O id do canal em foco, ou "" — o player usa para saber se o overlay esta
// apontando para o canal que esta no ar.
const char *guia_id_focado(void);

#endif
