/* Actual HTTP transport with injected failures: exact bytes and queue ownership. */
#define NV_DTS_FFMPEG
#ifndef NV_TEST_LEGACY_RECOVERY
#define NV_DTS_WEBOS_RECOVERY
#endif
#include "../src/dts/dts_engine.c"
#include <assert.h>
#include <unistd.h>
static void bytes(DtsEngine *e,int amount) {
 uint8_t b[65536];
 while(amount>0) {
  int64_t at=e->pos;int n=read_range(e,b,amount<(int)sizeof b?amount:(int)sizeof b);assert(n>0);
  for(int i=0;i<n;i++) assert(b[i]==(unsigned char)((at+i)%251));amount-=n;
 }
}
static DtsRangeJob *job_at(DtsEngine *e,int64_t at) {
 for(int i=0;i<RANGE_SLOTS;i++) if(e->ranges[i].state && e->ranges[i].start==at) return &e->ranges[i];
 return NULL;
}
static void *cancel_later(void *e) {usleep(50000);dts_engine_cancel(e);return NULL;}
int main(int argc,char **argv) {
 assert(argc==2);DtsEngine *e=dts_engine_create();assert(e);assert(!e->allow_p7);e->range_slots=8;
 e->url=strdup(argv[1]);e->headers=strdup("Authorization: Bearer engine-test\nX-Fixture: yes");
 if(strstr(argv[1],"dv.bin")){e->dv=1;e->range_bytes=RANGE_BYTES_DV;}
 int64_t fault_at=e->dv?RANGE_BYTES_DV:4*1024*1024;
 uint64_t begun=clock_ns(CLOCK_MONOTONIC);
 if(strstr(argv[1],"retry-live-")) {
  int boot=strstr(argv[1],"live-boot")!=NULL;
  __atomic_store_n(&e->range_live,!boot,__ATOMIC_RELAXED);
  bytes(e,65536);
  if(strstr(argv[1],"live-speculative")) {
   uint64_t until=clock_ns(CLOCK_MONOTONIC)+UINT64_C(11000000000);int done=0;
   do {
    pthread_mutex_lock(&e->range_lock);DtsRangeJob *j=job_at(e,fault_at);done=j && j->state==3 && j->data;
    pthread_mutex_unlock(&e->range_lock);if(!done)usleep(10000);
   } while(!done && clock_ns(CLOCK_MONOTONIC)<until);assert(done);
  }
  if(strstr(argv[1],"live-cancel")) {
   bytes(e,4*1024*1024-65536);pthread_t t;assert(!pthread_create(&t,NULL,cancel_later,e));uint8_t b;
   uint64_t t0=clock_ns(CLOCK_MONOTONIC);assert(read_range(e,&b,1)==AVERROR_EXIT);pthread_join(t,NULL);
   assert(clock_ns(CLOCK_MONOTONIC)-t0<UINT64_C(1500000000));
  } else bytes(e,(int)(9LL*e->range_bytes)-65536);
  double seconds=(clock_ns(CLOCK_MONOTONIC)-begun)/1e9;
  if(strstr(argv[1],"live-hangall")) assert(seconds>=8 && seconds<16);
  else if(strstr(argv[1],"live-hang")||strstr(argv[1],"live-bodyhang")) assert(seconds<6);
  else if(strstr(argv[1],"live-steady")) assert(seconds>=4 && seconds<9);
  else if(strstr(argv[1],"live-speculative")) assert(seconds>=7);
  else if(boot) assert(seconds>=3 && seconds<7);
  printf("PASS: live range elapsed=%.3f s\n",seconds);
 } else if(strstr(argv[1],"retry-cancel")) {
  pthread_t t;assert(!pthread_create(&t,NULL,cancel_later,e));uint8_t b;
  assert(read_range(e,&b,1)==AVERROR_EXIT);pthread_join(t,NULL);
  assert(clock_ns(CLOCK_MONOTONIC)-begun<UINT64_C(1500000000));
 } else if(strstr(argv[1],"retry-hard") || strstr(argv[1],"retry-exhaust")) {
  uint8_t b=77;assert(read_range(e,&b,1)==AVERROR(EIO));assert(e->pos==0&&b==77);
 } else {
  bytes(e,65536);
  uint64_t until=clock_ns(CLOCK_MONOTONIC)+UINT64_C(5000000000);
  char *saved=NULL;int slot=-1;
  /* Observe a successful later range while the failed range remains in flight. */
  while(clock_ns(CLOCK_MONOTONIC)<until) {
   pthread_mutex_lock(&e->range_lock);
   DtsRangeJob *fault=job_at(e,fault_at);
   for(int i=0;i<RANGE_SLOTS;i++) if(e->ranges[i].start>fault_at && e->ranges[i].state==3 && e->ranges[i].data && fault && fault->state==2) {saved=e->ranges[i].data;slot=i;break;}
   pthread_mutex_unlock(&e->range_lock);if(saved)break;usleep(1000);
  }
  /* Small malformed/truncated responses can recover before this observation;
   * delayed 503 tests must prove that future buffers really stay allocated. */
  if(strstr(argv[1],"retry-once")||strstr(argv[1],"retry-twice")||strstr(argv[1],"retry-seek")) assert(saved);
  if(strstr(argv[1],"retry-seek")) {
   assert(seek_range(e,12*1024*1024+17,SEEK_SET)==12*1024*1024+17);bytes(e,65536);
   assert(clock_ns(CLOCK_MONOTONIC)-begun<UINT64_C(2500000000));
  } else {
   int ready=0;
   do {
    pthread_mutex_lock(&e->range_lock);ready=0;
    for(int i=0;i<e->range_slots;i++) ready+=e->ranges[i].state==3&&e->ranges[i].data!=NULL;
    if(saved) assert(e->ranges[slot].data==saved);
    pthread_mutex_unlock(&e->range_lock);if(ready<e->range_slots)usleep(1000);
   } while(ready<e->range_slots && clock_ns(CLOCK_MONOTONIC)<until);
   assert(ready==e->range_slots);
   assert(e->range_next==9LL*e->range_bytes);
   bytes(e,(int)(9LL*e->range_bytes)-65536);
  }
 }
 dts_engine_destroy(e);puts("PASS: range retry fixture completed");
}
