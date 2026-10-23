#include "perftv.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  PerfTv p = {0};
  char line[320], small[8];
  /* Changing aggregation, the strict >33 cutoff, or nearest-rank percentiles
     must break this real formatter/aggregator check. */
  assert(!perftv_frame(&p, "home", 10, 1, 2, 3, 4, line, sizeof line));
  assert(!perftv_frame(&p, "home", 33, 3, 4, 5, 6, line, sizeof line));
  assert(!perftv_frame(&p, "home", 40, 5, 6, 7, 8, line, sizeof line));
  assert(perftv_frame(&p, "detail", 20, 0, 0, 0, 0, line, sizeof line));
  assert(!strcmp(line, "[perf-tv] screen=home frames=3 p50_ms=33.00 p95_ms=40.00 max_ms=40.00 over33=1 uploads_pf=3.000 text_pf=4.000 blur_pf=5.000 glass_pf=6.000"));
  assert(p.frames.n == 1 && !strcmp(p.screen, "detail"));
  assert(perftv_flush(&p, line, sizeof line));
  assert(strstr(line, "screen=detail frames=1") && strstr(line, "p95_ms=20.00"));
  assert(!perftv_flush(&p, line, sizeof line));
  assert(!perftv_frame(&p, "settings", 2999, 0, 0, 0, 0, line, sizeof line));
  assert(perftv_frame(&p, "settings", 1, 0, 0, 0, 0, line, sizeof line));
  assert(strstr(line, "frames=2") && strstr(line, "max_ms=2999.00 over33=1"));
  for (int i = 0; i < FPS_MAX; i++) {
    int emitted = perftv_frame(&p, "home", 0.1, 1, 0, 0, 0, line, sizeof line);
    assert(emitted == (i == FPS_MAX - 1));
  }
  assert(strstr(line, "frames=1200") && strstr(line, "uploads_pf=1.000"));
  perftv_frame(&p, "home", 20, 0, 0, 0, 0, line, sizeof line);
  assert(perftv_flush(&p, small, sizeof small) && small[7] == '\0');
  puts("perf-tv formatter/aggregator: PASS");
  return 0;
}
