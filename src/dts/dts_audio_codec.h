#ifndef NV_DTS_AUDIO_CODEC_H
#define NV_DTS_AUDIO_CODEC_H
#include <string.h>

/* Experimental DV audio conversion, kept out of normal release routing. */
static inline int nv_dts_truehd_codec(const char *codec) {
#ifdef NV_DV_TRUEHD_TEST
  return !strcmp(codec,"truehd") || !strcmp(codec,"A_TRUEHD");
#else
  (void)codec;return 0;
#endif
}
static inline int nv_dts_audio_feeds(const char *codec,int dv) {
  return !strcmp(codec,"dts") || (dv && (!strcmp(codec,"ac3") ||
         !strcmp(codec,"eac3") || nv_dts_truehd_codec(codec)));
}
#endif
