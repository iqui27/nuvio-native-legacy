#ifndef NV_EPISODIOS_H
#define NV_EPISODIOS_H
#include <SDL2/SDL.h>
void episodios_abrir(int titulo, int temporada, int episodio);
int episodios_aberto(void);
void episodios_evento(const SDL_Event *e);
void episodios_atualizar(float dt);
void episodios_desenhar(void);
int episodios_escolheu(int *temporada, int *episodio);

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
void episodios_fechar(void);
#endif
