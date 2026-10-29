// The rimless variant must keep the shared glass/solid material path.
#include "plrui.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

int gfx_vidro_sem_aro;
static int glass, materials, fills;
static GfxRect expected;
int ajustes_vidro(void) { return glass; }
void gfx_rect(GfxRect r, GLuint tex, GfxModo mode, float focus, float px, float py,
              float radius, float red, float green, float blue, float alpha) {}
void gfx_sombra_sob(GfxRect r, float focus, float px, float radius, float red,
                    float green, float blue, float alpha, GfxRect clip, float cr, float a) {}
void gfx_cor(GfxRect r, float radius, float red, float green, float blue, float alpha) {
  assert(!glass);
  assert(fabsf(red - .082f) < .0001f && fabsf(green - .086f) < .0001f && fabsf(blue - .102f) < .0001f);
  assert(r.x == expected.x && r.y == expected.y && r.w == expected.w && r.h == expected.h);
  fills++;
}
void gfx_vidro_material(GfxRect r, float radius, float density, float alpha) {
  assert(gfx_vidro_sem_aro);
  assert(density == GFX_VIDRO_DENS);
  assert(r.x == expected.x && r.y == expected.y && r.w == expected.w && r.h == expected.h);
  materials++;
}
int main(void) {
  expected = (GfxRect){120, 540, 920, 64};
  for (glass = 0; glass <= 1; glass++) for (int previous = 0; previous <= 1; previous++) {
    gfx_vidro_sem_aro = previous;
    plrui_material_plano(expected, 28, .7f);
    assert(gfx_vidro_sem_aro == previous);
  }
  assert(materials == 2 && fills == 2);
  puts("PASS: rimless material uses shared glass/solid path and restores rim state");
}
