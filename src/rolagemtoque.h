#ifndef NV_ROLAGEMTOQUE_H
#define NV_ROLAGEMTOQUE_H

#ifdef NV_TOUCH_UI
#include "gfx.h"
#include "layout.h"
#include "ponteiro.h"
#include <math.h>

/* Geometry is saved while drawing: pointer events use base canvas units,
 * whereas a sheet may draw at an enlarged UI scale. Free scrolling survives
 * release and redraw; only navigation or a new list restores focus scrolling. */
typedef struct {
  GfxRect regiao;
  float escala, minimo, maximo;
  float *offset;
  int eixoY, livre;
} ToqueRolagem;

static inline float toquerol_clamp(float v, float minimo, float maximo) {
  return v < minimo ? minimo : v > maximo ? maximo : v;
}
/* A screen may retain touch state while the mode selector is open. TV resumes
 * its selected-item scrolling as soon as that state is read again. */
static inline int toquerol_livre(ToqueRolagem *r) {
  if (!layout_modo_mobile()) r->livre = 0;
  return r->livre;
}
static inline void toquerol_vincular(ToqueRolagem *r, GfxRect regiao,
                                    float escala, float minimo, float maximo,
                                    int eixoY, float *offset) {
  r->regiao = regiao;
  r->escala = escala > 0.0f ? escala : 1.0f;
  r->minimo = minimo;
  r->maximo = maximo > minimo ? maximo : minimo;
  r->eixoY = eixoY;
  r->offset = offset;
  if (toquerol_livre(r) && offset) *offset = toquerol_clamp(*offset, r->minimo, r->maximo);
}
static inline void toquerol_limpar(ToqueRolagem *r) { r->livre = 0; }
static inline int toquerol_evento(ToqueRolagem *r, const PonteiroRolagem *e) {
  if (!layout_modo_mobile()) { r->livre = 0; return 0; }
  if (!r->offset) return 0;
  if (e->fase == PONT_ROL_INICIO) {
    float x = e->x / r->escala, y = e->y / r->escala;
    if (e->eixoY != r->eixoY || r->maximo <= r->minimo ||
        x < r->regiao.x || x >= r->regiao.x + r->regiao.w ||
        y < r->regiao.y || y >= r->regiao.y + r->regiao.h) return 0;
    r->livre = 1;
    return 1;
  }
  if (!r->livre) return 0;
  if (e->fase == PONT_ROL_MOVER || e->fase == PONT_ROL_INERCIA) {
    float antes = *r->offset;
    *r->offset = toquerol_clamp(antes - e->delta / r->escala, r->minimo, r->maximo);
    return fabsf(*r->offset - antes) > 0.001f;
  }
  return 1;
}
static inline int toquerol_navegacao(const SDL_Event *e) {
  SDL_Keycode k;
  if (e->type != SDL_KEYDOWN) return 0;
  k = e->key.keysym.sym;
  return k == SDLK_UP || k == SDLK_DOWN || k == SDLK_LEFT || k == SDLK_RIGHT ||
         k == SDLK_PAGEUP || k == SDLK_PAGEDOWN || k == SDLK_HOME || k == SDLK_END;
}
#endif
#endif
