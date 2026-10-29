// Isolated source-list fixtures have no active playback pipeline.
#ifndef NV_TEST_STUB_PLAYER_SOURCE_ACTIONS_H
#define NV_TEST_STUB_PLAYER_SOURCE_ACTIONS_H
#include "player.h"
#include <stdlib.h>
unsigned player_recarregar_contexto(void) { return 0; }
int player_recarregar_fonte(unsigned contexto) {
  (void)contexto;
  abort(); // A disabled source action must never request playback work.
}
#endif
