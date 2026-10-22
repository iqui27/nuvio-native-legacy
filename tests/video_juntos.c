// Exercise the production video contract without opening media or networking.
#include "video.h"
#include "cacheboost.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>

__attribute__((weak)) void video_taxa_sync(int rate) {
  (void)rate; assert(!"Missing separate sync multiplier");
}
__attribute__((weak)) int video_taxa_efetiva(void) { return -1; }
__attribute__((weak)) double video_pos_amostra(Uint64 *stamp) {
  (void)stamp; assert(!"Missing native position sample"); return -1;
}

int cacheboost_ganho_estado(void) { return 0; }

int main(void) {
  Uint64 stamp, before, after;
  video_velocidade(150);
  video_taxa_sync(950);
  assert(video_velocidade_atual() == 150);
  assert(video_taxa_efetiva() == 143);
  video_taxa_sync(1050);
  assert(video_taxa_efetiva() == 158);
  video_taxa_sync(1000);
  assert(video_taxa_efetiva() == 150);
  video_taxa_sync(0);
  assert(video_taxa_efetiva() == 150);
  video_velocidade(100);
  video_taxa_sync(950);
  assert(video_taxa_efetiva() == 95);
  video_taxa_sync(1000);
  assert(video_taxa_efetiva() == 100);

  VideoSimulacao sim = {0};
  sim.pos = 12.5; sim.pronto = 1;
  before = SDL_GetTicks64();
  video_simular(&sim);
  after = SDL_GetTicks64();
  assert(fabs(video_pos_amostra(&stamp) - 12.5) < 1e-9);
  assert(stamp >= before && stamp <= after);
  SDL_Delay(10);
  Uint64 same;
  assert(video_pos_amostra(&same) == 12.5 && same == stamp);
  before = SDL_GetTicks64();
  video_simular(&sim); // Equal values still represent a fresh sample.
  assert(video_pos_amostra(&same) == 12.5 && same >= before && same > stamp);
  assert(video_pos_amostra(NULL) == 12.5);
  video_velocidade_recusada();
  assert(video_taxa_efetiva() == 100);
  puts("video_juntos: sample age, equal samples, personal speed and sync multiplier OK");
  return 0;
}
