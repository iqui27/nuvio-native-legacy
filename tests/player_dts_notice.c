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
  puts("DTS converted notice: once per source, bounded duration, seeks/reloads/interruption and tick wrap passed");
}
