#ifndef NV_FILMO_H
#define NV_FILMO_H
#include <SDL2/SDL.h>
void filmo_abrir(void);
// 1 requests closing the person screen; title requests use filmo_titulo().
int filmo_evento(const SDL_Event *e);
void filmo_desenhar(float a);
int filmo_titulo(void);
int filmo_foco(void);
int filmo_aba(void);
int filmo_camada(void);
#endif
