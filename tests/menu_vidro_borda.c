// Executa a funcao real de gfx.c com capturas das passadas de desenho.
#include "gfx.h"
#include <assert.h>
#include <stdio.h>
static int efeitosMinimos, contorno, aneis, brilhos;
int ajustes_vidro_contorno(void) { return contorno; }
void gfx_anel(GfxRect r, float raio, float esp, float cr, float cg, float cb, float a) { aneis++; }
void gfx_vidro_aro(GfxRect r, float raio, float esp, float cr, float cg, float cb, float a) {
  if (ajustes_vidro_contorno()) gfx_anel(r, raio, esp, cr, cg, cb, a);
}
void gfx_brilho_topo(GfxRect r, float raio, float cobertura, float cr, float cg, float cb, float a) { brilhos++; }
#include "menu_vidro_borda_impl.h"
int main(void) {
  for (int minimo=0; minimo<2; minimo++) {
    for (contorno=0; contorno<2; contorno++) {
      efeitosMinimos=minimo; aneis=brilhos=0;
      gfx_vidro_borda((GfxRect){100,100,400,80},0.5f,1.0f);
      assert(aneis == (!minimo && contorno));
      assert(brilhos == !minimo);
    }
  }
  puts("menu_vidro_borda: ok");
  return 0;
}
