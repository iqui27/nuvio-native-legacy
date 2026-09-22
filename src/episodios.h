#ifndef NV_EPISODIOS_H
#define NV_EPISODIOS_H
#include <SDL2/SDL.h>
#include "vistoep.h"
void episodios_abrir(int titulo, int temporada, int episodio);
int episodios_aberto(void);
void episodios_evento(const SDL_Event *e);
void episodios_atualizar(float dt);
void episodios_desenhar(void);
int episodios_escolheu(int *temporada, int *episodio);

// PARA TESTE: a linha em foco e a rolagem ja aplicada. O issue #102 — a folha
// abrindo no primeiro episodio da temporada e voltando para ele sozinha — nao
// tinha como ser provado por assercao sem estes dois: o unico observavel era a
// captura de tela, que nao falha por conta propria.
int   episodios_foco_linha(void);
float episodios_rolagem(void);

// O MENU DE VISTO, SOZINHO, sobre a tela de quem chamar.
//
// Ele nasceu dentro da folha de episodios e so podia ser usado la — e a folha
// so abre de DENTRO DO PLAYER (episodios_abrir e chamada de player.c e mais
// nada). Ou seja: "marcar este / ate aqui / a temporada inteira" existia e era
// inalcancavel para quem estava na pagina de detalhe, que e onde qualquer um
// iria procurar. Isto abre a mesma coisa de fora.
//
// `temporada` e o NUMERO da temporada, nao o indice de uma aba.
void episodios_menu_visto(int idxCat, int temporada, int episodio, const char *nome);
int  episodios_menu_aberto(void);
void episodios_menu_evento(const SDL_Event *e);
void episodios_menu_desenhar(void);
// 1 uma vez, quando a pessoa escolheu "Fontes deste episodio" no menu. Quem
// chamou decide o que abrir — o menu nao conhece a folha de fontes.
int  episodios_menu_pediu_fontes(void);
// O MENU DA TEMPORADA (issue #108), sozinho sobre a tela de quem chamar — a
// pagina de detalhe abre com a pressao longa na aba. Duas linhas: "Marcar
// temporada como assistida" e "Desmarcar temporada". `temporada` e o NUMERO.
// Eventos e desenho pelas mesmas episodios_menu_evento/desenhar.
void episodios_menu_temporada(int idxCat, int temporada);
// 1 com o menu aberto no modo temporada (para teste).
int  episodios_menu_modo_temporada(void);
// O lote que "temporada inteira" aplica: catalogo + mapa, sem o que nao foi ao
// ar. `saida` nula conta. Publico para o teste.
int  episodios_lote(int idxCat, int temporada, VistoPar *saida, int max);
void episodios_fechar(void);
// O menu de visto esta aberto, venha da folha ou da pagina de detalhe. Para
// teste; a pagina de detalhe usa episodios_menu_aberto, que so ve o seu.
int  episodios_menu_aberto_qualquer(void);

#endif
