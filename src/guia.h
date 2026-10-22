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
#include "epg.h"
#include "gfx.h"   /* GfxRect de guia_logo_desenhar */

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
// Igual, mas so ESPIA o alvo (banner do zapping): nao mexe na origem do pedido.
int  guia_zap_ver(const char *idAtual, int dir, CatItem *saida);

// Para o OSD do canal ao vivo (aovivo.h): posicao na ordem do guia, categoria,
// favorito e o programa no ar de QUALQUER canal da lista (banner do zapping).
int  guia_info_canal(const char *id, int *numero, int *total, char *cat, size_t n);
int  guia_e_favorito(const char *id);
void guia_alternar_favorito(const char *id);
int  guia_programa_agora(const char *id, time_t t, EpgProg *p);

void guia_evento(const SDL_Event *e);
// 1 enquanto a recarga pedida pelo painel de addons de canais ainda corre com
// o painel ja fechado (a ilha mostra "Atualizando a lista de canais…").
int  guia_atualizando_lista(void);
void guia_atualizar(float dt, Uint32 agora);
void guia_desenhar(Uint32 agora);

// 1 uma unica vez quando o usuario pediu para sair da tela cheia.
int  guia_quer_sair(void);
int  guia_foco_topo(void);   // testes do ponteiro: chip em foco ou -1

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
typedef struct { char id[80]; char nome[140]; int altura; } GuiaVariante;
// O canal e as outras resolucoes dele na lista Xtream (ver guia.c); 0 = nao e Xtream.
int  guia_variantes(const char *id, GuiaVariante *out, int max);
// Canais da fileira em foco para o diagnostico da Live TV; `bases` (opcional)
// recebe o addon de cada um, `grupo` o nome da fileira.
int  guia_canais_para_teste(GuiaVariante *out, char bases[][600], int max, char *grupo, size_t ng);
int  guia_pediu_livetv_diag(void);   // VERDE no guia completo
int  guia_item_do_canal(const char *id, const char *nome, const char *base, CatItem *it);

// --- A CARA DO CANAL FORA DO GUIA (29/09/2026) -------------------------------
// O player ao vivo (aovivoui.c) desenha a marca, o selo AO VIVO e a etiqueta
// de categoria com as MESMAS funcoes do heroi do guia: a regra do dono para o
// logo (sem azulejo; recortado de um tom so vira branco, `tom` 0.965; com fundo
// proprio, cantos arredondados; sem arquivo, iniciais num azulejo escuro) vive
// num lugar so. `cx` e a caixa onde o logo fica centrado, cabendo em maxW x maxH.
void  guia_logo_desenhar(const char *logo, const char *nome, GfxRect cx,
                         float maxW, float maxH, float tom, float a);
// Pilula vermelha "AO VIVO" de 32 px; devolve a largura.
float guia_selo_ao_vivo(float x, float y, float a);
// Etiqueta translucida de 34 px (categoria); devolve a largura (0 sem texto).
float guia_etiqueta(const char *s, float x, float y, float maxW, float a);
// Descricao do canal SEM o molde do addon ("Categoria: X Qualidades: FHD, HD
// N fonte(s)"): devolve o texto que sobra fora dele, ou a descricao inteira
// quando ela nao e o molde. Pode devolver "" (so havia o molde).
const char *guia_desc_livre(const char *desc, char *buf, size_t n);

// BUSCA DE CANAIS PARA O SPOTLIGHT (spotlight.c): os canais publicados cujo
// nome normalizado (buscanorm.h) contem `alvoNorm`, na ordem do guia, ate
// `max`. 0 com a lista ainda nao carregada. Os ponteiros de guia_canal_campos
// valem ate a proxima republicacao da lista (so o fio de desenho chama).
int  guia_buscar_canais(const char *alvoNorm, int *indices, int max);
// Le a lista do cache do guia (a ultima publicada) se a sessao ainda nao tem
// lista. Nao vai a rede. O Spotlight chama ao abrir.
void guia_preparar_busca(void);
int  guia_canal_campos(int i, const char **id, const char **nome, const char **logo,
                       const char **cat, const char **base);

#endif
