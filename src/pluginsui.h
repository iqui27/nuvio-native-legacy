// Plugins Nuvio (F09): ligar/desligar, os repositorios (da conta ou digitados)
// com os scrapers de cada um, e adicionar/remover repositorio. Ver plugins.h.
// O desenho mora em ajustes_ux_ilha.inc (ajustes_desenhar_plugins), com as
// mesmas pecas de vidro da lista de addons.
#ifndef NV_PLUGINSUI_H
#define NV_PLUGINSUI_H
#include <SDL2/SDL.h>

void pluginsui_abrir(void);
void pluginsui_evento(const SDL_Event *e);
void pluginsui_atualizar(float dt, Uint32 agora);
void pluginsui_desenhar(Uint32 agora);
int  pluginsui_quer_sair(void);
// Ponteiro (#99): foca a linha i (PonteiroFn). pluginsui_foco para os testes.
void pluginsui_ponteiro(int i, int b);
int  pluginsui_foco(void);
#ifdef NV_TOUCH_UI
// A seta da linha abre os scrapers; o restante da linha continua alternando.
void pluginsui_detalhes(int i, int b);
#endif
#ifdef AJUSTES_TESTE
void pluginsui_teste(int nivel, int foco, int repo);
#endif

#endif
