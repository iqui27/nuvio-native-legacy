/* Exercise the production Android reveal gate with delayed native layout. */
#include "trailer.h"
#include "video.h"
#include <assert.h>
static int applied;
int video_janela_pronta(void) { return applied; }
#undef __APPLE__
#include "../src/trailer.c"
int main(void) {
  trailer_no_cartao(1);
  assert(!trailer_mostra_video());
  applied = 1;
  assert(trailer_mostra_video());
  applied = 0; /* A second move invalidates the previous layout. */
  assert(!trailer_mostra_video());
  trailer_no_cartao(0);
  assert(trailer_mostra_video()); /* Hero/detail behavior stays unchanged. */
  puts("ok Android card waits for native layout; other destinations unchanged");
  return 0;
}
