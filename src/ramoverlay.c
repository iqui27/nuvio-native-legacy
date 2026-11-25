#ifdef NV_HTTP_BUFFER_DIAG
#include "ramoverlay.h"
#include "memlog.h"
#include "gfx.h"
#include "text.h"
#include <stdio.h>

void ramoverlay_desenhar(uint32_t now, int enabled) {
  static int sampled;
  static uint32_t sampled_at;
  static MemlogInfo memory;
  static char line1[128], line2[128];
  if (!enabled) { sampled = 0; return; }
  if (!sampled || (uint32_t)(now - sampled_at) >= 1000u) {
    memlog_ler(&memory);
    sampled_at = now; sampled = 1;
    if (memory.rss_kib >= 0 && memory.peak_kib >= 0)
      snprintf(line1, sizeof line1, "RAM app (RSS): %.1f MiB | peak: %.1f MiB",
               memory.rss_kib / 1024.0, memory.peak_kib / 1024.0);
    else snprintf(line1, sizeof line1, "RAM app (RSS): unavailable");
    if (memory.available_kib >= 0 && memory.total_kib > 0)
      snprintf(line2, sizeof line2, "System available: %.0f / %.0f MiB",
               memory.available_kib / 1024.0, memory.total_kib / 1024.0);
    else snprintf(line2, sizeof line2, "System available: unavailable");
  }
  TxtLinha l1 = txt_linha(TXT_G19M, line1, 240, 240, 240, 255);
  TxtLinha l2 = txt_linha(TXT_G19M, line2, 220, 220, 220, 255);
  float width = (l1.w > l2.w ? l1.w : l2.w) + 32.0f;
  float height = l1.h + l2.h + 28.0f;
  gfx_cor((GfxRect){40.0f, 124.0f, width, height}, 0.12f, 0, 0, 0, 0.78f);
  txt_desenhar_alpha(l1, 56.0f, 136.0f, 1.0f);
  txt_desenhar_alpha(l2, 56.0f, 140.0f + l1.h, 1.0f);
}

#endif
