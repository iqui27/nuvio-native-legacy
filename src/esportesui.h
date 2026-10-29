#ifndef NV_ESPORTESUI_H
#define NV_ESPORTESUI_H
#include <SDL2/SDL.h>
#include "esportes.h"
#include "gfx.h"
/* Guide v5 shares artwork and the existing bounded Sources matcher. */
void esportesui_fontes(const EspEvento *ev);
void esportesui_fontes_passo(void);
void esportesui_cartao(const EspEvento *ev, GfxRect rect, int focused);
void esportesui_logo(const char *url, int sport, GfxRect rect);
int esportesui_fundo(GfxRect rect, int sport, int focused);
void esportesui_recursos(const char *dirArte);
void esportesui_iniciar(void);
void esportesui_fechar(void);
void esportesui_evento(const SDL_Event *e);
void esportesui_atualizar(float dt, Uint32 agora);
void esportesui_desenhar(Uint32 agora);
int esportesui_quer_sair(void);
int esportesui_pediu_relogio(void);
int esportesui_pediu_canal(char *id, unsigned n);
#endif
