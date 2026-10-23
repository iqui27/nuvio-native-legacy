#include "dts/dts_engine.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
static void nap(void) { struct timespec t={0,10000000};nanosleep(&t,NULL); }
int main(int argc,char **argv) {
  assert(argc==2);
  const int64_t MiB=1048576;
  for(int mib=64;mib<=256;mib*=2) {
    dts_engine_set_ahead_mib(mib);
    DtsEngine *e=dts_engine_create();assert(e);
    dts_engine_dv(e,0);dts_engine_allow_p7(e,0);
    assert(dts_engine_open(e,argv[1],"Authorization: Bearer engine-test\nX-Fixture: yes",-1,0,0)==0);
    const DtsMediaInfo *media=dts_engine_info(e);
    assert(media && !strcmp(media->video_codec,"h264") && !strcmp(media->audio_codec,"aac"));
    DtsBufferInfo b={0};int full=0,gap=0;
    for(int i=0;i<1000;i++) {
      dts_engine_buffer_info(e,&b);
      assert(b.total==300*MiB && b.capacity_bytes==(mib/4+1)*(4*MiB-1));
      assert(b.ready_bytes>=0 && b.ready_bytes<=b.capacity_bytes);
      assert(b.contiguous_bytes>=0 && b.contiguous_bytes<=b.ready_bytes);
      if(b.ready_bytes>b.contiguous_bytes+4*MiB)gap=1;
      if(b.ready_bytes>=(mib-4)*MiB && b.contiguous_bytes==b.ready_bytes) { full=1;break; }
      nap();
    }
    assert(full && gap); // Real prefetch and an out-of-order completed queue.
    printf("PASS SDR/DTS P7-off: %d MiB ahead, ready=%lld, active-plus-queue=%lld, gap reporting verified\n",
           mib,(long long)b.ready_bytes,(long long)b.capacity_bytes);
    assert(dts_engine_seek(e,1.0)==0);
    DtsFrame frame;assert(dts_engine_next(e,&frame)>0);
    dts_engine_buffer_info(e,&b);
    assert(b.ready_bytes>=0 && b.ready_bytes<=b.capacity_bytes);
    dts_engine_destroy(e);
  }
  dts_engine_set_ahead_mib(0);
  return 0;
}
