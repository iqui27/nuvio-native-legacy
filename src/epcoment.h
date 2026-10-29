#ifndef NV_EPCOMENT_H
#define NV_EPCOMENT_H
#include <SDL2/SDL.h>
#include "gfx.h"
void epcoment_abrir(const char *imdb, int temporada, int episodio);
void epcoment_atualizar(float dt);
int epcoment_quantidade(void); // -1 loading, 0 empty
int epcoment_entrar(void);
int epcoment_evento(const SDL_Event *e);
void epcoment_desenhar(GfxRect r, float a);
int epcoment_foco(void); // -1 when focus belongs to the menu
void epcoment_menu(void);
// Merge hook: central can route this to the shared full-review reader.
void comentario_abrir_inteiro(int indice);
#endif
