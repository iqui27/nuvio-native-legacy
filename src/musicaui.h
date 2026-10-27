#ifndef NV_MUSICAUI_H
#define NV_MUSICAUI_H
#include "musica.h"
#include "gfx.h"
#include <SDL2/SDL.h>
void musicaui_abrir(const MusAlbum *a, const char *imdb, const char *titulo, int ano, int temp);
void musicaui_fechar(void);
void musicaui_origem(GfxRect origem);
GLuint musicaui_ambiente(const char *capa);
// On-screen rect of the modal island this frame (card rect -> centred modal).
GfxRect musicaui_painel(void);
int musicaui_visivel(void);
int musicaui_aberta(void);
void musicaui_evento(const SDL_Event *e);
void musicaui_atualizar(Uint32 agora);
void musicaui_desenhar(Uint32 agora);
#endif
