// Exercise the shared top-bar/TV-menu avatar renderer with real cached GIFs.
#include "gfx.h"
static float aspect;
static GfxModo drawnMode;
static void menuRect(GfxRect r, GLuint tex, GfxModo mode, float f, float x, float y,
                     float radius, float red, float green, float blue, float alpha) {
  if (tex) { aspect = gfx_tex_aspect_atual; drawnMode = mode; }
  gfx_rect(r, tex, mode, f, x, y, radius, red, green, blue, alpha);
}
#define gfx_rect menuRect
#include "../src/menu.c"
#undef gfx_rect
#include <assert.h>

float gifavatar_menu_aspect(void) {
  aspect = -1;
  tvAvatar((GfxRect){0, 0, 44, 44}, 1);
  assert(aspect > 0 && drawnMode == GFX_AVATAR);
  assert(gfx_tex_aspect_atual == 0);
  return aspect;
}
