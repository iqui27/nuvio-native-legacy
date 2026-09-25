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
//   - OVERLAY (o "mini guia", desde 25/09/2026): guia_overlay_abrir(), uma
//     FAIXA no rodape por cima do video em tela cheia — o canal no ar e dois
//     de cada lado, agora/a seguir. OK em outro canal troca; Azul abre o guia
//     completo com o canal encolhendo para o preview; some sozinha em 6 s.
//
// PAINEL DE CATEGORIAS: segurar CIMA ou BAIXO (~1,1 s) ou o chip
// "Categorias" do cabecalho abre uma gaveta com as secoes; nos 768 canais do
// FrostView, descer linha a linha ate "Canais Sportv" seria minutos de D-pad.
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

// --- o preview e o canal no ar (25/09/2026) --------------------------------
// O preview do guia e uma sessao "mini no guia" do player (player.h). O guia
// so PEDE; o app executa, porque abrir sessao e buscar fonte e dele:
//   guia_pediu_preview        tocar este canal no preview (player_mini_no_guia
//                             + player_manter_mini + o tocarCanal de sempre);
//   guia_pediu_parar_preview  fechar a sessao do preview (saiu do guia,
//                             desligou o preview);
//   guia_pediu_restaurar      OK no canal que ja toca: tela cheia, mesmo fluxo;
//   guia_pediu_guia_cheio     Azul na faixa: guia completo com o canal no ar.
// guia_adotar_canal: o canal no ar veio para o preview (encolhido pelo app);
// o guia foca a linha dele e deixa de seguir o foco ate o proximo OK.
int  guia_pediu_preview(CatItem *it);
int  guia_pediu_parar_preview(void);
int  guia_pediu_restaurar(void);
int  guia_pediu_guia_cheio(void);
void guia_adotar_canal(const char *id);
void guia_preview_rect(float *x, float *y, float *w, float *h);
// O CatItem do canal de um lembrete (e a origem para a busca de fonte): o do
// guia quando a lista esta carregada, senao so id + nome guardados. 0 sem id.
int  guia_item_do_canal(const char *id, const char *nome, const char *base, CatItem *it);

#endif
