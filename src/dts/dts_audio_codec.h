#ifndef NV_DTS_AUDIO_CODEC_H
#define NV_DTS_AUDIO_CODEC_H
#include <string.h>

/* Which source audio the converter session feeds. Adapted from Matis94
 * 115f3a0c0 (TrueHD), cba4365a0/f3cf69b35 (DTS Off). Flags per session:
 *   dv     - Dolby Vision path (AC-3/E-AC-3 pass through or, fixed, convert)
 *   truehd - TrueHD may be decoded and re-encoded (DV path only)
 *   off    - DTS row Off: neither DTS nor TrueHD feed, on any path. */
static inline int nv_dts_truehd_codec(const char *c) {
  return c && (!strcmp(c, "truehd") || !strcmp(c, "A_TRUEHD"));
}
static inline int nv_dts_audio_feeds(const char *c, int dv, int truehd, int off) {
  if (!c) return 0;
  if (!strcmp(c, "dts")) return !off;
  if (nv_dts_truehd_codec(c)) return dv && truehd && !off;
  return dv && (!strcmp(c, "ac3") || !strcmp(c, "eac3"));
}
#endif
