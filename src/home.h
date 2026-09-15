#ifndef NV_HOME_H
#define NV_HOME_H
#include <SDL2/SDL.h>
#include "gfx.h"
#include "catalogo.h"

// Tipos de fileira presentes na home moderna do Nuvio 1.0.1 legacy.
typedef enum {
  FILEIRA_CONTINUE,   // card 16:9 com barra de progresso
  FILEIRA_NORMAL,      // poster retrato 2:3
  FILEIRA_DESTAQUE,    // seleção editorial: arte landscape maior
  FILEIRA_TOP10,       // ranking usa poster retrato no legacy
  FILEIRA_COLECAO,     // premiações / coleções: landscape intermediário
  FILEIRA_SERVICO,     // catálogo por serviço: landscape compacto
  FILEIRA_SOCIAL,      // atividade dos amigos: editorial largo com autoria
  FILEIRA_RETORNO,     // sessão recém-interrompida: faixa compacta de retomar
  FILEIRA_CATALOGOS    // atalhos para catálogos existentes, não títulos
} TipoFileira;

// O item sob o foco, com o retangulo que ele ocupa na tela NESTE quadro. A
// transicao para o detalhe parte dai: o card nao "abre uma tela nova", ele voa
// ate virar o hero — e sem o rect de origem real a animacao teria que chutar
// de onde veio, que e exatamente o que faz uma transicao parecer barata.
typedef struct {
  // Indice NO CATALOGO do card focado. Faltava, e por isso o detalhe abria
  // sempre o item 0: a tela de destino nao tinha como saber o que fora aberto.
  int indice;
  GfxRect rect;
  const char *arte;
  const char *titulo;
  const char *genero;
  const char *meta;
} HomeItem;

int  home_iniciar(const char *dirArte);
int  home_item_focado(HomeItem *out);      // 0 se o foco ainda nao foi desenhado
int  home_n_artes(void);                   // acervo de backdrops, usado pelo detalhe
// Ha fileira montada? Serve para o app saber que o catalogo CHEGOU depois do
// arranque, e nao so que existia na hora de abrir.
int  home_tem_fileiras(void);
const char *home_arte(int i);
const char *home_backdrop(int i);   // arte do titulo i do catalogo
void home_evento(const SDL_Event *e);
void home_atualizar(float dt, Uint32 agora);
void home_desenhar(Uint32 agora);

// Onde a ARTE do hero foi desenhada no ultimo quadro. A tela de detalhe usa
// isto para nascer com o backdrop no mesmo lugar — o fundo do detalhe E a arte
// do titulo em foco, e ela nao deve reaparecer, deve continuar.
void home_hero_rect(float *x, float *y, float *w, float *h);
void home_encerrar(void);
// Registra o titulo interrompido para a faixa contextual "Retomar agora".
// A faixa so existe enquanto o progresso fizer sentido (nem inicio nem fim).
void home_registrar_retorno(int indice, double posSeg, double durSeg);
int  home_quer_sair(void);
int  home_pediu_abrir(void);   // OK pressionado: consome o pedido
int  home_pediu_menu(void);    // ESQUERDA na primeira coluna: chama o menu
int home_pediu_social(void);
int home_pediu_pessoa_social(CatItem *saida);
// OK numa fileira de canal ("Ver tudo" dela, ou num cartao de canal — que ai
// recebe o id para o guia ja abrir focado nele). `id` pode ser NULL.
int  home_pediu_guia(char *id, int tam);

#endif
