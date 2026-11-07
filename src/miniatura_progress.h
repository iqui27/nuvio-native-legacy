#ifndef MINIATURA_PROGRESS_H
#define MINIATURA_PROGRESS_H
#include <stdint.h>
typedef struct { unsigned generation, seen; int active; uint32_t since; } MiniProgress;
static inline int mini_progress(MiniProgress *p, unsigned generation, int exists,
                                int half, int done, int cached, uint32_t now) {
  if (!exists) { *p = (MiniProgress){0}; return 0; }
  if (p->generation != generation) {
    *p = (MiniProgress){.generation=generation};
    if (cached) {
      // Cached milestones are not new work. Partial sets may still finish later.
      p->seen = cached == 2 ? 7u : 1u | (half ? 2u : 0u);
      p->active=4; p->since=now; return 4;
    }
  }
  if (p->active && (uint32_t)(now-p->since) < 4000u) return p->active;
  p->active=0;
  unsigned available=1u | (half ? 2u : 0u) | (done ? 4u : 0u);
  for (int i=0;i<3;i++) if ((available & (1u<<i)) && !(p->seen & (1u<<i))) {
    p->seen |= 1u<<i; p->active=i+1; p->since=now; return p->active;
  }
  return 0;
}
#endif
