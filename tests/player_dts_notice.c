#include "player_dts_notice.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
  NvDtsNotice s={0};
  assert(!nv_dts_notice_show(&s,0,100)); // Loading/debounce is not a delivery.
  assert(nv_dts_notice_show(&s,1,1600));
  assert(nv_dts_notice_show(&s,1,6599));
  assert(!nv_dts_notice_show(&s,1,6600));
  for(int seek=0;seek<20;seek++) {
    assert(!nv_dts_notice_show(&s,0,7000+seek*2000));
    assert(!nv_dts_notice_show(&s,1,8600+seek*2000));
  }
  s=(NvDtsNotice){0}; // A genuinely new source can announce conversion again.
  assert(nv_dts_notice_show(&s,1,100));
  assert(!nv_dts_notice_show(&s,0,200)); // A seek interrupts even the first notice.
  assert(!nv_dts_notice_show(&s,1,300));
  s=(NvDtsNotice){0};
  assert(!nv_dts_notice_show(&s,0,100)); // Parental/loading overlay waits first.
  assert(nv_dts_notice_show(&s,1,2000));
  assert(!nv_dts_notice_show(&s,0,2100)); // Do not resurrect it after interruption.
  assert(!nv_dts_notice_show(&s,1,4000));
  s=(NvDtsNotice){0};
  assert(nv_dts_notice_show(&s,1,UINT32_MAX-2000));
  assert(nv_dts_notice_show(&s,1,2000));
  assert(!nv_dts_notice_show(&s,1,3000));
  { /* Owner 09/11: DTS, AC-3, DTS again announces again; seeks keep the track. */
    int last=-1;
    s=(NvDtsNotice){0};
    nv_dts_notice_track(&s,&last,14); assert(nv_dts_notice_show(&s,1,100) && !nv_dts_notice_show(&s,1,5200));
    nv_dts_notice_track(&s,&last,14); assert(!nv_dts_notice_show(&s,1,9000));   /* same track (seek) */
    nv_dts_notice_track(&s,&last,-1); assert(!nv_dts_notice_show(&s,1,9100));   /* loading: unknown */
    nv_dts_notice_track(&s,&last,9);  assert(!nv_dts_notice_show(&s,0,9200));   /* AC-3: not converted */
    nv_dts_notice_track(&s,&last,14); assert(nv_dts_notice_show(&s,1,12000));    /* DTS again */
    assert(nv_dts_notice_track(&s,&last,16) && nv_dts_notice_show(&s,1,20000));  /* another DTS track */
    assert(!nv_dts_notice_track(&s,&last,16)); }
  puts("DTS converted notice: once per source, bounded duration, seeks/reloads/interruption and tick wrap passed");
}
