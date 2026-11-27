#ifndef NV_VIDEO_SEEKVIEW_H
#define NV_VIDEO_SEEKVIEW_H
#include <stdint.h>
#include <math.h>
/* UI position only. Playback/subtitle clocks always retain the real samples. */
typedef struct { int active, sent; double target; uint32_t since; } NvSeekView;
static inline void nv_seekview_begin(NvSeekView *s,double target,uint32_t now) {
  *s=(NvSeekView){.active=1,.target=target,.since=now};
}
static inline void nv_seekview_sent(NvSeekView *s,uint32_t now) {
  if(s->active) { s->sent=1;s->since=now; }
}
static inline void nv_seekview_clock(NvSeekView *s,double actual) {
  /* Do not release on a decoder keyframe before the chosen point. */
  if(s->active && s->sent && isfinite(actual) &&
     actual>=s->target-0.05 && actual<=s->target+1.5)s->active=0;
}
static inline double nv_seekview_pos(NvSeekView *s,double actual,uint32_t now) {
  if(s->active && (uint32_t)(now-s->since)>=15000u)s->active=0;
  return s->active?s->target:actual;
}
#endif
