#include "../src/ajustes.c"
#include <assert.h>

// 2.1 (three islands): the three toggles live in Reproducao > Escolha da fonte,
// are device-local, reachable by the deep link and the search, and OK flips
// them at once (no editor), like every switch.
static void pressSettings(SDL_Keycode k) { SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=k;ajustes_evento(&e); }
void fonteincompat_settings(void) {
  ajustes_iniciar();
  for (int op=AJ_FONTE_OCULTAR_DV;op<=AJ_FONTE_OCULTAR_VIDEO;op++) {
    assert(escopoDe(op)==ESC_TV && somenteDesteAparelho(op));
    assert(uxValorAtual(op)==uxValorPadrao(op));
    triIniciar(); focarOpcao(op);
    assert(focoOp==op && triIlha==TRI_LISTA && visivel(focoItem) && secAtual==uxSecaoDe(op));
  }
  AjusteBuscaResultado found[AJ_N];
  int n=ajustes_buscar("incompatível",found,AJ_N), hits=0;
  for(int i=0;i<n;i++) hits += found[i].op>=AJ_FONTE_OCULTAR_DV && found[i].op<=AJ_FONTE_OCULTAR_VIDEO;
  assert(hits==3);
  // Three values (Automatico / Ligado / Desligado): OK opens the editor, the
  // chosen value applies on the second OK, and "Automatico" follows this TV.
  triIniciar(); focarOpcao(AJ_FONTE_OCULTAR_DV);
  int before=ajustes_fonte_ocultar(0);
  pressSettings(SDLK_RETURN); assert(uxEditor && uxOp==AJ_FONTE_OCULTAR_DV && ajustes_fonte_ocultar(0)==before);
  uxPendente=before?1:0; pressSettings(SDLK_RETURN);
  assert(!uxEditor && ajustes_fonte_ocultar(0)!=before);
  pressSettings(SDLK_RETURN); uxPendente=2; pressSettings(SDLK_RETURN);
  assert(!uxEditor && valor[AJ_FONTE_OCULTAR_DV]==2 && ajustes_fonte_ocultar(0)==before);
}
void fonteincompat_settings_draw(void) {
  ajustes_atualizar(1.f/60,SDL_GetTicks());
  ajustes_desenhar(SDL_GetTicks());
}
