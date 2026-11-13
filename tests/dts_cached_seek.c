/* Real HTTP queue ownership: an interior cached seek transfers, never refetches. */
#define NV_DTS_FFMPEG
#define NV_DTS_WEBOS_RECOVERY
#include "../src/dts/dts_engine.c"
#include <assert.h>
#include <unistd.h>
static void check(DtsEngine *e,int64_t at) {
  uint8_t out[4096];assert(seek_range(e,at,SEEK_SET)==at);
  assert(read_range(e,out,sizeof out)==sizeof out);
  for(size_t i=0;i<sizeof out;i++) assert(out[i]==(uint8_t)((at+i)%251));
}
int main(int argc,char **argv) {
  assert(argc==2);
  // Pending interval later completes as a short response, before the seek
  // target. Restart at the exact target instead of copying a negative length.
  {
    DtsEngine *e=dts_engine_create();assert(e);
    char url[1024];const char *path=strrchr(argv[1],'/');assert(path);
    snprintf(url,sizeof url,"%.*s/slow%s",(int)(path-argv[1]),argv[1],path);
    e->url=strdup(url);e->headers=strdup("Authorization: Bearer engine-test\nX-Fixture: yes");
    e->range_bytes=RANGE_BYTES_DV;e->range_slots=2;
    assert(start_ranges(e));
    pthread_mutex_lock(&e->range_lock);queue_ranges(e);pthread_mutex_unlock(&e->range_lock);
    check(e,1024*1024+123);
    dts_engine_destroy(e);
    puts("PASS cached pending seek past a short HTTP prefix");
  }
#ifdef NV_NATIVE_SEEK_TEST
  // A full FUTURE cache must not leave only one slot to refill a multi-range
  // gap. Real delayed responses expose the old serial ~4 MiB/RTT behavior.
  {
    DtsEngine *e=dts_engine_create();assert(e);
    char url[1024];const char *path=strrchr(argv[1],'/');assert(path);
    snprintf(url,sizeof url,"%.*s/audio-gap%s",(int)(path-argv[1]),argv[1],path);
    e->url=strdup(url);e->headers=strdup("Authorization: Bearer engine-test\nX-Fixture: yes");
    e->dv=1;e->range_bytes=RANGE_BYTES_DV;e->range_slots=16;
    check(e,0);check(e,20LL*RANGE_BYTES_DV);
    __atomic_store_n(&e->range_live,1,__ATOMIC_RELAXED);
    int full=0;
    for(int n=0;n<2000;n++) {
      pthread_mutex_lock(&e->range_lock);full=1;
      for(int i=0;i<e->range_slots;i++)full&=e->ranges[i].state==3 && e->ranges[i].data;
      pthread_mutex_unlock(&e->range_lock);if(full)break;usleep(10000);
    }
    assert(full);
    char *saved[16];for(int i=0;i<16;i++)saved[i]=e->ranges[i].data;
    uint64_t began=clock_ns(CLOCK_MONOTONIC);
    int64_t target=16LL*RANGE_BYTES_DV+123;
    for(int i=0;i<4;i++)check(e,target+(int64_t)i*RANGE_BYTES_DV);
    assert(clock_ns(CLOCK_MONOTONIC)-began<UINT64_C(2500000000));
    int kept=0;int64_t bytes=e->cache_size;
    pthread_mutex_lock(&e->range_lock);
    for(int i=0;i<16;i++)if(e->ranges[i].data) {
      bytes+=e->ranges[i].count;
      for(int k=0;k<16;k++)kept+=e->ranges[i].data==saved[k];
    }
    pthread_mutex_unlock(&e->range_lock);
    assert(kept>=12 && bytes<=17LL*RANGE_BYTES_DV);
    dts_engine_destroy(e);
    puts("PASS full future DV cache: four missing ranges refill in parallel, exact bytes, retained payloads, unchanged RAM cap");
  }
  // All four HTTP lanes can be occupied by slow speculative requests while
  // audio selection needs a preceding keyframe. Free one lane, not the cache.
  {
    DtsEngine *e=dts_engine_create();assert(e);
    char url[1024];const char *path=strrchr(argv[1],'/');assert(path);
    snprintf(url,sizeof url,"%.*s/audio-prefetch%s",(int)(path-argv[1]),argv[1],path);
    e->url=strdup(url);e->headers=strdup("Authorization: Bearer engine-test\nX-Fixture: yes");
    e->dv=1;e->range_bytes=RANGE_BYTES_DV;e->range_slots=8;
    check(e,0);check(e,RANGE_BYTES_DV+123);
    __atomic_store_n(&e->range_live,1,__ATOMIC_RELAXED);
    int busy=0;
    for(int n=0;n<200;n++) {
      pthread_mutex_lock(&e->range_lock);busy=0;
      for(int i=0;i<e->range_slots;i++)busy+=e->ranges[i].state==2;
      pthread_mutex_unlock(&e->range_lock);if(busy==4)break;usleep(10000);
    }
    assert(busy==4);
    uint64_t began=clock_ns(CLOCK_MONOTONIC);check(e,123);
    assert(clock_ns(CLOCK_MONOTONIC)-began<UINT64_C(5000000000));
    int canceled=0;
    pthread_mutex_lock(&e->range_lock);
    for(int i=0;i<e->range_slots;i++)canceled+=__atomic_load_n(&e->ranges[i].cancel,__ATOMIC_RELAXED)!=0;
    pthread_mutex_unlock(&e->range_lock);assert(canceled==1);
    dts_engine_destroy(e);
    puts("PASS DV audio keyframe preempts one busy HTTP lane, exact bytes, no whole-queue cancellation");
  }
#endif
  for(int mib=64;mib<=256;mib*=2) {
    DtsEngine *e=dts_engine_create();assert(e);
    e->url=strdup(argv[1]);e->headers=strdup("Authorization: Bearer engine-test\nX-Fixture: yes");
    e->range_bytes=RANGE_BYTES_DV;e->range_slots=mib/4;
#ifdef NV_NATIVE_SEEK_TEST
    e->dv=1;
#endif
    check(e,0);
#ifdef NV_NATIVE_SEEK_TEST
    __atomic_store_n(&e->range_live,1,__ATOMIC_RELAXED);
#endif
    int full=0;
    for(int n=0;n<2000;n++) {
      pthread_mutex_lock(&e->range_lock);full=1;
      for(int i=0;i<e->range_slots;i++) full&=e->ranges[i].state==3 && e->ranges[i].data!=NULL;
      pthread_mutex_unlock(&e->range_lock);if(full)break;usleep(10000);
    }
    assert(full);
#ifdef NV_NATIVE_SEEK_TEST
    int64_t retired=0;
#endif
    for(int slot=3;slot<=11;slot+=4) {
      pthread_mutex_lock(&e->range_lock);
      DtsRangeJob *j=&e->ranges[slot];
      int64_t start=j->start,at=start+123;
      char *payload=j->data;long size=j->count;
      pthread_mutex_unlock(&e->range_lock);
      assert(payload && size>8192);
      // FFmpeg can position at an uncached index/EOF and return WITHOUT a read.
      // That probe must not cancel prefetch or free the cached seek target.
      int64_t probe = (int64_t)(e->range_slots + 10) * e->range_bytes;
      assert(seek_range(e,probe,SEEK_SET)==probe);
      pthread_mutex_lock(&e->range_lock);
      assert(j->data==payload && j->state==3);
      pthread_mutex_unlock(&e->range_lock);
      assert(seek_range(e,at,SEEK_SET)==at);
      // Old code cleared this exact payload on an interior seek.
      pthread_mutex_lock(&e->range_lock);assert(j->data==payload && j->state==3);pthread_mutex_unlock(&e->range_lock);
#ifdef NV_NATIVE_SEEK_TEST
      retired=e->cache_start;
#endif
      check(e,at);
      assert(e->cache==payload && e->cache_start==start && e->cache_size==size);
      check(e,at+17); // Small backward/forward reposition inside the active range.
      int64_t bytes=e->cache_size;
      pthread_mutex_lock(&e->range_lock);
      for(int i=0;i<e->range_slots;i++) if(e->ranges[i].data)bytes+=e->ranges[i].count;
      pthread_mutex_unlock(&e->range_lock);
      assert(bytes<=(int64_t)(e->range_slots+1)*e->range_bytes);
    }
#ifdef NV_NATIVE_SEEK_TEST
    // Audio selection demuxes back to an earlier keyframe. That small hole
    // must not discard the already downloaded forward window or its payloads.
    char *saved[RANGE_SLOTS_MAX]={0};int saved_count=0;
    pthread_mutex_lock(&e->range_lock);
    for(int i=0;i<e->range_slots;i++)if(e->ranges[i].state==3 && e->ranges[i].data) {
      saved[saved_count++]=e->ranges[i].data;
    }
    pthread_mutex_unlock(&e->range_lock);
    assert(saved_count>=e->range_slots-4 && retired>0);
    check(e,retired+123);
    int kept=0;int64_t bytes=e->cache_size;
    pthread_mutex_lock(&e->range_lock);
    for(int i=0;i<e->range_slots;i++)if(e->ranges[i].data) {
      bytes+=e->ranges[i].count;
      for(int k=0;k<saved_count;k++)kept+=e->ranges[i].data==saved[k];
    }
    pthread_mutex_unlock(&e->range_lock);
    assert(kept>=saved_count-RANGE_WORKERS && bytes<=(int64_t)(e->range_slots+1)*e->range_bytes);
    // Cross the replenished gap and the retained ranges byte-for-byte.
    check(e,retired+RANGE_BYTES_DV-4096);
    check(e,retired+RANGE_BYTES_DV+17);
    // The same backward hole can be requested again after its active payload
    // was consumed. Replenishing it must not overlap the retained intervals.
    check(e,retired+222);
    pthread_mutex_lock(&e->range_lock);
    for(int i=0;i<e->range_slots;i++)for(int k=i+1;k<e->range_slots;k++) {
      DtsRangeJob *a=&e->ranges[i],*b=&e->ranges[k];
      if(!a->state || !b->state)continue;
      int64_t a_end=a->state==3?a->start+a->count:a->end+1;
      int64_t b_end=b->state==3?b->start+b->count:b->end+1;
      if(a_end>a->start && b_end>b->start)assert(a_end<=b->start || b_end<=a->start);
    }
    pthread_mutex_unlock(&e->range_lock);
    printf("PASS %d MiB DV audio reposition: retained %d/%d payloads after a missing keyframe range; unchanged allocation cap\n",mib,kept,saved_count);
#endif
    // A real read outside the old window still replaces/refills the bounded
    // queue at the demanded offset; deferring eviction cannot return old bytes.
    check(e,(int64_t)(e->range_slots+10)*e->range_bytes+321);
    assert(e->cache_start==(int64_t)(e->range_slots+10)*e->range_bytes+321);
    dts_engine_destroy(e);
    printf("PASS %d MiB: index positioning retains cached ranges, exact seek bytes, original allocation transferred, unchanged payload cap\n",mib);
  }
}
