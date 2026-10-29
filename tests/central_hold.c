// Control Center hold feedback: deterministic event timestamps, no GL/window.
#include "central.h"
#include "chsegura.h"
#include "anim.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

// Missing feedback on the baseline must fail an assertion, not the linker.
__attribute__((weak, noinline)) float central_segurar_progresso(Uint32 agora) {
  (void)agora;
  return 0.0f;
}
static float progress(Uint32 t) { return central_segurar_progresso(t); }
static int delivered;
static SDL_Event original;
static void deliver(SDL_Event *e) {
  assert(e->key.keysym.sym == original.key.keysym.sym);
  assert(e->key.timestamp == original.key.timestamp);
  assert(e->type == (delivered % 2 ? SDL_KEYUP : SDL_KEYDOWN));
  if (e->type == SDL_KEYUP) assert(e->key.state == SDL_RELEASED);
  delivered++;
}
static void key(Uint32 t, Uint32 type) {
  SDL_Event e = {0};
  e.type = type;
  e.key.timestamp = t;
  e.key.keysym.sym = SDLK_PAGEUP;
  e.key.state = type == SDL_KEYDOWN ? SDL_PRESSED : SDL_RELEASED;
  if (type == SDL_KEYDOWN && !central_tecla_ocupada()) original = e;
  assert(central_tecla(&e, 1));
}
static void frame(Uint32 t) { central_tecla_quadro(t, deliver); }
// Expected fill after d ms held: dead zone, then linear to the threshold.
static float fill(Uint32 d) {
  return d <= CHS_ATRASO_MS ? 0.0f : (d - CHS_ATRASO_MS) / (float)(CHS_SEGURAR_MS - CHS_ATRASO_MS);
}
static void near(float actual, float expected) { assert(fabsf(actual - expected) < 0.00001f); }

int main(void) {
  Uint32 t = CHS_ESQUECER_MS;
  SDL_Init(SDL_INIT_TIMER);

  // Platforms without valid keyup keep their existing silence fallback.
  key(t, SDL_KEYDOWN);
  frame(t + CHS_SILENCIO_MS - 1);
  assert(delivered == 0 && !central_aberta());
  frame(t + CHS_SILENCIO_MS);
  assert(delivered == 2 && !central_aberta());
  assert(progress(t + CHS_SILENCIO_MS) > 0.0f);
  assert(progress(t + CHS_SILENCIO_MS + CHS_EMENDA_MS / 2) > 0.0f);
  assert(progress(t + CHS_SILENCIO_MS + CHS_EMENDA_MS / 2) < progress(t + CHS_SILENCIO_MS));
  near(progress(t + CHS_SILENCIO_MS + CHS_EMENDA_MS), 0.0f);
  delivered = 0;
  t += CHS_ESQUECER_MS;

  key(t, SDL_KEYDOWN);
  near(progress(t), 0.0f);
  frame(t + 250);
  near(progress(t + 250), 0.0f);
  assert(progress(t + 400) > 0.0f);
  frame(t + CHS_SEGURAR_MS * 2 / 3);
  near(progress(t + CHS_SEGURAR_MS * 2 / 3), 1.0f / 3);
  assert(!central_aberta() && delivered == 0);

  // Early release reverses immediately; short press still waits for emenda.
  key(t + CHS_SEGURAR_MS * 2 / 3, SDL_KEYUP);
  near(progress(t + CHS_SEGURAR_MS * 2 / 3), 1.0f / 3);
  frame(t + CHS_SEGURAR_MS * 2 / 3 + CHS_EMENDA_MS / 2);
  assert(progress(t + CHS_SEGURAR_MS * 2 / 3 + CHS_EMENDA_MS / 2) > 0.0f);
  assert(progress(t + CHS_SEGURAR_MS * 2 / 3 + CHS_EMENDA_MS / 2) < 1.0f / 3);
  assert(delivered == 0 && !central_aberta());
  frame(t + CHS_SEGURAR_MS * 2 / 3 + CHS_EMENDA_MS);
  near(progress(t + CHS_SEGURAR_MS * 2 / 3 + CHS_EMENDA_MS), 0.0f);
  assert(delivered == 2 && !central_aberta());
  frame(t + CHS_SEGURAR_MS);
  assert(delivered == 2);

  // A clock-driven hold opens exactly at the existing threshold, once.
  t += CHS_ESQUECER_MS;
  key(t, SDL_KEYDOWN);
  frame(t + CHS_SEGURAR_MS - 1);
  near(progress(t + CHS_SEGURAR_MS - 1), fill(CHS_SEGURAR_MS - 1));
  assert(!central_aberta());
  frame(t + CHS_SEGURAR_MS);
  near(progress(t + CHS_SEGURAR_MS), 1.0f);
  assert(central_aberta() && delivered == 2);
  key(t + CHS_SEGURAR_MS + CHS_EMENDA_MS, SDL_KEYUP);
  frame(t + CHS_SEGURAR_MS + CHS_EMENDA_MS * 2);
  near(progress(t + CHS_SEGURAR_MS + CHS_EMENDA_MS * 2), 0.0f);
  assert(delivered == 2);
  central_fechar();

  // Repeated keydowns use the same deadline and complete feedback as well.
  t += CHS_ESQUECER_MS;
  key(t, SDL_KEYDOWN);
  key(t + CHS_SEGURAR_MS * 2 / 3, SDL_KEYDOWN);
  near(progress(t + CHS_SEGURAR_MS * 2 / 3), 1.0f / 3);
  key(t + CHS_SEGURAR_MS, SDL_KEYDOWN);
  near(progress(t + CHS_SEGURAR_MS), 1.0f);
  assert(central_aberta() && delivered == 2);
  key(t + CHS_SEGURAR_MS + CHS_EMENDA_MS, SDL_KEYUP);
  frame(t + CHS_SEGURAR_MS + CHS_EMENDA_MS * 2);
  central_fechar();

  // Reduced motion keeps timed progress, but cancels without animation.
  t += CHS_ESQUECER_MS;
  anim_politica_reduzida = 1;
  key(t, SDL_KEYDOWN);
  near(progress(t + CHS_SEGURAR_MS * 2 / 3), 1.0f / 3);
  key(t + CHS_SEGURAR_MS * 2 / 3, SDL_KEYUP);
  near(progress(t + CHS_SEGURAR_MS * 2 / 3), 0.0f);
  frame(t + CHS_SEGURAR_MS * 2 / 3 + CHS_EMENDA_MS);
  assert(delivered == 4 && !central_aberta());
  anim_politica_reduzida = 0;
  // Click released at 200 ms never drew progress, even through the release fade.
  t += CHS_ESQUECER_MS;
  anim_politica_reduzida = 0;
  key(t, SDL_KEYDOWN);
  frame(t + 100);
  key(t + 200, SDL_KEYUP);
  for (Uint32 d = 0; d <= 200 + CHS_EMENDA_MS + 20; d += 10) {
    frame(t + d);
    near(progress(t + d), 0.0f);
  }

  puts("PASS: threshold progress, smooth cancellation, original short press, repeat, reduced motion");
  SDL_Quit();
  return 0;
}
