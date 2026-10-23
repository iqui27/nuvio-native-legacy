// ISSUE #513, the slot. The latched T/E used to ride cwTocarT/E, which is the
// Continue Watching slot. The source sheet stays open across frames
// (stream_folha_aberta eats the keys), so a later writer of that slot wins:
//
//   1. sheet opened for T1E3, then a Continue Watching card stamps T2E5 into
//      cwTocar — the pick must still be T1E3, and cwTocar must still be T2E5;
//   2. title-level sheet (no latch, hold on Play) then the agenda stamps
//      "Assistir T2E5" — the pick must NOT inherit T2E5;
//   3. consume once: a second pick does not replay the old latch.
//
// Compiles src/app.c (the shell drops it from the link line, same as
// tests/detail_ep_fonte.c). No window. Both slots start at zero.
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/app.c"

int main(void) {
  int t = -1, e = -1;

  // 1. Latch, then CW overwrites its own slot. The sheet keeps T1E3.
  app_teste_gravar_folha(1, 3, 1);
  app_teste_cw_tocar(2, 5);
  assert(app_teste_consumir_folha(&t, &e));
  assert(t == 1 && e == 3);
  assert(cwTocarT == 2 && cwTocarE == 5);
  puts("ok: sheet latch survives a later Continue Watching stamp");

  // Consumed. A second pick (the next sheet, or none) does not replay it.
  t = e = -1;
  assert(!app_teste_consumir_folha(&t, &e));
  assert(t == -1 && e == -1);
  puts("ok: latch is consumed once");

  // 2. Title-level sheet stores nothing. Agenda's T2E5 stays in cwTocar and
  //    does not become the pick.
  app_teste_gravar_folha(2, 5, 0);
  app_teste_cw_tocar(2, 5);
  t = 0; e = 0;
  assert(!app_teste_consumir_folha(&t, &e));
  assert(t == 0 && e == 0);
  puts("ok: title-level sheet does not inherit a later cwTocar stamp");

  // Cancel an episode sheet, then resume without writing a new latch.
  app_teste_gravar_folha(1, 3, 1);
  stream_folha_abrir();
  SDL_Event back = {0};
  back.type = SDL_KEYDOWN;
  back.key.keysym.sym = SDLK_ESCAPE;
  app_evento(&back);
  assert(!stream_folha_aberta());
  atualizarFolha(0, SDL_GetTicks());
  stream_folha_abrir();
  t = 1; e = 2;
  assert(!app_teste_consumir_folha(&t, &e));
  assert(t == 1 && e == 2);
  puts("ok: canceled sheet cannot override a later resume target");

  puts("detail_ep_fonte_trava: sheet T/E is not the Continue Watching slot");
  return 0;
}
