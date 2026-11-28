/* Probe-policy regression: an uncached P2P tail waits for cancellation.
 * With a fixture MKV, the real open goes through Standard (1)'s read_fluxo:
 * bytes from the Cues on (Cues, Tags) are not downloaded yet, and the
 * header's SeekHead makes the demuxer read the Tags while opening. */
#define NV_DTS_FFMPEG
#include "../src/dts/dts_engine.c"
#include <assert.h>
#include <unistd.h>

static const char *fixture;      /* local MKV served as the P2P stream */
static int64_t fixture_tail=-1;  /* first byte the motor does not have */
static int tail_waits;
static void hold(volatile int *cancel,long ms) {
  uint64_t until=clock_ns(CLOCK_MONOTONIC)+(uint64_t)ms*1000000u;
  while(!__atomic_load_n(cancel,__ATOMIC_RELAXED) && clock_ns(CLOCK_MONOTONIC)<until) usleep(1000);
}
char *rede_baixar_trecho64_cab(const char *url,const char *headers,int64_t start,int64_t end,
    long *size,int64_t *total,int *status,volatile int *cancel) {
  (void)url; (void)headers;
  if(fixture) {
    *size=0; *total=-1; *status=0;
    if(start>=fixture_tail) { __atomic_add_fetch(&tail_waits,1,__ATOMIC_RELAXED); hold(cancel,15000); return NULL; }
    FILE *f=fopen(fixture,"rb"); assert(f);
    assert(!fseeko(f,0,SEEK_END)); *total=ftello(f);
    if(end>=fixture_tail) end=fixture_tail-1;
    long n=(long)(end-start+1); char *d=malloc((size_t)n); assert(d);
    assert(!fseeko(f,start,SEEK_SET)); *size=(long)fread(d,1,(size_t)n,f); fclose(f);
    *status=206; return d;
  }
  if(start>0) {
    hold(cancel,15000);
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
char *rede_baixar_trecho64_relato(const char *u,const char *h,int64_t s,int64_t e,long *n,
    int64_t *t,int *status,volatile int *c,char *f,int *x,volatile int *urgent,unsigned idle,int *stalled,int *retry_after) {
  (void)urgent;(void)idle;if(stalled)*stalled=0;if(retry_after)*retry_after=-1;
  return rede_baixar_trecho64_final(u,h,s,e,n,t,status,c,f,x);
}
char *rede_range_headers(const char *raw,int public_only) {
  (void)public_only;return strdup(raw?raw:"");
}
int rede_fluxo64(const char *u,const char *h,int64_t s,int (*sink)(void *,const char *,size_t),void *o,
    int64_t *t,int *st,volatile int *c,char *f,int *cross,long ms,int64_t *got) {
  (void)u;(void)h;
  if(!fixture) return -1;
  if(f) *f=0; if(cross) *cross=0;
  FILE *in=fopen(fixture,"rb"); assert(in);
  assert(!fseeko(in,0,SEEK_END)); *t=ftello(in); *st=206; *got=0;
  assert(!fseeko(in,s,SEEK_SET));
  static char buf[65536];
  for(int64_t at=s;at<*t;) {
    if(__atomic_load_n(c,__ATOMIC_RELAXED)) { fclose(in); return 1; }
    if(at>=fixture_tail) {   /* the motor has not fetched these pieces */
      __atomic_add_fetch(&tail_waits,1,__ATOMIC_RELAXED);
      hold(c,ms); fclose(in); return __atomic_load_n(c,__ATOMIC_RELAXED)?1:-1;
    }
    int64_t k=fixture_tail-at; if(k>(int64_t)sizeof buf) k=sizeof buf;
    assert(fread(buf,1,(size_t)k,in)==(size_t)k);
    if(!sink(o,buf,(size_t)k)) { fclose(in); return 1; }
    at+=k; *got+=k;
  }
  fclose(in); return 0;
}
static volatile int opened;
static void *watchdog(void *e) {   /* bounds the old 20 s stall + retries */
  for(int i=0;i<500 && !__atomic_load_n(&opened,__ATOMIC_RELAXED);i++) usleep(10000);
  if(!__atomic_load_n(&opened,__ATOMIC_RELAXED)) dts_engine_cancel(e);
  return NULL;
}
static int open_timed(DtsEngine *e,const char *url,double *seconds) {
  __atomic_store_n(&opened,0,__ATOMIC_RELAXED);
  pthread_t dog; assert(!pthread_create(&dog,NULL,watchdog,e));
  uint64_t begun=clock_ns(CLOCK_MONOTONIC);
  int r=dts_engine_open(e,url,"",1,0,0);
  *seconds=(clock_ns(CLOCK_MONOTONIC)-begun)/1e9;
  __atomic_store_n(&opened,1,__ATOMIC_RELAXED); pthread_join(dog,NULL);
  return r;
}
static void frames(DtsEngine *e,int stream,int want) {
  DtsFrame f; int audio=0, video=0, r;
  while((audio<want || video<want) && (r=dts_engine_next(e,&f))>0) {
    if(f.kind==DTS_AUDIO) { assert(f.stream_index==stream); audio++; }
    else if(f.kind==DTS_VIDEO) video++;
  }
  if(audio<want || video<want) { fprintf(stderr,"frames: %s\n",e->error); assert(0); }
}
/* Standard (1) on the real open path: the Cues probe gives up, media plays. */
static void standard_fixture(void) {
  FILE *f=fopen(fixture,"rb"); assert(f);
  assert(!fseeko(f,0,SEEK_END)); int64_t size=ftello(f);
  long tail=size>65536?65536:(long)size; char *b=malloc((size_t)tail); assert(b);
  assert(!fseeko(f,size-tail,SEEK_SET) && fread(b,1,(size_t)tail,f)==(size_t)tail); fclose(f);
  for(long i=tail-4;i>=0 && fixture_tail<0;i--)
    if(!memcmp(b+i,"\x1c\x53\xbb\x6b",4)) fixture_tail=size-tail+i;   /* Cues ID */
  free(b);
  assert(fixture_tail>4*1024*1024 && fixture_tail>=size-4*1024*1024);
  dts_engine_set_http_workers(1);
  DtsEngine *e=dts_engine_create(); assert(e);
  double took;
  int r=open_timed(e,"http://127.0.0.1:5555/stream/fixture",&took);
  fprintf(stderr,"Standard (1) P2P open with missing Cues: %.3f s, rc=%d%s%s\n",took,r,r<0?" ":"",r<0?e->error:"");
  assert(e->conv_stream && e->io && e->io->read_packet==read_fluxo);
  assert(r==0 && took<3.0 && tail_waits>0);
  AVFormatContext *fmt=e->fmt; AVIOContext *io=e->io;
  int64_t missing=fixture_tail; fixture_tail=INT64_MAX;   /* the motor got them since */
  frames(e,1,8);
  /* Standard (1) reuse: seek and track change keep the demuxer and ring. */
  assert(dts_engine_seek(e,2.0)==0); frames(e,1,8);
  assert(dts_engine_audio(e,2,0,1.0)==0);
  assert(e->fmt==fmt && e->io==io && e->info.audio_stream==2);
  frames(e,2,8);
  dts_engine_destroy(e);
  /* Control: any other route keeps waiting for its tail (only the watchdog
   * stops it); the budget is the embedded motor's alone. */
  fixture_tail=missing; e=dts_engine_create(); assert(e);
  r=open_timed(e,"http://192.168.1.2:5555/stream/fixture",&took);
  fprintf(stderr,"Standard (1) non-P2P open with missing Cues: %.3f s\n",took);
  assert(took>=4.5);
  dts_engine_destroy(e);
  dts_engine_set_http_workers(4);
}
int main(int argc,char **argv) {
  if(argc>1) { fixture=argv[1]; standard_fixture(); fixture=NULL; }
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
