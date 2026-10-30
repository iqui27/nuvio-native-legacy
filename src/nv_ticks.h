#ifndef NV_TICKS_H
#define NV_TICKS_H
#include <stdint.h>
#include <time.h>
/* Monotonic milliseconds. Not SDL_GetTicks64: webOS 4 ships SDL < 2.0.18
 * without it, and the lazy-bound call killed playback on the C9. */
static inline uint64_t nv_ticks64(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (uint64_t)t.tv_sec * 1000u + (uint64_t)t.tv_nsec / 1000000u;
}
#endif
