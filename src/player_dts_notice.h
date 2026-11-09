#ifndef NV_PLAYER_DTS_NOTICE_H
#define NV_PLAYER_DTS_NOTICE_H
#include <stdint.h>
/* One converted-audio notice per source, including across seeks, reloads and
 * temporary loading states. A new source owns a fresh zeroed state. */
typedef struct { int shown, active; uint32_t started; } NvDtsNotice;
static inline int nv_dts_notice_show(NvDtsNotice *s,int eligible,uint32_t now) {
  if(!eligible) { s->active=0;return 0; }
  if(!s->shown) { s->shown=s->active=1;s->started=now; }
  return s->active && now-s->started<5000u;
}
/* Another audio track picked by hand is a new announcement (owner 09/11: DTS,
 * then AC-3, then DTS again said nothing; one DTS track to another DTS one
 * too). Seeks and reloads keep the track,
 * so they still announce once. -1: unknown (loading), nothing changes. */
static inline int nv_dts_notice_track(NvDtsNotice *s,int *last,int track) {
  if(track<0 || track==*last) return 0;
  *last=track; *s=(NvDtsNotice){0};
  return 1;   /* the caller restarts its settle wait (the new track loads first) */
}
#endif
