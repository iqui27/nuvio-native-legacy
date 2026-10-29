// Inspect the anchor consumed by the actual context-menu drawing path.
#include "../src/ctxmenu.c"
#include <assert.h>
int rev4_home(void) {
  HomeItem displayed;
  assert(home_item_focado(&displayed));
  assert(displayed.rect.w > 8 && displayed.rect.w < 960);
  ctx_abrir(displayed.indice);
  assert(ctx_aberto() && temCartaz);
  ctx_atualizar(1.f / 60, SDL_GetTicks());
  ctx_desenhar(SDL_GetTicks());
  GfxRect actual = cartazRect;
  float e = gfx_escala_ui();
  float error = fabsf(actual.x * e - displayed.rect.x) +
                fabsf(actual.y * e - displayed.rect.y) +
                fabsf(actual.w * e - displayed.rect.w) +
                fabsf(actual.h * e - displayed.rect.h);
  printf("%s Home menu redraw: displayed %.2f,%.2f %.2fx%.2f; menu %.2f,%.2f %.2fx%.2f\n",
         error > .1f ? "FAIL" : "PASS", displayed.rect.x, displayed.rect.y,
         displayed.rect.w, displayed.rect.h, actual.x * e, actual.y * e,
         actual.w * e, actual.h * e);
  return error > .1f;
}
