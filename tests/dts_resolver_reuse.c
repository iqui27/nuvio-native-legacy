#define NV_DTS_FFMPEG
#define NV_DTS_WEBOS_RECOVERY
#include "../src/dts/dts_engine.c"
#include <assert.h>
#include <unistd.h>

typedef struct { DtsEngine *engine; DtsRangeJob job; } Fetch;
static void init_fetch(Fetch *f,DtsEngine *e,int64_t at) {
  memset(f,0,sizeof *f);f->engine=e;f->job.start=at;f->job.end=at+31;
}
static void *fetch(void *opaque) {
  Fetch *f=opaque;int stalled=0;
  range_fetch(f->engine,&f->job,1,&stalled);
  return NULL;
}
static void good(Fetch *f) {
  assert(f->job.data && f->job.status==206 && f->job.count==32);
  for(int i=0;i<32;i++)assert((unsigned char)f->job.data[i]==(f->job.start+i)%251);
  free(f->job.data);f->job.data=NULL;
}
static DtsEngine *engine(const char *base,const char *path) {
  DtsEngine *e=dts_engine_create();assert(e);
  char url[256];snprintf(url,sizeof url,"%s%s",base,path);
  e->url=strdup(url);e->headers=strdup("Authorization: Bearer resolver-test\nX-Fixture: yes");
  assert(e->url && e->headers && !e->dv && !e->allow_p7);
  return e;
}
int main(int argc,char **argv) {
  assert(argc==2);
  DtsEngine *e=engine(argv[1],"/resolve");Fetch f;
  for(int i=0;i<8;i++) {init_fetch(&f,e,i*32);fetch(&f);good(&f);}
  assert(strstr(e->final_url,"/media?g=1") && e->final_cross==1);
  assert(e->rate_until_ns[0]==0 && e->rate_until_ns[1]==0); // No redirect delay.
  init_fetch(&f,e,256);fetch(&f);good(&f);
  assert(strstr(e->final_url,"/media?g=2")); // Expired CDN address refreshed.
  init_fetch(&f,e,288);fetch(&f);good(&f);
  e->cache=malloc(32);assert(e->cache);memset(e->cache,77,32);e->cache_size=32;
  char *retained=e->cache;char final[8192];snprintf(final,sizeof final,"%s",e->final_url);
  init_fetch(&f,e,512);fetch(&f);assert(!f.job.data && f.job.status==429);
  assert(!strcmp(final,e->final_url)); // 429 preserves the direct URL.
  uint64_t begun=clock_ns(CLOCK_MONOTONIC);
  Fetch lanes[4];pthread_t threads[4];
  for(int i=0;i<4;i++) {init_fetch(&lanes[i],e,512+i*32);assert(!pthread_create(&threads[i],NULL,fetch,&lanes[i]));}
  for(int i=0;i<4;i++) {assert(!pthread_join(threads[i],NULL));good(&lanes[i]);}
  assert(clock_ns(CLOCK_MONOTONIC)-begun>=UINT64_C(900000000));
  assert(e->cache==retained && e->cache[0]==77 && e->cache_size==32);
  init_fetch(&f,e,1024);fetch(&f);assert(f.job.status==429 && !f.job.data);
  assert(e->rate_until_ns[1]>clock_ns(CLOCK_MONOTONIC)+UINT64_C(1500000000));
  Fetch seek;init_fetch(&seek,e,1024);pthread_t thread;
  assert(!pthread_create(&thread,NULL,fetch,&seek));usleep(50000);
  begun=clock_ns(CLOCK_MONOTONIC);__atomic_store_n(&seek.job.cancel,1,__ATOMIC_RELAXED);
  assert(!pthread_join(thread,NULL) && !seek.job.data);
  assert(clock_ns(CLOCK_MONOTONIC)-begun<UINT64_C(500000000));
  // Long server delays remain promptly cancellable by closing the player.
  pthread_mutex_lock(&e->range_lock);e->rate_until_ns[1]=0;pthread_mutex_unlock(&e->range_lock);
  init_fetch(&f,e,2048);fetch(&f);assert(f.job.status==429 && !f.job.data);
  assert(e->rate_until_ns[1]>clock_ns(CLOCK_MONOTONIC)+UINT64_C(59000000000));
  init_fetch(&seek,e,2048);assert(!pthread_create(&thread,NULL,fetch,&seek));usleep(50000);
  begun=clock_ns(CLOCK_MONOTONIC);dts_engine_cancel(e);
  assert(!pthread_join(thread,NULL) && !seek.job.data);
  assert(clock_ns(CLOCK_MONOTONIC)-begun<UINT64_C(500000000));
  dts_engine_destroy(e);
  e=engine(argv[1],"/resolve-limit");
  init_fetch(&f,e,0);fetch(&f);assert(f.job.status==429 && !f.job.data && !e->final_url[0]);
  // Model another in-flight lane publishing a usable media address. The API
  // pause must not stall that direct CDN route or be cleared by its success.
  pthread_mutex_lock(&e->range_lock);snprintf(e->final_url,sizeof e->final_url,"%s",final);e->final_cross=1;pthread_mutex_unlock(&e->range_lock);
  begun=clock_ns(CLOCK_MONOTONIC);init_fetch(&f,e,0);fetch(&f);good(&f);
  assert(clock_ns(CLOCK_MONOTONIC)-begun<UINT64_C(500000000));
  assert(e->rate_until_ns[0]>clock_ns(CLOCK_MONOTONIC));
  pthread_mutex_lock(&e->range_lock);e->final_url[0]=0;pthread_mutex_unlock(&e->range_lock);
  begun=clock_ns(CLOCK_MONOTONIC);init_fetch(&f,e,0);fetch(&f);good(&f);
  assert(clock_ns(CLOCK_MONOTONIC)-begun>=UINT64_C(700000000));
  init_fetch(&f,e,32);fetch(&f);good(&f);dts_engine_destroy(e);
  puts("PASS: SDR/P7-off resolver reuse, expired URL refresh, credential stripping, shared 429 delay, cache retention and cancel/seek");
}
