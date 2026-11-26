// The faded-group path of gfx.c (subtitles at alpha < 1) for fixtures that
// link src/assrender.c without src/gfx.c: never grouped, drawn straight.
#include "gfx.h"
int gfx_grupo_comecar(GfxGrupo *g, GfxRect caixa, float alpha) {
  (void)caixa; (void)alpha;
  g->ativo = 0;
  return 0;
}
void gfx_grupo_terminar(GfxGrupo *g, float alpha) { (void)g; (void)alpha; }
