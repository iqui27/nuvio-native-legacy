#include "perftv.h"
#include <stdio.h>
#include <string.h>

int perftv_enabled;
int perftv_uploads, perftv_blur, perftv_glass;

int perftv_flush(PerfTv *p, char *out, size_t cap) {
  int n = p->frames.n;
  if (!n) return 0;
  snprintf(out, cap, "[perf-tv] screen=%s frames=%d p50_ms=%.2f p95_ms=%.2f max_ms=%.2f over33=%d uploads_pf=%.3f text_pf=%.3f blur_pf=%.3f glass_pf=%.3f",
           p->screen, n, fps_percentil(&p->frames, 50), fps_percentil(&p->frames, 95),
           fps_pior(&p->frames), p->over33, (double)p->frames.upl / n,
           (double)p->text / n, (double)p->blur / n, (double)p->glass / n);
  memset(p, 0, sizeof *p);
  return 1;
}

int perftv_frame(PerfTv *p, const char *screen, double ms, int uploads,
                 int text, int blur, int glass, char *out, size_t cap) {
  int emitted = 0;
  if (p->frames.n && strcmp(p->screen, screen)) emitted = perftv_flush(p, out, cap);
  if (!p->frames.n) snprintf(p->screen, sizeof p->screen, "%s", screen);
  fps_add(&p->frames, ms, uploads);
  p->elapsed += ms;
  p->over33 += ms > 33.0;
  p->text += text; p->blur += blur; p->glass += glass;
  // A transition emits the previous window first; keep this new sample.
  if (!emitted && (p->elapsed >= 3000 || p->frames.n == FPS_MAX))
    emitted = perftv_flush(p, out, cap);
  return emitted;
}
