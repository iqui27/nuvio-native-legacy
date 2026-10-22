#ifndef NV_EPISODIOS_H
#define NV_EPISODIOS_H
#include <SDL2/SDL.h>
#include "vistoep.h"
#include "gfx.h"
void episodios_abrir(int titulo, int temporada, int episodio);
int episodios_aberto(void);
float episodios_anim(void);   // 0..1, a entrada da folha
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
// O MENU TEM MOLA (abre, fecha, foco das linhas): quem o hospeda sobre a
// propria tela chama uma vez por quadro. Dentro da folha do player quem anda
// com ela e episodios_atualizar.
void episodios_menu_atualizar(float dt);
// 1 enquanto ha o que desenhar — aberto, ou fechando (a mola ainda nao pousou).
// episodios_menu_aberto cai no instante do Voltar e devolve as teclas a pagina;
// este segura o desenho ate o fim do caminho de volta.
int  episodios_menu_visivel(void);
// DE ONDE O MENU SAIU, na tela virtual (escala.h), a cada quadro e antes de
// episodios_menu_desenhar. No menu do EPISODIO e o card: a ilha fica ao lado
// dele, como a do cartaz na home, e quem chamou redesenha o card por cima do
// veu. No da TEMPORADA e a aba: ela se abre para baixo num painel com as
// acoes (episodios_menu_painel() = 1), e quem chamou redesenha a aba por cima,
// de cabecalho. Sem ancora o menu vai para o meio da tela.
void episodios_menu_ancora(GfxRect r);
int  episodios_menu_painel(void);
// Para teste: a caixa (tela virtual) do menu ou do painel ABERTO no ultimo
// quadro desenhado, a mola de abertura 0..1 e a linha em foco.
int   episodios_menu_caixa(GfxRect *r);
float episodios_menu_anim(void);
int   episodios_menu_foco(void);
// 1 com o menu aberto no modo temporada (para teste).
int  episodios_menu_modo_temporada(void);
// O lote que "temporada inteira" aplica: catalogo + mapa, sem o que nao foi ao
// ar. `saida` nula conta. Publico para o teste.
int  episodios_lote(int idxCat, int temporada, VistoPar *saida, int max);
void episodios_fechar(void);
// O titulo da folha, ja conferido pelo id. Para teste (#190).
int  episodios_titulo(void);
// O menu de visto esta aberto, venha da folha ou da pagina de detalhe. Para
// teste; a pagina de detalhe usa episodios_menu_aberto, que so ve o seu.
int  episodios_menu_aberto_qualquer(void);

#ifdef NV_SHOT_HOOKS
void episodios_shot_menu(void);
void episodios_shot_foco(int linha);
#endif

#endif
