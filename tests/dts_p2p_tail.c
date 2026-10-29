/* Probe-policy regression: an uncached P2P tail waits for cancellation. */
#define NV_DTS_FFMPEG
#include "../src/dts/dts_engine.c"
#include <assert.h>
#include <unistd.h>

char *rede_baixar_trecho64_cab(const char *url,const char *headers,int64_t start,int64_t end,
    long *size,int64_t *total,int *status,volatile int *cancel) {
  (void)url; (void)headers; (void)end;
  if(start>0) {
    uint64_t until=clock_ns(CLOCK_MONOTONIC)+UINT64_C(15000000000);
    while(!__atomic_load_n(cancel,__ATOMIC_RELAXED) && clock_ns(CLOCK_MONOTONIC)<until) usleep(1000);
    *size=0; *total=-1; *status=206; return NULL;
  }
  *size=32; *total=INT64_C(7180)*1024*1024; *status=206;
  return calloc(1,32);
}
char *rede_baixar_trecho64_final(const char *u,const char *h,int64_t s,int64_t end,
    long *n,int64_t *t,int *st,volatile int *c,char *f,int *cross) {
  if(f) *f=0; if(cross) *cross=0;
  return rede_baixar_trecho64_cab(u,h,s,end,n,t,st,c);
}
char *rede_range_headers(const char *raw,int public_only) {
  (void)public_only;return strdup(raw?raw:"");
}
int rede_fluxo64(const char *u,const char *h,int64_t s,int (*sink)(void *,const char *,size_t),void *o,
    int64_t *t,int *st,volatile int *c,char *f,int *cross,long ms,int64_t *got) {
  (void)u;(void)h;(void)s;(void)sink;(void)o;(void)t;(void)st;(void)c;(void)f;(void)cross;(void)ms;(void)got;
  return -1;
}
int main(void) {
  DtsEngine *e=dts_engine_create(); assert(e);
  e->url=strdup("http://127.0.0.1:5555/stream/test"); e->headers=strdup("");
  e->total=INT64_C(7180)*1024*1024; e->pos=e->total-1024;
  e->range_bytes=RANGE_BYTES; e->range_slots=1; e->opening=1;
  assert(optional_p2p_tail(e));
  e->opening=0; assert(!optional_p2p_tail(e)); e->opening=1;
  const char *normal[]={"https://cdn.example/stream/test","http://127.0.0.1:5555/movie.mkv",
    "http://127.0.0.1:5555.evil/stream/test","http://192.168.1.2:5555/stream/test"};
  char *local=e->url;
  for(unsigned i=0;i<sizeof normal/sizeof *normal;i++) {
    e->url=(char *)normal[i]; assert(!optional_p2p_tail(e));
  }
  e->url=local;
  uint64_t begun=clock_ns(CLOCK_MONOTONIC);
  assert(read_range(e,(uint8_t[1]){0},1)==AVERROR(EIO));
  uint64_t elapsed=clock_ns(CLOCK_MONOTONIC)-begun;
  fprintf(stderr,"P2P stalled tail: %.3f s (old deadline: 15 s)\n",elapsed/1e9);
  assert(elapsed<UINT64_C(2500000000));
  assert(seek_range(e,0,SEEK_SET)==0);
  assert(!optional_p2p_tail(e));
  assert(read_range(e,(uint8_t[1]){0},1)==1);
  dts_engine_destroy(e);
}
