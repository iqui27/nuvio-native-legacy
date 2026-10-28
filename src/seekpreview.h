#ifndef NV_SEEKPREVIEW_H
#define NV_SEEKPREVIEW_H
#include <math.h>
#include <stdint.h>

// Half the on-device ten-second grid. Sparse stand-ins remain approximate.
#define SEEKPREVIEW_MAX_S 5.0
typedef struct {
  double cursor, pts;
  uint32_t when;
  unsigned generation;
  int stale, shown, direct;
} SeekPreview;

static inline double seekpreview_confirmar(const SeekPreview *p, double target,
                                          double duration, unsigned generation,
                                          uint32_t now) {
  if (!p->shown || !p->direct || p->stale || p->generation != generation ||
      (uint32_t)(now - p->when) > 250u || !isfinite(target) ||
      !isfinite(p->pts) || !isfinite(p->cursor) || !isfinite(duration) ||
      p->pts < 0 || p->pts > duration || fabs(p->cursor - target) > 0.5 ||
      fabs(p->pts - target) > SEEKPREVIEW_MAX_S) return target;
  return p->pts;
}
#endif
