// Real shared decoder; expose queue readiness without guessing worker timing.
#define NV_ANDROID
#include "../src/gif.c"
int gifperfis_ready(void) {
  GifFaixa frame;
  return gif_fio_pegar(fio, &frame);
}
int gifperfis_started(void) { return mostrou; }
