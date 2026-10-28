#include "../src/ajustes.c"
#include <assert.h>

void fonteincompat_settings(void) {
  ajustes_iniciar();
  for (int op=AJ_FONTE_OCULTAR_DV;op<=AJ_FONTE_OCULTAR_VIDEO;op++) {
    assert(escopoDe(op)==ESC_TV && somenteDesteAparelho(op));
    assert(uxValorAtual(op)==uxValorPadrao(op));
    ajPenFind(op); assert(ajPenPage==31);
  }
  AjusteBuscaResultado found[AJ_N];
  int n=ajustes_buscar("incompatível",found,AJ_N), hits=0;
  for(int i=0;i<n;i++) hits += found[i].op>=AJ_FONTE_OCULTAR_DV && found[i].op<=AJ_FONTE_OCULTAR_VIDEO;
  assert(hits==3);
  ajPenOpen(31); aj4Topo=-1; ajPenRow=7;
  int before=ajustes_fonte_ocultar(0);
  assert(ajPenEvent(SDLK_RETURN));
  assert(ajustes_fonte_ocultar(0)!=before);
  assert(ajPenEvent(SDLK_RETURN));
  assert(ajustes_fonte_ocultar(0)==before);
  ajPenRow=7;
}
void fonteincompat_settings_draw(void) {
  ajustes_atualizar(1.f/60,SDL_GetTicks());
  ajustes_desenhar(SDL_GetTicks());
}
