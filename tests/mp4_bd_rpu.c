#include "../src/dts/dv7_mp4.h"
#include "../src/video_mp4_test.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
  uint8_t out[NV_MP4_RPU_MAX];size_t n;
  const uint8_t packet[]={0,0,0,3,0x02,1,0x55,0,0,0,4,0x7c,1,0x19,0xab};
  assert(!nv_mp4_rpu(packet,sizeof packet,4,out,&n) && n==4 && !memcmp(out,packet+11,4));
  assert(nv_mp4_rpu(packet,sizeof packet-1,4,out,&n)<0);
  assert(nv_mp4_rpu(packet,7,4,out,&n)<0);
  assert(nv_mp4_rpu(packet,sizeof packet,0,out,&n)<0);
  assert(nv_mp4_rpu(packet,NV_MP4_PACKET_MAX+1,4,out,&n)<0);
  uint8_t duplicate[16]={0,0,0,4,0x7c,1,0x19,0xab,0,0,0,4,0x7c,1,0x19,0xab};
  assert(nv_mp4_rpu(duplicate,sizeof duplicate,4,out,&n)<0);
  assert(nv_mp4_bd_retry(1,204,1,0,0,0,0,0));
  assert(!nv_mp4_bd_retry(0,204,1,0,0,0,0,0));
  assert(!nv_mp4_bd_retry(1,200,1,0,0,0,0,0));
  assert(!nv_mp4_bd_retry(1,204,0,0,0,0,0,0));
  assert(!nv_mp4_bd_retry(1,204,1,1,0,0,0,0));
  assert(!nv_mp4_bd_retry(1,204,1,0,1,0,0,0));
  assert(!nv_mp4_bd_retry(1,204,1,0,0,1,0,0));
  assert(!nv_mp4_bd_retry(1,204,1,0,0,0,1,0));
  assert(!nv_mp4_bd_retry(1,204,1,0,0,0,0,1));
  puts("PASS bounded EL metadata extraction and startup-only MP4 demux retry policy");
}
