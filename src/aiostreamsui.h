#ifndef NV_AIOSTREAMSUI_H
#define NV_AIOSTREAMSUI_H
#include <SDL2/SDL.h>
#include "aiostreams.h"
void aioui_arte(const char *dir);
void aioui_abrir(int addon);
void aioui_inspecionar(const char *id, const char *tipo);
int aioui_aberto(void);
int aioui_opaco(void);
void aioui_evento(const SDL_Event *e);
void aioui_atualizar(float dt, Uint32 agora);
void aioui_desenhar(Uint32 agora);
void aioui_addons_desenhar(int foco);
int aioui_detectar(int addon);
#ifdef NV_SHOT_HOOKS
void aioui_shot(int pagina, const AioSnapshot *s);
#endif
#endif
