/* Real loopback HTTP, using the same readers installed in conversion AVIO. */
#define NV_DTS_FFMPEG
#define NV_DTS_WEBOS_RECOVERY
#ifndef C1_ENGINE
#define C1_ENGINE "../src/dts/dts_engine.c"
#endif
#include C1_ENGINE
#include <assert.h>
#include <unistd.h>
#ifdef C1_BASELINE
/* Old engine has no controls: request counts must still fail the oracle. */
static void dts_engine_set_http_workers(int n) { (void)n; }
static void dts_engine_set_source_workers(int n) { (void)n; }
#endif
static void *cancel_soon(void *p) { usleep(100000); dts_engine_cancel(p); return NULL; }
static void compare(DtsEngine *e, const char *path, int native) {
 FILE *f=fopen(path,"rb"); assert(f);
 unsigned char got[65536],want[65536]; int64_t at=0;
 while(at<20*1024*1024) {
  int n=native?dts_engine_source_read(e,at,got,sizeof got,-1):e->io->read_packet(e,got,sizeof got);
  assert(n>0 && n<=(int)sizeof got);
  assert(fread(want,1,(size_t)n,f)==(size_t)n && !memcmp(got,want,(size_t)n)); at+=n;
 }
 fclose(f);
}
int main(int argc,char **argv) {
 assert(argc==6); const char *mode=argv[1],*url=argv[2]; int workers=atoi(argv[4]),ahead=atoi(argv[5]);
 dts_engine_set_source_workers(workers); dts_engine_set_http_workers(workers);
 DtsEngine *e=NULL; uint64_t begun=clock_ns(CLOCK_MONOTONIC);
 if(!strcmp(mode,"native")) {
  e=dts_engine_source_create(url,"Authorization: Bearer c1-private-sentinel\nX-Fixture: c1-config-sentinel",64); assert(e);
  compare(e,argv[3],1);
 } else if(!strcmp(mode,"conversion")) {
  e=dts_engine_create(); assert(e); dts_engine_set_ahead_mib(ahead);
  assert(!dts_engine_open(e,url,"Authorization: Bearer c1-private-sentinel\nX-Fixture: c1-config-sentinel",-1,0,0));
#ifndef C1_BASELINE
  assert(e->http_workers==workers);
  assert(e->io->read_packet==(workers==1?read_fluxo:read_range));
  if(workers==1) assert(e->fx_cap==16*1024*1024);
  /* Range queue RAM stays inside Settings > Buffer size, whatever the lanes. */
  else assert((int64_t)e->range_slots*e->range_bytes<=(int64_t)ahead*1024*1024);
#endif
  assert(e->io->seek(e,0,SEEK_SET)==0); compare(e,argv[3],0);
 } else {
  /* SDR range recovery and streaming recovery share real rede.c transport. */
  e=dts_engine_create(); assert(e); e->url=strdup(url); e->headers=strdup("Authorization: Bearer c1-private-sentinel");
  e->range_slots=4; e->range_bytes=65536;
#ifndef C1_BASELINE
  e->http_workers=workers;
#endif
  int stream=strstr(mode,"stream")!=NULL;
  if(stream) {
#ifndef C1_BASELINE
   e->conv_stream=1;
#endif
   e->fx_cap=16*1024*1024; e->fx_req=-1;
  }
  unsigned char b[1024];
  if(strstr(mode,"cancel")) {
   pthread_t t; assert(!pthread_create(&t,NULL,cancel_soon,e));
   assert((stream?read_fluxo(e,b,sizeof b):read_range(e,b,sizeof b))==AVERROR_EXIT);
   pthread_join(t,NULL); assert(clock_ns(CLOCK_MONOTONIC)-begun<UINT64_C(2000000000));
  } else if(strstr(mode,"exhaust")) {
   assert((stream?read_fluxo(e,b,sizeof b):read_range(e,b,sizeof b))==AVERROR(EIO));
   assert(clock_ns(CLOCK_MONOTONIC)-begun<UINT64_C(45000000000));
  } else {
   /* First resolver response caches CDN; next request expires, then renews. */
   int n=stream?read_fluxo(e,b,sizeof b):read_range(e,b,sizeof b); assert(n>0);
   for(int i=0;i<n;i++) assert(b[i]==(unsigned char)(i%251));
   if(stream) { assert(seek_fluxo(e,8*1024*1024,SEEK_SET)==8*1024*1024); }
   else { assert(seek_range(e,8*1024*1024,SEEK_SET)==8*1024*1024); }
   n=stream?read_fluxo(e,b,sizeof b):read_range(e,b,sizeof b); assert(n>0);
   for(int i=0;i<n;i++) assert(b[i]==(unsigned char)((8*1024*1024+i)%251));
  }
 }
 dts_engine_cancel(e); dts_engine_destroy(e);
 DtsEngineRings rings; dts_engine_rings(&rings,0); assert(!rings.alive && !rings.bytes);
 assert(clock_ns(CLOCK_MONOTONIC)-begun<UINT64_C(45000000000));
 printf("PASS: %s workers=%d\n",mode,workers); return 0;
}
