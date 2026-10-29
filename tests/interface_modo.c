// Device policy, rotation and retained touch state; no window or network.
#include "layout.h"
#include "telefoneui.h"
#ifdef NV_TOUCH_UI
#include "rolagemtoque.h"
#endif
#include <assert.h>
#include <stdio.h>

static void dimensoes(float w, float h) {
  assert(fabsf(NV_TELA_W - w) < 0.1f);
  assert(fabsf(NV_TELA_H - h) < 0.1f);
}

static void politica(void) {
  for (int automatico = 0; automatico <= 1; automatico++) {
    assert(!layout_modo_resolver(0, automatico)); // saved TV wins on phones
    assert(layout_modo_resolver(1, automatico));  // saved Mobile wins on TVs
    assert(layout_modo_resolver(2, automatico) == automatico);
    assert(layout_modo_resolver(-1, automatico) == automatico);
    assert(layout_modo_resolver(99, automatico) == automatico);
  }
}

#ifdef NV_TOUCH_UI
static void rolagem(void) {
  float offset = 40.0f;
  ToqueRolagem r = {0};
  PonteiroRolagem e = {0};
  layout_modo_definir(1);
  toquerol_vincular(&r, (GfxRect){100, 100, 300, 200}, 2, 0, 300, 1, &offset);
  e.fase = PONT_ROL_INICIO; e.x = 240; e.y = 260;
  e.eixoY = 0; assert(!toquerol_evento(&r, &e));
  e.eixoY = 1; e.x = 50; assert(!toquerol_evento(&r, &e));
  e.x = 240; assert(toquerol_evento(&r, &e));
  e.fase = PONT_ROL_MOVER; e.delta = -80;
  assert(toquerol_evento(&r, &e) && offset == 80);
  e.fase = PONT_ROL_INERCIA; e.delta = -1000;
  assert(toquerol_evento(&r, &e) && offset == 300);
  e.fase = PONT_ROL_FIM; assert(toquerol_evento(&r, &e));
  assert(toquerol_livre(&r)); // release keeps the selected scroll position

  // A hidden sheet can retain a drag when Settings switches back to TV.
  layout_modo_definir(0);
  assert(!toquerol_livre(&r));
  assert(offset == 300); // TV resumes its own focus scrolling, without a jump here
  e.fase = PONT_ROL_INERCIA; e.delta = 80;
  assert(!toquerol_evento(&r, &e) && offset == 300);
  r.livre = 1; // a previously registered callback also has to reject TV
  assert(!toquerol_evento(&r, &e) && !r.livre && offset == 300);
  layout_modo_definir(1);
  assert(!toquerol_evento(&r, &e)); // no stale inertia after returning to Mobile
  e.fase = PONT_ROL_INICIO; assert(toquerol_evento(&r, &e));
  toquerol_vincular(&r, r.regiao, 2, 0, 50, 1, &offset);
  assert(offset == 50); // a resized/replaced list clamps an active swipe
  toquerol_limpar(&r); assert(!toquerol_livre(&r));
}
#endif

int main(void) {
  politica();
  layout_modo_definir(0); layout_tela_definir(1080, 2340);
  assert(!telefoneui_ativo()); dimensoes(1920, 1080);
  layout_modo_definir(1);
#ifdef NV_TOUCH_UI
  assert(telefoneui_ativo()); dimensoes(1080, 2340);
  layout_tela_definir(2340, 1080); dimensoes(2340, 1080);
  layout_tela_definir(0, -1); dimensoes(2340, 1080);
  layout_modo_definir(0); dimensoes(1920, 1080);
  layout_modo_definir(1); dimensoes(2340, 1080); // reuse the last drawable
  layout_tela_definir(1200, 1920); dimensoes(1080, 1728);
  rolagem();
  puts("interface_modo: PASS (saved choice, Auto, rotation, TV/Mobile scroll reset)");
#else
  assert(!layout_modo_mobile() && !telefoneui_ativo()); dimensoes(1920, 1080);
  layout_tela_definir(2340, 1080); dimensoes(1920, 1080);
  puts("interface_modo: PASS (non-touch TV keeps its fixed canvas)");
#endif
  return 0;
}
