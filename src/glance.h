// Quick glance over a title list. The owner keeps its list and scroll alive.
#ifndef NV_GLANCE_H
#define NV_GLANCE_H
#include <SDL2/SDL.h>
#include "catalogo.h"
typedef struct {
  int (*total)(void);
  int (*item)(int i, CatItem *out);
  void (*foco)(int i);
  void (*detalhes)(int i);
} GlanceFonte;
enum { GLANCE_BT_PRIMARIO, GLANCE_BT_DETALHES, GLANCE_BT_SALVAR, GLANCE_BT_VISTO };
void glance_abrir(const GlanceFonte *fonte, int i);
void glance_fechar(void);
int glance_aberto(void);
int glance_indice(void);
int glance_botao(void);
void glance_evento(const SDL_Event *e);
void glance_atualizar(float dt, Uint32 agora);
void glance_desenhar(Uint32 agora);
// One-shot playback intent. Details still use the owner's original route,
// including its asynchronous TMDB resolution; the router consumes this later.
int glance_pediu_tocar(const CatItem *titulo);
// Consume only metadata requested for a quick action, never open Details.
int glance_tomar_acao(int indiceCatalogo);
#endif
