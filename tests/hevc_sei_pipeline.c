#define _POSIX_C_SOURCE 200809L
#include "dts/dts_pipeline.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
static int recognized,removed,drop;
static void event(void *ctx,const char *json) {
  (void)ctx;
  if(strstr(json,"hevc-sei") && strstr(json,"found=1")){recognized++;removed+=strstr(json,"removed=1")!=NULL;}
  if(strstr(json,"hevc-sei-drop"))drop++;
}
int main(int argc,char **argv) {
  assert(argc==2);assert(!setenv("NUVIO_DTS_ADAPTER_DIR",argv[1],1));
  assert(dts_pipeline_available(0));
  void *native=dlopen("libplayerAPIs.so",RTLD_NOW|RTLD_LOCAL);assert(native);
  int (*plays)(void);*(void **)(&plays)=dlsym(native,"dts_fixture_play_calls");assert(plays);
  DtsPipeline *p=dts_pipeline_create("app","\"window",0,event,NULL);assert(p);
  DtsMediaInfo info={0};strcpy(info.video_codec,"hevc");strcpy(info.audio_codec,"aac");
  info.width=1920;info.height=1080;info.channels=2;info.sample_rate=48000;
  assert(dts_pipeline_load(p,&info,2.5));assert(dts_pipeline_play(p));
  const unsigned char sei[]={0,0,1,78,1,4,7,0xb5,0,0x3c,0,1,4,1,128};
  const unsigned char picture[]={0,0,1,40,1,0xaa,0xbb,128,0,0,0,1,124,1,0x44,128};
  DtsFrame f={0};f.kind=DTS_VIDEO;f.pts_ns=3000000000LL;f.data=sei;f.size=sizeof sei;
  // Metadata-only removal must not call native Feed or satisfy video preroll.
  assert(dts_pipeline_feed(p,&f)==1 && plays()==0 && drop==1);
  unsigned char packet[sizeof sei+sizeof picture],saved[sizeof packet];
  memcpy(packet,sei,sizeof sei);memcpy(packet+sizeof sei,picture,sizeof picture);memcpy(saved,packet,sizeof packet);
  f.data=packet;f.size=sizeof packet;assert(!setenv("DTS_SEI_REFUSE","1",1));
  assert(dts_pipeline_feed(p,&f)==0 && plays()==0);
  assert(dts_pipeline_feed(p,&f)==1 && plays()==0);
  assert(!memcmp(packet,saved,sizeof packet));
  unsigned char audio[]={2,3,4,5};f.kind=DTS_AUDIO;f.data=audio;f.size=sizeof audio;
  assert(dts_pipeline_feed(p,&f)==1 && plays()==1);
  assert(recognized && removed);
  // The existing native seek acknowledgement and A/V preroll remain intact.
  assert(dts_pipeline_flush(p,5));assert(dts_pipeline_play(p));
  f.kind=DTS_VIDEO;f.data=packet;f.size=sizeof packet;assert(dts_pipeline_feed(p,&f)==1);
  f.kind=DTS_AUDIO;f.data=audio;f.size=sizeof audio;assert(dts_pipeline_feed(p,&f)==1 && plays()==2);
  dts_pipeline_destroy(p);dlclose(native);
  puts("PASS native HEVC SEI adapter: exact firmware bytes, unchanged input/audio/RPU, metadata-only drop, BufferFull retry and seek preroll");
}
