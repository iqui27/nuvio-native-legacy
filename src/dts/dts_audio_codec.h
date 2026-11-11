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
/* Settings > DTS "Off" (LG): DTS is never converted, so a DTS track is not
 * fed on any path; AC-3/E-AC-3 still keep Dolby Vision. Owner 09/11: Off
 * stops the TrueHD conversion too (the Dolby Vision test path), so a remux
 * with TrueHD plays its same-language AC-3/E-AC-3 in Dolby Vision, and only
 * a TrueHD-only language goes to the TV player (HDR10), like DTS. */
/* dts_off 2: Off and the TV declares DTS (tv.model.edidType): the Dolby
 * Vision path passes the DTS stream through untouched (C3 owner's report 11/11:
 * DTS on that TV fell to HDR10). Experimental: a TV player that refuses the
 * codec ends the session and the TV player takes over, as before. */
static inline int nv_dts_audio_feeds_off(const char *codec,int dv,int dts_off) {
  int dts=!strcmp(codec,"dts") || !strncmp(codec,"A_DTS",5);
  if(dts && dts_off==2 && dv) return 1;
  if(dts_off && dts) return 0;
  if(dts_off && nv_dts_truehd_codec(codec)) return 0;
  return nv_dts_audio_feeds(codec,dv);
}
#endif
