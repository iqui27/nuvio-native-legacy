/* Exercise the actual AVIO byte transport without decoder timing obscuring it. */
#define NV_DTS_FFMPEG
#ifndef NV_TEST_LEGACY_RECOVERY
#define NV_DTS_WEBOS_RECOVERY
#endif
#include "../src/dts/dts_engine.c"
#include <assert.h>
#include <unistd.h>
static void check_bytes(DtsEngine *e,int amount) {
  uint8_t bytes[65536];
  while(amount>0) {
    int n=amount>(int)sizeof bytes?(int)sizeof bytes:amount;
    int64_t offset=e->pos;
    int r=read_range(e,bytes,n); assert(r>0);
    for(int i=0;i<r;i++) assert(bytes[i]==(unsigned char)((offset+i)%251));
    amount-=r;
  }
}
static void *cancel_read(void *opaque) { usleep(200000); dts_engine_cancel(opaque); return NULL; }
/* Model native backpressure by leaving the consumer idle. A delayed range
 * beyond the old four-slot frontier must finish without any further reads. */
static void check_prefetch(DtsEngine *e) {
  check_bytes(e,65536);
  uint64_t deadline=clock_ns(CLOCK_MONOTONIC)+UINT64_C(4000000000);
  int ready=0;
  do {
    usleep(10000);
    pthread_mutex_lock(&e->range_lock);
    int active=0, occupied=0;
    long payload=0;
    ready=0;
    for(int i=0;i<RANGE_SLOTS;i++) {
      DtsRangeJob *job=&e->ranges[i];
      occupied+=job->state!=0;
      active+=job->state==2;
      if(job->state==3) { ready++; payload+=job->count; }
    }
    assert(active<=RANGE_WORKERS && occupied<=RANGE_SLOTS);
    assert(payload + e->cache_size <= (int64_t)(e->range_slots+1)*e->range_bytes);
    pthread_mutex_unlock(&e->range_lock);
  } while(ready<e->range_slots && clock_ns(CLOCK_MONOTONIC)<deadline);
  assert(ready==e->range_slots);
  assert(e->range_next==(int64_t)(e->range_slots+1)*e->range_bytes);
  uint64_t blocked=e->metrics.range_blocked_ns;
  check_bytes(e,9*RANGE_BYTES-65536);
  assert(e->metrics.range_blocked_ns-blocked<UINT64_C(200000000));
  assert(seek_range(e,40*RANGE_BYTES+17,SEEK_SET)==40*RANGE_BYTES+17);
  check_bytes(e,65536);
  pthread_mutex_lock(&e->range_lock);
  for(int i=0;i<RANGE_SLOTS;i++)
    assert(!e->ranges[i].state || e->ranges[i].start>=40*RANGE_BYTES+17);
  pthread_mutex_unlock(&e->range_lock);
  dts_engine_cancel(e);
  assert(read_range(e,(uint8_t[1]){0},1)==AVERROR_EXIT);
  dts_engine_destroy(e);
  fprintf(stderr,"Idle-consumer prefetch, delayed future range, bounded queue and seek/cancel passed (lanes=%d).\n",RANGE_WORKERS);
}
int main(int argc,char **argv) {
  assert(argc==2);
  DtsEngine *e=dts_engine_create(); assert(e); dts_engine_allow_p7(e,1);
  if (strstr(argv[1],"dv-prefetch")) {
    e->dv=1; e->range_bytes=RANGE_BYTES_DV; e->range_slots=RANGE_SLOTS_DV;
  }
  e->url=strdup(argv[1]); e->headers=strdup("Authorization: Bearer engine-test\nX-Fixture: yes");
  if(strstr(argv[1],"/prefetch/")) { check_prefetch(e); return 0; }
  uint64_t begun=clock_ns(CLOCK_MONOTONIC);
  check_bytes(e,65536);
  int64_t frontier=e->range_next;
  int tokens[RANGE_SLOTS];
  for(int i=0;i<RANGE_SLOTS;i++) tokens[i]=__atomic_load_n(&e->ranges[i].cancel,__ATOMIC_RELAXED);
  assert(seek_range(e,123,SEEK_SET)==123);
  assert(e->range_next==frontier);
  for(int i=0;i<RANGE_SLOTS;i++) assert(tokens[i]==__atomic_load_n(&e->ranges[i].cancel,__ATOMIC_RELAXED));
  check_bytes(e,65536);
  assert(seek_range(e,0,SEEK_SET)==0);
  check_bytes(e,16*1024*1024);
  assert(read_range(e,(uint8_t[1]){0},1)==AVERROR_EOF);
  if(strstr(argv[1],"/slow/")) {
    assert(e->range_bytes==256*1024);
    assert(e->metrics.range_short_reads==1);
  }
  fprintf(stderr,"AVIO lanes=%d:16MiB in %.3fs, %llu consumed ranges\n",
    RANGE_WORKERS,(clock_ns(CLOCK_MONOTONIC)-begun)/1e9,(unsigned long long)e->metrics.range_requests);
  assert(seek_range(e,5*1024*1024+17,SEEK_SET)==5*1024*1024+17);
  check_bytes(e,65536);
  assert(seek_range(e,99,SEEK_SET)==99); check_bytes(e,2*1024*1024);
  assert(seek_range(e,-123,SEEK_END)==16*1024*1024-123); check_bytes(e,123);
  dts_engine_destroy(e);
  e=dts_engine_create(); assert(e); dts_engine_allow_p7(e,1);
  e->url=strdup(argv[1]); e->headers=strdup("Authorization: Bearer engine-test\nX-Fixture: yes");
  char *path=strchr(e->url+7,'/'); assert(path); strcpy(path,"/idle");
  pthread_t thread; assert(!pthread_create(&thread,NULL,cancel_read,e));
  begun=clock_ns(CLOCK_MONOTONIC);
  assert(read_range(e,(uint8_t[1]){0},1)==AVERROR_EXIT);
  assert(!pthread_join(thread,NULL)); dts_engine_destroy(e);
  assert(clock_ns(CLOCK_MONOTONIC)-begun<UINT64_C(2500000000));
  fprintf(stderr,"Concurrent AVIO exact bytes, unaligned/backward/end seeks and in-flight cancellation passed.\n");
}
