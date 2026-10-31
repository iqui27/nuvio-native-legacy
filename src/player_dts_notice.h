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
#endif
