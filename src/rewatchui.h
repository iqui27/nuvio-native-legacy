#ifndef NV_REWATCHUI_H
#define NV_REWATCHUI_H
#include "catalogo.h"
#include <SDL2/SDL.h>
// A mesma ilha de contexto para titulo e Continuar assistindo.
void rewatchui_abrir(const CatItem *ci, int origem, int t, int e);
int rewatchui_aberta(void);
void rewatchui_evento(const SDL_Event *ev);
void rewatchui_desenhar(void);
int rewatchui_tocar(char *id, size_t tam, int *t, int *e);
#endif
