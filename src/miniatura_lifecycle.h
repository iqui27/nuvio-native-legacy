#ifndef MINIATURA_LIFECYCLE_H
#define MINIATURA_LIFECYCLE_H

#include <stdint.h>

typedef struct { uint32_t pausedSince; int waiting, attempted; } MiniPauseAdmission;

// Refining an existing hardware session allocates no additional source decoder.
// Keep the normal floor; retain extra headroom when hardware is absent.
static inline int mini_refine_memory_ok(long availableKB, long floorKB, int ready) {
  return availableKB < 0 || availableKB >= floorKB + (ready ? 0 : 100L * 1024);
}

// A scrub/buffer transition is never a creation window. One attempt per genuine
// pause: failures/releases cannot cause a retry loop while playback runs or rests.
static inline int mini_pause_admit(MiniPauseAdmission *p, uint32_t now,
                                  int confirmed, int scrubbing, int buffering) {
  if (!confirmed || scrubbing || buffering) {
    p->waiting = 0;
    if (!confirmed && !scrubbing && !buffering) p->attempted = 0;
    return 0;
  }
  if (!p->waiting) { p->pausedSince = now; p->waiting = 1; }
  if (p->attempted || (uint32_t)(now - p->pausedSince) < 2000u) return 0;
  p->attempted = 1;
  return 1;
}
#endif
