#include "dts_engine.h"
#include "dv7.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
struct DtsEngine;
/* Output mode for the NEXT open: 0 = stereo AAC (default, the proven path),
 * 1 = AC3 5.1 at 640 kbps (falls back to AAC when the encoder is absent). */
static volatile int g_surround_ac3;
void dts_engine_set_ac3(int on) { g_surround_ac3 = on ? 1 : 0; }
/* Samsung remux ring (read_fluxo) for engines created after this call, in MiB:
 * 32 by default; the .tpk asks for 16 on Tizen 6/6.5/7 (less RAM). */
static volatile int g_fx_cap_mib = 32;
void dts_engine_set_ring_mib(int mib) { g_fx_cap_mib = mib >= 8 && mib <= 32 ? mib : 32; }
static volatile int g_ahead_mib;
void dts_engine_set_ahead_mib(int mib) { g_ahead_mib = mib > 0 && mib <= 256 ? mib : 0; }
/* Rings alive (read_fluxo allocates, fx_close frees): the .tpk memory line. */
static pthread_mutex_t g_rings_lock = PTHREAD_MUTEX_INITIALIZER;
static DtsEngineRings g_rings;
void dts_engine_rings(DtsEngineRings *out, int reset_peak) {
  pthread_mutex_lock(&g_rings_lock);
  if (out) *out = g_rings;
  if (reset_peak) { g_rings.peak = g_rings.alive; g_rings.peak_bytes = g_rings.bytes; }
  pthread_mutex_unlock(&g_rings_lock);
}
#ifndef NV_DTS_FFMPEG
struct DtsEngine { DtsMediaInfo info; };
void dts_engine_buffer_info(DtsEngine *e,DtsBufferInfo *b) { (void)e; if(b) memset(b,0,sizeof *b); }
DtsEngine *dts_engine_source_create(const char *u,const char *h,int m) { (void)u;(void)h;(void)m;return NULL; }
int dts_engine_source_read(DtsEngine *e,int64_t o,unsigned char *d,int n,int fd) { (void)e;(void)o;(void)d;(void)n;(void)fd;return -1; }
int64_t dts_engine_source_total(DtsEngine *e) { (void)e;return -1; }
DtsEngine *dts_engine_create(void) { return calloc(1, sizeof(DtsEngine)); }
int dts_engine_available(void) { return 0; }
void dts_engine_dv(DtsEngine *e, int enabled) { (void)e; (void)enabled; }
void dts_engine_allow_p7(DtsEngine *e, int enabled) { (void)e; (void)enabled; }
void dts_engine_metrics(DtsEngine *e,DtsEngineMetrics *out) { (void)e; if(out) memset(out,0,sizeof(*out)); }
int dts_engine_open(DtsEngine *e,const char *u,const char *h,int a,int c,double s) {
  (void)e;(void)u;(void)h;(void)a;(void)c;(void)s; return -1;
}
const DtsMediaInfo *dts_engine_info(DtsEngine *e) { return e ? &e->info : NULL; }
int dts_engine_has_core(DtsEngine *e) { (void)e; return 0; }
const void *dts_engine_subtitle_parameters(DtsEngine *e,int i) { (void)e;(void)i; return NULL; }
int dts_engine_next(DtsEngine *e,DtsFrame *f) { (void)e;(void)f; return -1; }
int dts_engine_seek(DtsEngine *e,double s) { (void)e;(void)s; return -1; }
void dts_engine_cancel(DtsEngine *e) { (void)e; }
void dts_engine_destroy(DtsEngine *e) { free(e); }
const char *dts_engine_error(DtsEngine *e) { (void)e; return "DTS conversion not built"; }
int dts_engine_open_demux(DtsEngine *e,const char *u,const char *h) { (void)e;(void)u;(void)h; return -1; }
void *dts_engine_avformat(DtsEngine *e) { (void)e; return NULL; }
int64_t dts_engine_size(DtsEngine *e) { (void)e; return -1; }
void dts_engine_interrupt(DtsEngine *e,int on) { (void)e; (void)on; }
#else
#include "../rede.h"
#include <limits.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/bsf.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/opt.h>
#include <libavutil/dovi_meta.h>
#include <libswresample/swresample.h>
#define RANGE_BYTES (1024 * 1024)
/* Dolby Vision path (dts_engine_dv): 4K sources of 25-30 Mbit/s starved on
 * 1 MiB requests that re-followed the add-on redirects every time (LG C9:
 * readMs 4953/5000, 0.9 s of video ahead). Larger requests sent straight to
 * the final address keep the native buffer full with the same 32 MiB cap. */
#define RANGE_BYTES_DV (4 * 1024 * 1024 - 1)
#define RANGE_SLOTS_DV 8
#ifndef RANGE_WORKERS
#define RANGE_WORKERS 4
#endif
#define RANGE_SLOTS 32
#define RANGE_SLOTS_MAX 64       /* 256 MiB of 4 MiB ranges (dts_engine_set_ahead_mib) */
typedef struct {
  pthread_t thread;
  struct DtsEngine *engine;
} DtsRangeWorker;
typedef struct {
  int state; /* 0 idle, 1 queued, 2 fetching, 3 complete */
  volatile int cancel, urgent;
  int64_t start, end, total;
  char *data;
  long count;
  int status;
  uint64_t elapsed;
} DtsRangeJob;
static const AVRational NS = {1,1000000000};
struct DtsEngine {
  DtsMediaInfo info;
  char error[160], *url, *headers, *cache;
  char *source_public_headers;
  int64_t pos, total, cache_start;
  long cache_size;
  volatile int cancelled;
  pthread_mutex_t range_lock;
  pthread_cond_t range_wake;
  DtsRangeJob ranges[RANGE_SLOTS_MAX];
  DtsRangeWorker workers[RANGE_WORKERS];
  int range_threads, range_stop;
  volatile int range_live;
  int64_t range_next;
  long range_bytes;
  int dv, passthrough, range_slots, final_cross, opening;
  int source_stream; /* Native HTTP cache; conversion readers retain their behavior. */
  volatile int source_cancel;
  /* Samsung remux (dts_engine_open_demux): container only, large ranges and
   * the final address reused, like the Dolby Vision path, because the whole
   * video also flows through this reader. Never set on webOS. */
  int remux;
  /* Remux only: abort the CURRENT read without the permanent cancel (a stuck
   * range while the player already gave up on that segment). Cleared by the
   * caller before the next run; the demuxer state is then reset by a seek. */
  volatile int interrupt;
  /* Remux: ONE sequential connection (rede_fluxo64) into a ring buffer, like
   * the TV's player reads, instead of 4 MiB requests on 4 lanes (TorBox
   * throttled and then refused those at 60+ Mbit/s). Guarded by range_lock. */
  pthread_t fx_thread;
  int fx_thread_ok, fx_stop, fx_eof, fx_err, fx_attempts;
  unsigned char *fx_buf;
  int64_t fx_cap;            /* ring size, fixed when the ring is allocated */
  int64_t fx_lo, fx_hi;      /* bytes [fx_lo, fx_hi) of the file are in the ring */
  int64_t fx_req;            /* restart the read at this offset (-1: none) */
  int64_t fx_from;           /* where the current read started (read-ahead grows from it) */
  uint64_t fx_resolved_ms;   /* last time the final address was dropped for the add-on's */
  char final_url[8192];
  AVFormatContext *fmt;
  AVIOContext *io;
  AVCodecContext *dec, *enc;
  AVBSFContext *bsf;
  SwrContext *swr;
  AVAudioFifo *fifo;
  AVPacket *in, *out;
  int allow_p7, dv7;
  uint8_t *dv7_buffer;
  unsigned dv7_capacity;
  AVFrame *decoded, *encoded;
  AVChannelLayout input_layout;
  enum AVSampleFormat input_format;
  int input_rate, demux_eof, decoder_eof, resampler_eof, encoder_sent_eof;
  int64_t sample_pts, target_ns;
  int64_t pad_anchor; /* Output samples; budget follows decoded media time. */
  int pad_count, pad_samples;
  int timeline, has_core;
  DtsEngineMetrics metrics;
#ifdef NV_DTS_DEBUG
  DtsEngineMetrics logged_metrics;
  uint64_t logged_at_ns;
#endif
};
static uint64_t clock_ns(clockid_t clock) {
  struct timespec t;
  if(clock_gettime(clock,&t)) return 0;
  return (uint64_t)t.tv_sec*UINT64_C(1000000000)+(uint64_t)t.tv_nsec;
}
void dts_engine_metrics(DtsEngine *e,DtsEngineMetrics *out) {
  if(out) { if(e) *out=e->metrics; else memset(out,0,sizeof(*out)); }
}
static int fail(DtsEngine *e,const char *msg,int code) {
  char detail[64] = "";
  if (code < 0) av_strerror(code,detail,sizeof(detail));
  snprintf(e->error,sizeof(e->error),"%s%s%s",msg,*detail?": ":"",detail);
  return -1;
}
static int cancelled(void *opaque) {
  return __atomic_load_n(&((DtsEngine *)opaque)->cancelled,__ATOMIC_RELAXED);
}
/* Persistent HTTP lanes retain independent curl connection caches. The bounded
 * queue keeps fetching while native playback backpressures the demux consumer,
 * so a slow future request can finish before playback needs its bytes. Payload
 * storage is at most 32 speculative 1 MiB ranges plus the 1 MiB active cache
 * (each HTTP allocation also includes one terminating byte). */
/* Retry the SAME validated byte interval before exposing an I/O error to
 * libavformat. Other completed speculative ranges stay owned by their slots.
 * A failed/partial HTTP body never enters the active cache. */
#ifdef NV_DTS_WEBOS_RECOVERY
#define RANGE_ATTEMPTS 3
#else
#define RANGE_ATTEMPTS 1
#endif
static int range_retryable(int status) {
  return status==0 || status==206 || (status>=300 && status<400) ||
         status==408 || status==425 || status==429 || (status>=500 && status<=599);
}
static int range_backoff(DtsEngine *e,DtsRangeJob *job,int ms) {
  struct timespec until; clock_gettime(CLOCK_REALTIME,&until);
  until.tv_nsec+=(long)ms*1000000L;
  if(until.tv_nsec>=1000000000L) { until.tv_sec++; until.tv_nsec-=1000000000L; }
  pthread_mutex_lock(&e->range_lock);
  int r=0;
  while(!e->range_stop && !cancelled(e) && !__atomic_load_n(&job->cancel,__ATOMIC_RELAXED) && r!=ETIMEDOUT) {
    r=pthread_cond_timedwait(&e->range_wake,&e->range_lock,&until);
    if(r && r!=ETIMEDOUT) break;
  }
  int ok=!e->range_stop && !cancelled(e) && !__atomic_load_n(&job->cancel,__ATOMIC_RELAXED);
  pthread_mutex_unlock(&e->range_lock); return ok;
}
static void range_fetch(DtsEngine *e,DtsRangeJob *job,int fast,int *stalled) {
  uint64_t begun=clock_ns(CLOCK_MONOTONIC);
#ifndef NV_DTS_WEBOS_RECOVERY
  (void)fast;
#endif
  if(!e->dv && !e->remux) {
#ifdef NV_DTS_WEBOS_RECOVERY
    job->data=rede_baixar_trecho64_vigiado(e->url,e->headers,job->start,job->end,
      &job->count,&job->total,&job->status,&job->cancel,NULL,NULL,
      fast?&job->urgent:NULL,2500,stalled);
#else
    job->data=rede_baixar_trecho64_cab(e->url,e->headers,job->start,job->end,
      &job->count,&job->total,&job->status,&job->cancel);
#endif
  } else {
    /* After the first answer, go straight to the address behind the add-on
     * redirects. Credential headers follow only on the same origin. A final
     * link that stops answering (expired: 403/410, or any failure) is
     * dropped and the request repeats through the original URL. */
    char target[8192], fin[8192]; int direct, cross=0;
    pthread_mutex_lock(&e->range_lock);
    direct=e->final_url[0]!=0; cross=e->final_cross;
    snprintf(target,sizeof target,"%s",direct?e->final_url:e->url);
    pthread_mutex_unlock(&e->range_lock);
#ifdef NV_DTS_WEBOS_RECOVERY
    job->data=rede_baixar_trecho64_vigiado(target,direct&&cross?"":e->headers,job->start,job->end,
      &job->count,&job->total,&job->status,&job->cancel,fin,&cross,
      fast?&job->urgent:NULL,2500,stalled);
#else
    job->data=rede_baixar_trecho64_final(target,direct&&cross?"":e->headers,job->start,job->end,
      &job->count,&job->total,&job->status,&job->cancel,fin,&cross);
#endif
    uint64_t first=clock_ns(CLOCK_MONOTONIC);
    int first_status=job->status, first_ok=job->data!=NULL;
    if(direct && !job->data && !*stalled && !__atomic_load_n(&job->cancel,__ATOMIC_RELAXED)) {
      pthread_mutex_lock(&e->range_lock);
      if(!strcmp(e->final_url,target)) e->final_url[0]=0;
      pthread_mutex_unlock(&e->range_lock);
#ifdef NV_DTS_WEBOS_RECOVERY
      job->data=rede_baixar_trecho64_vigiado(e->url,e->headers,job->start,job->end,
        &job->count,&job->total,&job->status,&job->cancel,fin,&cross,
        fast?&job->urgent:NULL,2500,stalled);
#else
      job->data=rede_baixar_trecho64_final(e->url,e->headers,job->start,job->end,
        &job->count,&job->total,&job->status,&job->cancel,fin,&cross);
#endif
      direct=0;
      /* #203 DV startup took ~19.5 s twice (LG C9): one stalled request
       * hitting the 15 s curl deadline plus a ~4.5 s add-on re-resolve is
       * the suspicion. Name it in the log: no URL, only timings/status. */
      uint64_t done=clock_ns(CLOCK_MONOTONIC);
      printf("[dts-range] direto falhou: at=%lldMiB ms=%llu http=%d; pelo addon: ms=%llu http=%d bytes=%ld\n",
             (long long)(job->start>>20),(unsigned long long)((first-begun)/1000000),first_status,
             (unsigned long long)((done-first)/1000000),job->status,job->count);
      fflush(stdout);
    } else if(!__atomic_load_n(&job->cancel,__ATOMIC_RELAXED) &&
              ((!first_ok && !(e->remux && first_status==416)) || first-begun>=UINT64_C(3000000000))) {
      printf("[dts-range] %s: at=%lldMiB ms=%llu http=%d bytes=%ld direto=%d\n",first_ok?"lento":"falhou",
             (long long)(job->start>>20),(unsigned long long)((first-begun)/1000000),first_status,job->count,direct);
      fflush(stdout);
    }
    if(!direct && job->data && fin[0]) {
      pthread_mutex_lock(&e->range_lock);
      snprintf(e->final_url,sizeof e->final_url,"%s",fin); e->final_cross=cross;
      pthread_mutex_unlock(&e->range_lock);
    }
  }
}
static void *range_worker(void *opaque) {
  DtsEngine *e=((DtsRangeWorker *)opaque)->engine;
  pthread_mutex_lock(&e->range_lock);
  for (;;) {
    DtsRangeJob *job=NULL;
    while(!e->range_stop) {
      if(!cancelled(e)) {
        for(int i=0;i<RANGE_SLOTS_MAX;i++) {
          DtsRangeJob *candidate=&e->ranges[i];
          if(candidate->state==1 && (!job || candidate->start<job->start)) job=candidate;
        }
      }
      if(job) break;
      pthread_cond_wait(&e->range_wake,&e->range_lock);
    }
    if(e->range_stop) break;
    job->state=2;
    pthread_mutex_unlock(&e->range_lock);
    uint64_t begun=clock_ns(CLOCK_MONOTONIC);
    int attempt=0, attempts=RANGE_ATTEMPTS;
    for(;attempt<attempts;attempt++) {
      if(cancelled(e) || __atomic_load_n(&job->cancel,__ATOMIC_RELAXED)) break;
      int stalled=0;
      range_fetch(e,job,attempt+1<attempts,&stalled);
      if(job->data || !range_retryable(job->status) || cancelled(e) ||
         __atomic_load_n(&job->cancel,__ATOMIC_RELAXED)) break;
      printf("[range-retry] start=%lld end=%lld attempt=%d/%d http=%d\n",
             (long long)job->start,(long long)job->end,attempt+1,attempts,job->status);
      fflush(stdout);
      if(attempt+1==attempts || (!stalled && !range_backoff(e,job,150*(attempt+1)))) break;
    }
    if(!cancelled(e) && !__atomic_load_n(&job->cancel,__ATOMIC_RELAXED) &&
       (attempt || !job->data)) {
      printf("[range-result] start=%lld end=%lld attempts=%d ok=%d http=%d bytes=%ld ms=%llu\n",
             (long long)job->start,(long long)job->end,attempt+1,job->data!=NULL,
             job->status,job->count,(unsigned long long)((clock_ns(CLOCK_MONOTONIC)-begun)/1000000));
      fflush(stdout);
    }
    uint64_t ended=clock_ns(CLOCK_MONOTONIC);
    pthread_mutex_lock(&e->range_lock);
    job->elapsed=ended>=begun?ended-begun:0; job->state=3;
    pthread_cond_broadcast(&e->range_wake);
  }
  pthread_mutex_unlock(&e->range_lock); return NULL;
}
static void clear_ranges(DtsEngine *e,const char *reason) {
  pthread_mutex_lock(&e->range_lock);
  int ready=0, active=0; int64_t bytes=0;
  for(int i=0;i<RANGE_SLOTS_MAX;i++) {
    DtsRangeJob *j=&e->ranges[i]; active+=j->state==2;
    if(j->state==3 && j->data && j->count>0) { ready++; bytes+=j->count; }
  }
  if(ready || active) {
    printf("[range-reset] reason=%s pos=%lld ready=%d readyBytes=%lld active=%d\n",
           reason,(long long)e->pos,ready,(long long)bytes,active); fflush(stdout);
  }
  for(int i=0;i<RANGE_SLOTS_MAX;i++) {
    DtsRangeJob *job=&e->ranges[i];
    __atomic_store_n(&job->cancel,1,__ATOMIC_RELAXED);
    if(job->state==1) job->state=0;
  }
  for(int i=0;i<RANGE_SLOTS_MAX;i++) {
    DtsRangeJob *job=&e->ranges[i];
    while(job->state==2) pthread_cond_wait(&e->range_wake,&e->range_lock);
    free(job->data); job->data=NULL; job->state=0;
  }
  e->range_next=e->pos;
  pthread_mutex_unlock(&e->range_lock);
}
static int start_ranges(DtsEngine *e) {
  pthread_mutex_lock(&e->range_lock);
  if(e->range_threads) { pthread_mutex_unlock(&e->range_lock); return 1; }
  for(int i=0;i<RANGE_WORKERS;i++) {
    DtsRangeWorker *worker=&e->workers[i]; worker->engine=e;
    if(pthread_create(&worker->thread,NULL,range_worker,worker)) break;
    e->range_threads++;
  }
  e->range_next=e->pos;
  int ok=e->range_threads>0;
  pthread_mutex_unlock(&e->range_lock); return ok;
}
/* Called under range_lock; only idle slots can receive new work. */
static void queue_ranges(DtsEngine *e) {
  for(int i=0;i<e->range_slots && !cancelled(e);i++) {
    DtsRangeJob *job=&e->ranges[i];
    if(job->state || (e->total>=0 && e->range_next>=e->total)) continue;
    job->start=e->range_next;
    job->end=job->start>INT64_MAX-e->range_bytes?INT64_MAX:job->start+e->range_bytes-1;
    job->count=0; job->status=0; job->total=-1;
    __atomic_store_n(&job->cancel,0,__ATOMIC_RELAXED);
    __atomic_store_n(&job->urgent,0,__ATOMIC_RELAXED);
    job->state=1;
    e->range_next=job->end==INT64_MAX?INT64_MAX:job->end+1;
  }
  pthread_cond_broadcast(&e->range_wake);
}
static void rings_add(long long bytes) {
  pthread_mutex_lock(&g_rings_lock);
  g_rings.alive += bytes > 0 ? 1 : -1; g_rings.bytes += bytes;
  if (g_rings.alive > g_rings.peak) g_rings.peak = g_rings.alive;
  if (g_rings.bytes > g_rings.peak_bytes) g_rings.peak_bytes = g_rings.bytes;
  pthread_mutex_unlock(&g_rings_lock);
}
#define FX_CAP (e->fx_cap)             /* ring: read-ahead plus history (dts_engine_set_ring_mib) */
#define FX_HIST (2 * 1024 * 1024)      /* kept behind the reader (small backward seeks) */
#define FX_SKIP (1024 * 1024)          /* a forward jump this short waits, no new request */
#define FX_STALL_MS 20000              /* no byte for this long (not paused by us): drop */
#define FX_TRIES 6                     /* 1+2+4+8+15 s of backoff, then the run fails */
static uint64_t fx_now_ms(void) { return clock_ns(CLOCK_MONOTONIC)/1000000u; }
/* Read-ahead allowed past the reader: twice what was read since the request
 * started, at least 2 MiB. A probe that only needs the header and the Cues
 * then costs a few MiB, not a full ring, at each jump. */
static int64_t fx_ahead(DtsEngine *e) {
  if(e->source_stream) return FX_CAP-FX_HIST;
  int64_t used=e->pos>e->fx_from?e->pos-e->fx_from:0, a=2*used;
  if(a<2*1024*1024) a=2*1024*1024;
  if(a>FX_CAP-FX_HIST) a=FX_CAP-FX_HIST;
  return a;
}
/* Called under range_lock by the streaming thread: room for more bytes? */
static int64_t fx_room(DtsEngine *e) {
  int64_t floor=e->pos<e->fx_hi?e->pos:e->fx_hi;
  floor-=FX_HIST; if(floor<e->fx_lo) floor=e->fx_lo;
  e->fx_lo=floor;                                  /* older bytes may be overwritten */
  int64_t room=FX_CAP-(e->fx_hi-e->fx_lo);
  int64_t ahead=fx_ahead(e)-(e->fx_hi-(e->pos>e->fx_hi?e->fx_hi:e->pos));
  if(e->pos>e->fx_hi) ahead=FX_CAP;                /* reader waits past the end: fill */
  return room<ahead?room:ahead;
}
static int fx_sink(void *opaque,const char *data,size_t n) {
  DtsEngine *e=opaque;
  pthread_mutex_lock(&e->range_lock);
  while(n>0) {
    if(e->fx_stop || cancelled(e) || e->fx_req>=0) { pthread_mutex_unlock(&e->range_lock); return 0; }
    int64_t room=fx_room(e);
    if(room<=0) { pthread_cond_wait(&e->range_wake,&e->range_lock); continue; }
    size_t k=n<(size_t)room?n:(size_t)room;
    size_t at=(size_t)(e->fx_hi%FX_CAP), first=FX_CAP-at<k?FX_CAP-at:k;
    memcpy(e->fx_buf+at,data,first);
    if(k>first) memcpy(e->fx_buf,data+first,k-first);
    e->fx_hi+=(int64_t)k; data+=k; n-=k;
    e->metrics.range_bytes+=(uint64_t)k;
    pthread_cond_broadcast(&e->range_wake);
  }
  pthread_mutex_unlock(&e->range_lock);
  return 1;
}
static void *fx_worker(void *opaque) {
  DtsEngine *e=opaque;
  pthread_mutex_lock(&e->range_lock);
  while(!e->fx_stop && !cancelled(e)) {
    if(e->fx_req>=0) {                       /* (re)start at the reader's offset */
      e->fx_lo=e->fx_hi=e->fx_from=e->fx_req; e->fx_req=-1;
      e->fx_eof=0; e->fx_err=0; e->fx_attempts=0;
    } else if(e->fx_eof || e->fx_err || (e->total>=0 && e->fx_hi>=e->total)) {
      if(e->total>=0 && e->fx_hi>=e->total) e->fx_eof=1;
      pthread_cond_broadcast(&e->range_wake);
      pthread_cond_wait(&e->range_wake,&e->range_lock);
      continue;
    }
    int64_t at=e->fx_hi;
    char target[8192], fin[8192]; int direct, cross=0, status=0; int64_t total=-1, got=0;
    direct=e->final_url[0]!=0;
    snprintf(target,sizeof target,"%s",direct?e->final_url:e->url);
    int strip=direct&&e->final_cross;
    if(e->source_stream)__atomic_store_n(&e->source_cancel,0,__ATOMIC_RELAXED);
    pthread_mutex_unlock(&e->range_lock);
    uint64_t t0=fx_now_ms();
    int r=rede_fluxo64(target,strip?(e->source_stream && e->final_cross!=2?e->source_public_headers:""):e->headers,at,fx_sink,e,&total,&status,
      e->source_stream?&e->source_cancel:&e->cancelled,fin,&cross,FX_STALL_MS,&got);
    uint64_t ms=fx_now_ms()-t0;
    pthread_mutex_lock(&e->range_lock);
    e->metrics.range_requests++;
    if(total>=0) e->total=total;
    if(!direct && got>0 && fin[0]) { snprintf(e->final_url,sizeof e->final_url,"%s",fin); e->final_cross=cross; }
    if(r==1 || e->fx_stop || cancelled(e) || (e->source_stream && e->fx_req>=0)) continue; /* seek/closing */
    if(r==0) {                                                /* body ended */
      e->fx_attempts=0;
      if(status==200) { e->total=e->fx_hi; e->fx_eof=1; continue; }   /* whole file, no Range */
      if(e->total>=0 && e->fx_hi>=e->total) e->fx_eof=1;
      else if(got==0) e->fx_eof=1;                            /* nothing more: treat as end */
      continue;                                               /* a capped range: go on from fx_hi */
    }
    /* Error. Bytes already received count as progress: the next try resumes at
     * fx_hi. An expired link (401/403/404/410) goes back through the add-on,
     * at most once a minute; anything else retries the same address. */
    if(got>0) e->fx_attempts=0;
    int expired=status==401||status==403||status==404||status==410;
    if(expired && direct && fx_now_ms()-e->fx_resolved_ms>60000u) {
      e->final_url[0]=0; e->fx_resolved_ms=fx_now_ms();
    }
    if(status==416 && e->total>=0 && at>=e->total) { e->fx_eof=1; continue; }
    e->fx_attempts++;
    printf("[dts-range] leitura caiu: at=%lldMiB http=%d depois de %lld KiB em %llu ms, %s; tentativa %d/%d\n",
           (long long)(at>>20),status,(long long)(got>>10),(unsigned long long)ms,
           expired?(e->final_url[0]?"endereco expirado (re-resolve em espera)":"endereco expirado, de novo pelo addon"):"mesmo endereco",
           e->fx_attempts,FX_TRIES);
    fflush(stdout);
    if(e->fx_attempts>=FX_TRIES) {
      e->fx_err=1;
      printf("[dts-range] leitura desistiu em %lldMiB depois de %d tentativas\n",(long long)(at>>20),FX_TRIES);
      fflush(stdout);
      pthread_cond_broadcast(&e->range_wake);
      continue;
    }
    { int wait_s=1<<(e->fx_attempts-1); if(wait_s>15) wait_s=15;
      struct timespec ts; clock_gettime(CLOCK_REALTIME,&ts); ts.tv_sec+=wait_s;
      while(!e->fx_stop && !cancelled(e) && e->fx_req<0 &&
            pthread_cond_timedwait(&e->range_wake,&e->range_lock,&ts)==0) {} }
  }
  pthread_cond_broadcast(&e->range_wake);
  pthread_mutex_unlock(&e->range_lock);
  return NULL;
}
static int read_fluxo_pending(void *opaque,uint8_t *dst,int n,int pending_fd) {
  DtsEngine *e=opaque;
  uint64_t wait_started=0;
  pthread_mutex_lock(&e->range_lock);
  if(!e->fx_thread_ok && !e->fx_stop) {
    if(!e->fx_buf) { if(!e->source_stream)e->fx_cap=(int64_t)g_fx_cap_mib*1024*1024; e->fx_buf=malloc((size_t)e->fx_cap); if(e->fx_buf) rings_add(e->fx_cap); }
    if(!e->fx_buf) { pthread_mutex_unlock(&e->range_lock); return AVERROR(ENOMEM); }
    e->fx_req=e->pos;
    if(pthread_create(&e->fx_thread,NULL,fx_worker,e)) { pthread_mutex_unlock(&e->range_lock); return AVERROR(ENOMEM); }
    e->fx_thread_ok=1;
  }
  for(;;) {
    struct pollfd pending={pending_fd,POLLIN,0};
    if(cancelled(e) || __atomic_load_n(&e->interrupt,__ATOMIC_RELAXED) ||
       (pending_fd>=0 && poll(&pending,1,0)>0)) { pthread_mutex_unlock(&e->range_lock); return AVERROR_EXIT; }
    if(e->total>=0 && e->pos>=e->total) { pthread_mutex_unlock(&e->range_lock); return AVERROR_EOF; }
    if(e->fx_req<0 && e->pos>=e->fx_lo && e->pos<e->fx_hi) {
      int64_t k=e->fx_hi-e->pos; if(k>n) k=n;
      size_t at=(size_t)(e->pos%FX_CAP); if((int64_t)(FX_CAP-at)<k) k=FX_CAP-at;
      memcpy(dst,e->fx_buf+at,(size_t)k); e->pos+=k;
      if(wait_started) { uint64_t w=clock_ns(CLOCK_MONOTONIC); if(w>=wait_started) e->metrics.range_blocked_ns+=w-wait_started; }
      pthread_cond_broadcast(&e->range_wake);         /* room for the stream */
      pthread_mutex_unlock(&e->range_lock);
      return (int)k;
    }
    if(e->fx_req<0 && e->fx_eof && e->pos>=e->fx_hi) { pthread_mutex_unlock(&e->range_lock); return AVERROR_EOF; }
    if(e->fx_req<0 && e->fx_err) { pthread_mutex_unlock(&e->range_lock); return AVERROR(EIO); }
    /* Outside what the stream has or will soon have: start over there. */
    if(e->fx_req<0 && (e->pos<e->fx_lo || e->pos>e->fx_hi+(e->source_stream?0:FX_SKIP))) {
      e->fx_req=e->pos;
      if(e->source_stream)__atomic_store_n(&e->source_cancel,1,__ATOMIC_RELAXED);
      pthread_cond_broadcast(&e->range_wake);
    }
    if(!wait_started) wait_started=clock_ns(CLOCK_MONOTONIC);
    struct timespec ts; clock_gettime(CLOCK_REALTIME,&ts);
    if(pending_fd<0) ts.tv_sec+=1;
    else {
      ts.tv_nsec+=100000000;
      if(ts.tv_nsec>=1000000000) { ts.tv_sec++;ts.tv_nsec-=1000000000; }
    }
    pthread_cond_timedwait(&e->range_wake,&e->range_lock,&ts);
  }
}
static int read_fluxo(void *opaque,uint8_t *dst,int n) {
  return read_fluxo_pending(opaque,dst,n,-1);
}
static int64_t seek_fluxo(void *opaque,int64_t offset,int whence) {
  DtsEngine *e=opaque;
  if (cancelled(e)) return AVERROR_EXIT;
  if (whence==AVSEEK_SIZE) return e->total>=0?e->total:AVERROR(ENOSYS);
  whence &= ~AVSEEK_FORCE;
  pthread_mutex_lock(&e->range_lock);
  int64_t base=whence==SEEK_SET?0:whence==SEEK_CUR?e->pos:whence==SEEK_END?e->total:-1;
  if (base<0 || (offset>0 && base>INT64_MAX-offset) || (offset<0 && offset<-base)) { pthread_mutex_unlock(&e->range_lock); return AVERROR(EINVAL); }
  e->pos=base+offset;
  if(e->source_stream && e->fx_thread_ok &&
     (e->pos<e->fx_lo || e->pos>e->fx_hi)) {
    e->fx_req=e->pos;
    __atomic_store_n(&e->source_cancel,1,__ATOMIC_RELAXED);
  }
  pthread_cond_broadcast(&e->range_wake);
  pthread_mutex_unlock(&e->range_lock);
  return e->pos;
}
static void fx_close(DtsEngine *e) {
  pthread_mutex_lock(&e->range_lock);
  e->fx_stop=1; pthread_cond_broadcast(&e->range_wake);
  if(e->source_stream)__atomic_store_n(&e->source_cancel,1,__ATOMIC_RELAXED);
  pthread_mutex_unlock(&e->range_lock);
  if(e->fx_thread_ok) pthread_join(e->fx_thread,NULL);
  e->fx_thread_ok=0; e->fx_stop=0; e->fx_req=-1; e->fx_eof=e->fx_err=0; e->fx_lo=e->fx_hi=0;
  if(e->fx_buf) rings_add(-e->fx_cap);
  free(e->fx_buf); e->fx_buf=NULL;
}
/* Only the embedded motor's loopback /stream route gets a startup budget.
 * Cues near EOF are optional; media reads and later seeks keep their deadline. */
static int optional_p2p_tail(DtsEngine *e) {
  if(!e->opening || e->total<0 || e->pos<4*1024*1024 ||
     e->pos<e->total-4*1024*1024) return 0;
  const char *port=NULL;
  if(!strncmp(e->url,"http://127.0.0.1:",17)) port=e->url+17;
  else if(!strncmp(e->url,"http://localhost:",17)) port=e->url+17;
  if(!port || *port<'0' || *port>'9') return 0;
  while(*port>='0' && *port<='9') port++;
  return !strncmp(port,"/stream/",8) && port[8];
}
static int read_range(void *opaque,uint8_t *dst,int n) {
  DtsEngine *e=opaque;
  if (cancelled(e) || __atomic_load_n(&e->interrupt,__ATOMIC_RELAXED)) return AVERROR_EXIT;
  if (e->total>=0 && e->pos>=e->total) return AVERROR_EOF;
  if (!e->cache || e->pos<e->cache_start || e->pos-e->cache_start>=e->cache_size) {
    uint64_t wait_started=clock_ns(CLOCK_MONOTONIC);
    if(!start_ranges(e)) return AVERROR(ENOMEM);
    pthread_mutex_lock(&e->range_lock);
    DtsRangeJob *job=NULL;
    for(int i=0;i<RANGE_SLOTS_MAX;i++)
      if(e->ranges[i].state && e->ranges[i].start==e->pos) job=&e->ranges[i];
    if(!job) {
      pthread_mutex_unlock(&e->range_lock);
      /* Seek or short 206: discard obsolete speculative ranges and restart
       * at the exact next byte. Never jump over a short response's tail. */
      clear_ranges(e,"missing");
      pthread_mutex_lock(&e->range_lock); queue_ranges(e);
      for(int i=0;i<RANGE_SLOTS_MAX;i++)
        if(e->ranges[i].state && e->ranges[i].start==e->pos) job=&e->ranges[i];
    }
    if(!job) { pthread_mutex_unlock(&e->range_lock); return AVERROR_EXIT; }
    if(__atomic_load_n(&e->range_live,__ATOMIC_RELAXED))
      __atomic_store_n(&job->urgent,1,__ATOMIC_RELAXED);
    int optional=optional_p2p_tail(e);
    struct timespec deadline;
    if(optional) { clock_gettime(CLOCK_REALTIME,&deadline); deadline.tv_sec++; }
    while(job->state!=3 && !cancelled(e) && !__atomic_load_n(&e->interrupt,__ATOMIC_RELAXED)) {
      if(optional) {
        if(pthread_cond_timedwait(&e->range_wake,&e->range_lock,&deadline)==ETIMEDOUT && job->state!=3) {
          pthread_mutex_unlock(&e->range_lock);
          /* Cancel speculative tail reads too, so the next seek cannot wait
           * for an abandoned request's 15-second transport timeout. */
          clear_ranges(e,"p2p-tail");
          return cancelled(e)?AVERROR_EXIT:AVERROR(EIO);
        }
      } else pthread_cond_wait(&e->range_wake,&e->range_lock);
    }
    if(cancelled(e) || __atomic_load_n(&e->interrupt,__ATOMIC_RELAXED)) { pthread_mutex_unlock(&e->range_lock); return AVERROR_EXIT; }
    long count=job->count; int status=job->status; int64_t total=job->total, end=job->end;
    uint64_t ready_at=clock_ns(CLOCK_MONOTONIC);
    if(ready_at>=wait_started) e->metrics.range_blocked_ns+=ready_at-wait_started;
    if(!job->data || (ready_at>=wait_started && ready_at-wait_started>=UINT64_C(500000000))) {
      int64_t ready=0, contiguous=0, next=e->pos;
      for(int i=0;i<e->range_slots;i++)
        if(e->ranges[i].state==3 && e->ranges[i].data && e->ranges[i].count>0) ready+=e->ranges[i].count;
      for(int k=0;k<e->range_slots;k++) {
        DtsRangeJob *j=NULL;
        for(int i=0;i<e->range_slots;i++)
          if(e->ranges[i].state==3 && e->ranges[i].data && e->ranges[i].count>0 && e->ranges[i].start==next) { j=&e->ranges[i]; break; }
        if(!j) break;
        contiguous+=j->count; next+=j->count;
        if(j->count<j->end-j->start+1) break;
      }
      printf("[range-read] pos=%lld waitMs=%llu http=%d ok=%d readyBytes=%lld contiguousBytes=%lld\n",
             (long long)e->pos,(unsigned long long)((ready_at-wait_started)/1000000),
             status,job->data!=NULL,(long long)ready,(long long)contiguous); fflush(stdout);
    }
    free(e->cache); e->cache=NULL; e->cache_size=0;
    e->cache=job->data; job->data=NULL; job->state=0;
    uint64_t elapsed=job->elapsed;
    e->metrics.range_requests++;
    e->metrics.range_wait_ns+=elapsed;
    if(elapsed>e->metrics.range_max_wait_ns) e->metrics.range_max_wait_ns=elapsed;
    if(count>0) e->metrics.range_bytes+=(uint64_t)count;
    if(e->cache && count>0 && count<end-e->pos+1 &&
        !(total>=0 && e->pos<=total && count>=total-e->pos)) {
      e->metrics.range_short_reads++;
      /* Origins can impose a smaller range size. Match their observed cap
       * after the first short response, avoiding repeated speculative holes. */
      if(count<e->range_bytes) e->range_bytes=count;
    }
    if(total>=0) {
      e->total=total;
      /* The first response discovers EOF after the initial speculative queue. */
      for(int i=0;i<RANGE_SLOTS_MAX;i++)
        if(e->ranges[i].state==1 && e->ranges[i].start>=total) e->ranges[i].state=0;
    }
    queue_ranges(e);
    pthread_mutex_unlock(&e->range_lock);
    if (!e->cache || count<=0 || count>RANGE_BYTES_DV || status!=206) {
      free(e->cache); e->cache=NULL;
      if (status==416 && total>=0 && e->pos>=total) { e->total=total; return AVERROR_EOF; }
      return AVERROR(EIO);
    }
    e->total=total; e->cache_start=e->pos; e->cache_size=count;
  }
  int64_t available=e->cache_size-(e->pos-e->cache_start);
  if (n>available) n=(int)available;
  if(e->pos>INT64_MAX-n) return AVERROR(EOVERFLOW);
  memcpy(dst,e->cache+(size_t)(e->pos-e->cache_start),n); e->pos+=n;
  return n;
}
static int64_t seek_range(void *opaque,int64_t offset,int whence) {
  DtsEngine *e=opaque;
  if (cancelled(e)) return AVERROR_EXIT;
  if (whence==AVSEEK_SIZE) return e->total>=0?e->total:AVERROR(ENOSYS);
  whence &= ~AVSEEK_FORCE;
  int64_t base=whence==SEEK_SET?0:whence==SEEK_CUR?e->pos:whence==SEEK_END?e->total:-1;
  if (base<0 || (offset>0 && base>INT64_MAX-offset) || (offset<0 && offset<-base)) return AVERROR(EINVAL);
  e->pos=base+offset;
  if(!e->cache || e->pos<e->cache_start || e->pos-e->cache_start>=e->cache_size)
    clear_ranges(e,"seek");
  return e->pos;
}
static int64_t ns(int64_t value,AVRational tb) {
  return value==AV_NOPTS_VALUE?INT64_MIN:av_rescale_q(value,tb,NS);
}
static void copy_metadata(char *out,size_t size,AVDictionary *m,const char *key) {
  AVDictionaryEntry *v=av_dict_get(m,key,NULL,0);
  snprintf(out,size,"%s",v?v->value:"");
}
static void close_media(DtsEngine *e) {
  fx_close(e);
  pthread_mutex_lock(&e->range_lock);
  e->range_stop=1;
  for(int i=0;i<RANGE_SLOTS_MAX;i++) __atomic_store_n(&e->ranges[i].cancel,1,__ATOMIC_RELAXED);
  pthread_cond_broadcast(&e->range_wake);
  pthread_mutex_unlock(&e->range_lock);
  for(int i=0;i<e->range_threads;i++) pthread_join(e->workers[i].thread,NULL);
  for(int i=0;i<RANGE_SLOTS_MAX;i++) free(e->ranges[i].data);
  pthread_mutex_lock(&e->range_lock);
  memset(e->ranges,0,sizeof e->ranges); e->range_threads=0; e->range_stop=0;
  pthread_mutex_unlock(&e->range_lock);
  av_packet_free(&e->in); av_packet_free(&e->out);
  av_frame_free(&e->decoded); av_frame_free(&e->encoded);
  avcodec_free_context(&e->dec); avcodec_free_context(&e->enc);
  av_bsf_free(&e->bsf); swr_free(&e->swr); av_audio_fifo_free(e->fifo); e->fifo=NULL;
  av_freep(&e->dv7_buffer); e->dv7_capacity=0; e->dv7=0;
  av_channel_layout_uninit(&e->input_layout);
  avformat_close_input(&e->fmt);
  if (e->io) { av_freep(&e->io->buffer); avio_context_free(&e->io); }
  free(e->cache); e->cache=NULL; free(e->url); e->url=NULL;
  free(e->headers); e->headers=NULL;
  free(e->source_public_headers); e->source_public_headers=NULL;
}
DtsEngine *dts_engine_create(void) {
  DtsEngine *e=calloc(1,sizeof(*e));
  if(e) { e->total=-1; e->range_bytes=RANGE_BYTES; e->range_slots=RANGE_SLOTS; pthread_mutex_init(&e->range_lock,NULL); pthread_cond_init(&e->range_wake,NULL); }
  return e;
}
void dts_engine_buffer_info(DtsEngine *e,DtsBufferInfo *b) {
  if(!b)return;
  memset(b,0,sizeof *b);
  if(!e)return;
  pthread_mutex_lock(&e->range_lock);
  if(e->source_stream) {
    b->position=e->pos;b->total=e->total;b->capacity_bytes=e->fx_buf?e->fx_cap:0;
    if(e->fx_req<0 && e->pos>=e->fx_lo && e->pos<=e->fx_hi)b->ready_bytes=e->fx_hi-e->pos;
    b->contiguous_bytes=b->ready_bytes;
  } else if(!e->remux && e->range_threads && !cancelled(e)) {
    // Called only by the conversion worker: it owns pos/cache. Completed
    // speculative ranges belong to the network workers and need range_lock.
    b->position=e->pos; b->total=e->total;
    b->capacity_bytes=(int64_t)(e->range_slots+1)*e->range_bytes;
    if(e->cache && e->pos>=e->cache_start && e->pos-e->cache_start<e->cache_size)
      b->ready_bytes=e->cache_size-(e->pos-e->cache_start);
    b->contiguous_bytes=b->ready_bytes;
    for(int i=0;i<e->range_slots;i++) {
      DtsRangeJob *j=&e->ranges[i];
      if(j->state==3 && j->data && j->count>0 && j->start>=e->pos)
        b->ready_bytes+=j->count;
    }
    int64_t next=e->pos+b->contiguous_bytes;
    for(int k=0;k<e->range_slots;k++) {
      DtsRangeJob *found=NULL;
      for(int i=0;i<e->range_slots;i++) {
        DtsRangeJob *j=&e->ranges[i];
        if(j->state==3 && j->data && j->count>0 && j->start==next) { found=j;break; }
      }
      if(!found)break;
      b->contiguous_bytes+=found->count;next+=found->count;
      if(found->count<found->end-found->start+1)break;
    }
  }
  pthread_mutex_unlock(&e->range_lock);
}
static int buffer_mib(int v) { return v == 128 || v == 256 ? v : 64; }
DtsEngine *dts_engine_source_create(const char *url,const char *headers,int mib) {
  DtsEngine *e=dts_engine_create();
  if (!e) return NULL;
  e->url=strdup(url); e->headers=strdup(headers?headers:"");
  e->source_public_headers=rede_range_headers(headers,1);
  e->source_stream=1;
  e->fx_cap=(int64_t)buffer_mib(mib)*1024*1024; e->fx_req=-1;
  if(!e->url || !e->headers || !e->source_public_headers) { dts_engine_destroy(e); return NULL; }
  return e;
}
int dts_engine_source_read(DtsEngine *e,int64_t offset,unsigned char *data,int n,int pending_fd) {
  if (!e || offset<0 || !data || n<1) return -1;
  /* One bounded probe discovers the finite file size before the loopback
   * response is sent. It finishes before the single streaming lane starts.
   * Do not start the old speculative four-lane queue just to inspect a header. */
  if(e->total<0 && !e->fx_thread_ok) {
    long got=0; int status=0,cross=0; int64_t total=-1; char fin[8192];
    char *probe=rede_baixar_trecho64_final(e->url,e->headers,0,255,
      &got,&total,&status,&e->cancelled,fin,&cross);
    if(!probe || total<=0) { free(probe); return -1; }
    free(probe);
    pthread_mutex_lock(&e->range_lock);
    e->total=total;
    if(fin[0]) { snprintf(e->final_url,sizeof e->final_url,"%s",fin); e->final_cross=cross; }
    pthread_mutex_unlock(&e->range_lock);
  }
  if(seek_fluxo(e,offset,SEEK_SET)<0) return -1;
  return read_fluxo_pending(e,data,n,pending_fd);
}
int64_t dts_engine_source_total(DtsEngine *e) {
  if(!e)return -1;
  pthread_mutex_lock(&e->range_lock); int64_t total=e->total; pthread_mutex_unlock(&e->range_lock);
  return total;
}
void dts_engine_dv(DtsEngine *e, int enabled) { if (e) e->dv = !!enabled; }
void dts_engine_allow_p7(DtsEngine *e, int enabled) { if (e) e->allow_p7 = !!enabled; }
int dts_engine_available(void) {
  return avcodec_find_decoder(AV_CODEC_ID_DTS) && avcodec_find_encoder(AV_CODEC_ID_AAC) && av_bsf_get_by_name("h264_mp4toannexb")
      && av_bsf_get_by_name("hevc_mp4toannexb");
}
const DtsMediaInfo *dts_engine_info(DtsEngine *e) { return e?&e->info:NULL; }
int dts_engine_has_core(DtsEngine *e) { return e?e->has_core:0; }
const void *dts_engine_subtitle_parameters(DtsEngine *e,int i) {
  if (!e || !e->fmt || i<0 || (unsigned)i>=e->fmt->nb_streams) return NULL;
  AVCodecParameters *p=e->fmt->streams[i]->codecpar;
  return p->codec_type==AVMEDIA_TYPE_SUBTITLE?p:NULL;
}
const char *dts_engine_error(DtsEngine *e) { return e?e->error:"No DTS engine"; }
void dts_engine_cancel(DtsEngine *e) {
  if(!e) return;
  __atomic_store_n(&e->cancelled,1,__ATOMIC_RELAXED);
  pthread_mutex_lock(&e->range_lock);
  __atomic_store_n(&e->source_cancel,1,__ATOMIC_RELAXED);
  for(int i=0;i<RANGE_SLOTS_MAX;i++) __atomic_store_n(&e->ranges[i].cancel,1,__ATOMIC_RELAXED);
  pthread_cond_broadcast(&e->range_wake); pthread_mutex_unlock(&e->range_lock);
}
void dts_engine_destroy(DtsEngine *e) {
  if(e) { close_media(e); pthread_cond_destroy(&e->range_wake); pthread_mutex_destroy(&e->range_lock); free(e); }
}
static int reset_encoder(DtsEngine *e) {
  /* Encoders are reopened after seek; encoder flush is not supported by AAC. */
  enum AVCodecID id=e->enc->codec_id;
  AVChannelLayout layout={0}; av_channel_layout_copy(&layout,&e->enc->ch_layout);
  avcodec_free_context(&e->enc);
  const AVCodec *codec=avcodec_find_encoder(id);
  e->enc=avcodec_alloc_context3(codec);
  if (!e->enc) { av_channel_layout_uninit(&layout); return fail(e,"Audio encoder allocation",0); }
  av_channel_layout_copy(&e->enc->ch_layout,&layout); av_channel_layout_uninit(&layout);
  e->enc->sample_rate=48000; e->enc->time_base=(AVRational){1,48000};
  e->enc->sample_fmt=AV_SAMPLE_FMT_FLTP;
  if(id==AV_CODEC_ID_AC3) e->enc->bit_rate=e->enc->ch_layout.nb_channels>2?640000:192000;
  else { e->enc->bit_rate=192000; e->enc->profile=AV_PROFILE_AAC_LOW; }
  int r=avcodec_open2(e->enc,codec,NULL);
  return r<0?fail(e,"Audio encoder open",r):0;
}
int dts_engine_open(DtsEngine *e,const char *url,const char *headers,int audio,int core,double start) {
  if (!e || !url || !*url) return -1;
  __atomic_store_n(&e->range_live,0,__ATOMIC_RELAXED);
  close_media(e); memset(&e->info,0,sizeof(e->info)); e->error[0]=0;
  memset(&e->metrics,0,sizeof(e->metrics));
#ifdef NV_DTS_DEBUG
  memset(&e->logged_metrics,0,sizeof(e->logged_metrics)); e->logged_at_ns=0;
#endif
  e->pos=0; e->total=-1; e->cache_size=0; e->has_core=0;
  e->range_bytes=e->dv?RANGE_BYTES_DV:RANGE_BYTES; e->range_slots=e->dv?RANGE_SLOTS_DV:RANGE_SLOTS;
  if(g_ahead_mib>0) { e->range_bytes=RANGE_BYTES_DV; e->range_slots=(g_ahead_mib+3)/4; }
  e->final_url[0]=0; e->final_cross=0; e->passthrough=0; e->remux=0; e->opening=1;
  e->demux_eof=e->decoder_eof=e->resampler_eof=e->encoder_sent_eof=e->timeline=0;
  if (cancelled(e)) return fail(e,"Cancelled",0);
  if (audio < -1 || !isfinite(start) || start<0 || start>INT64_MAX/1000000000.0)
    return fail(e,"Invalid DTS conversion options",0);
  e->url=strdup(url); e->headers=strdup(headers?headers:"");
  e->fmt=avformat_alloc_context();
  unsigned char *buffer=av_malloc(32768);
  if (!e->url || !e->headers || !e->fmt || !buffer) { av_free(buffer); return fail(e,"Demux allocation",0); }
  e->io=avio_alloc_context(buffer,32768,0,e,read_range,NULL,seek_range);
  if (!e->io) { av_free(buffer); return fail(e,"IO allocation",0); }
  e->fmt->pb=e->io; e->fmt->flags|=AVFMT_FLAG_CUSTOM_IO;
  e->fmt->interrupt_callback=(AVIOInterruptCB){cancelled,e};
  /* Never give libavformat the signed URL; custom AVIO is its sole transport.
   * No decoder logs are enabled, including malformed metadata containing URLs. */
  av_log_set_level(AV_LOG_QUIET);
  AVDictionary *opts=NULL;
  av_dict_set(&opts,"format_whitelist","matroska,webm,mov",0);
  av_dict_set(&opts,"probesize","4194304",0);
  av_dict_set(&opts,"analyzeduration","5000000",0);
  int r=avformat_open_input(&e->fmt,NULL,NULL,&opts); av_dict_free(&opts);
  if(r<0) return fail(e,"Container open",r);
  r=avformat_find_stream_info(e->fmt,NULL);
  e->opening=0;
  if(r<0) return fail(e,"Container metadata",r);
  if(e->fmt->duration==AV_NOPTS_VALUE || e->fmt->duration<=0)
    return fail(e,"Finite VOD duration required",0);
  e->info.audio_stream=e->info.video_stream=-1;
  for(unsigned i=0;i<e->fmt->nb_streams;i++) {
    AVStream *s=e->fmt->streams[i]; AVCodecParameters *p=s->codecpar;
    if(av_packet_side_data_get(p->coded_side_data,p->nb_coded_side_data,AV_PKT_DATA_ENCRYPTION_INIT_INFO)
       || p->codec_tag==MKTAG('e','n','c','a') || p->codec_tag==MKTAG('e','n','c','v'))
      return fail(e,"Encrypted media is unsupported",0);
    if(p->codec_type==AVMEDIA_TYPE_VIDEO && e->info.video_stream<0 && !(s->disposition&AV_DISPOSITION_ATTACHED_PIC))
      e->info.video_stream=i;
    /* Dolby Vision path: AC-3/E-AC-3 go to the TV untouched (passthrough). */
    if((p->codec_id==AV_CODEC_ID_DTS || (e->dv && (p->codec_id==AV_CODEC_ID_AC3 || p->codec_id==AV_CODEC_ID_EAC3)))
       && e->info.audio_stream<0 && (audio<0 || audio==(int)i)) e->info.audio_stream=i;
    int kind=p->codec_type==AVMEDIA_TYPE_VIDEO?DTS_VIDEO:p->codec_type==AVMEDIA_TYPE_AUDIO?DTS_AUDIO:p->codec_type==AVMEDIA_TYPE_SUBTITLE?DTS_SUBTITLE:0;
    if(kind && e->info.n_tracks<DTS_TRACK_MAX) {
      DtsTrack *t=&e->info.tracks[e->info.n_tracks++]; t->kind=kind; t->stream_index=i; t->stream_id=s->id;
      t->channels=p->ch_layout.nb_channels; t->forced=!!(s->disposition&AV_DISPOSITION_FORCED);
      snprintf(t->codec,sizeof(t->codec),"%s",avcodec_get_name(p->codec_id));
      copy_metadata(t->language,sizeof(t->language),s->metadata,"language");
      copy_metadata(t->title,sizeof(t->title),s->metadata,"title");
    }
  }
  if(e->info.audio_stream<0 || e->info.video_stream<0) return fail(e,"Selected DTS track or video absent",0);
  AVStream *v=e->fmt->streams[e->info.video_stream]; AVCodecParameters *vp=v->codecpar;
  const char *filter=vp->codec_id==AV_CODEC_ID_H264?"h264_mp4toannexb":vp->codec_id==AV_CODEC_ID_HEVC?"hevc_mp4toannexb":NULL;
  if(!filter && vp->codec_id!=AV_CODEC_ID_VP9 && vp->codec_id!=AV_CODEC_ID_AV1)
    return fail(e,"Compressed video codec unsupported",0);
  const AVPacketSideData *dv=av_packet_side_data_get(vp->coded_side_data,vp->nb_coded_side_data,AV_PKT_DATA_DOVI_CONF);
  if(dv && dv->size>=sizeof(AVDOVIDecoderConfigurationRecord)) {
    const AVDOVIDecoderConfigurationRecord *conf=(const AVDOVIDecoderConfigurationRecord *)dv->data;
    e->info.dovi_profile=conf->dv_profile; e->info.dovi_level=conf->dv_level;
    e->info.dovi_rpu_present=conf->rpu_present_flag;
    e->info.dovi_el_present=conf->el_present_flag;
    e->info.dovi_bl_present=conf->bl_present_flag;
    e->info.dovi_bl_compatibility_id=conf->dv_bl_signal_compatibility_id;
    if ((conf->dv_profile == 7 && !e->allow_p7) ||
        !nv_dv7_profile(conf->dv_profile,conf->el_present_flag,
                       conf->bl_present_flag,conf->rpu_present_flag) || vp->codec_id!=AV_CODEC_ID_HEVC)
      return fail(e,"Dolby Vision layer/profile unsupported",0);
    if (conf->dv_profile == 7) {
      e->dv7=1;
      e->info.dovi_profile=8; e->info.dovi_el_present=0;
      e->info.dovi_bl_compatibility_id=1;
      printf("[dv7] experimental P7 -> P8.1: RPU converted, enhancement layer discarded\n");
    }
    /* mp4toannexb preserves HEVC RPU NAL units; no video re-encode occurs. */
  }
  e->info.width=vp->width; e->info.height=vp->height;
  snprintf(e->info.video_codec,sizeof(e->info.video_codec),"%s",avcodec_get_name(vp->codec_id));
  e->info.fps_num=v->avg_frame_rate.num; e->info.fps_den=v->avg_frame_rate.den;
  e->info.color_primaries=vp->color_primaries; e->info.color_transfer=vp->color_trc; e->info.color_matrix=vp->color_space;
  snprintf(e->info.hdr,sizeof(e->info.hdr),"%s",e->info.dovi_profile?"DolbyVision":vp->color_trc==AVCOL_TRC_SMPTE2084?"PQ":vp->color_trc==AVCOL_TRC_ARIB_STD_B67?"HLG":"SDR");
  if(filter) {
    const AVBitStreamFilter *video_filter=av_bsf_get_by_name(filter);
    if(!video_filter) return fail(e,"Video filter absent",0);
    r=av_bsf_alloc(video_filter,&e->bsf);
    if(r<0) return fail(e,"Video filter allocation",r);
    avcodec_parameters_copy(e->bsf->par_in,vp); e->bsf->time_base_in=v->time_base;
    if((r=av_bsf_init(e->bsf))<0) return fail(e,"Video filter open",r);
  }
  AVCodecParameters *ap=e->fmt->streams[e->info.audio_stream]->codecpar;
  e->passthrough=ap->codec_id!=AV_CODEC_ID_DTS;
  if(e->passthrough) {
    e->in=av_packet_alloc(); e->out=av_packet_alloc();
    if(!e->in || !e->out) return fail(e,"Audio queue allocation",0);
    e->info.channels=ap->ch_layout.nb_channels>0?ap->ch_layout.nb_channels:6;
    e->info.sample_rate=ap->sample_rate>0?ap->sample_rate:48000;
    snprintf(e->info.audio_codec,sizeof(e->info.audio_codec),"%s",avcodec_get_name(ap->codec_id));
    e->info.duration_ns=e->fmt->duration==AV_NOPTS_VALUE?0:av_rescale_q(e->fmt->duration,AV_TIME_BASE_Q,NS);
    e->info.start_ns=e->fmt->start_time==AV_NOPTS_VALUE?0:av_rescale_q(e->fmt->start_time,AV_TIME_BASE_Q,NS);
    e->target_ns=(int64_t)(start*1000000000.0);
    return start>0?dts_engine_seek(e,start):0;
  }
  const AVCodec *decoder=avcodec_find_decoder(AV_CODEC_ID_DTS);
  if(!decoder) return fail(e,"DTS decoder absent",0);
  e->dec=avcodec_alloc_context3(decoder);
  if(!e->dec) return fail(e,"DTS decoder allocation",0);
  avcodec_parameters_to_context(e->dec,ap);
  e->dec->pkt_timebase=e->fmt->streams[e->info.audio_stream]->time_base;
  /* Corrupt HD extensions must reach the worker error path; an explicit
   * core-only reopen decides whether to discard the failed extension. */
  e->dec->err_recognition=AV_EF_EXPLODE;
  if(core && av_opt_set_int(e->dec->priv_data,"core_only",1,0)<0) return fail(e,"DTS core-only unavailable",0);
  if((r=avcodec_open2(e->dec,decoder,NULL))<0) return fail(e,"DTS decoder open",r);
  /* AC3 5.1 only when asked and the build has the encoder; else stereo AAC. */
  const AVCodec *encoder=g_surround_ac3?avcodec_find_encoder(AV_CODEC_ID_AC3):NULL;
  int out_channels=2;
  if(encoder) {
    int in_channels=ap->ch_layout.nb_channels;
    out_channels=(in_channels==0 || in_channels>2)?6:2;
  } else encoder=avcodec_find_encoder(AV_CODEC_ID_AAC);
  if(!encoder) return fail(e,"Audio encoder absent",0);
  e->enc=avcodec_alloc_context3(encoder);
  if(!e->enc) return fail(e,"Audio encoder absent",0);
  e->enc->codec_id=encoder->id;
  av_channel_layout_default(&e->enc->ch_layout,out_channels);
  if(reset_encoder(e)<0) return -1;
  e->fifo=av_audio_fifo_alloc(e->enc->sample_fmt,e->enc->ch_layout.nb_channels,4096);
  e->in=av_packet_alloc(); e->out=av_packet_alloc(); e->decoded=av_frame_alloc(); e->encoded=av_frame_alloc();
  if(!e->fifo || !e->in || !e->out || !e->decoded || !e->encoded) return fail(e,"Audio queue allocation",0);
  e->info.channels=e->enc->ch_layout.nb_channels; e->info.sample_rate=48000;
  snprintf(e->info.audio_codec,sizeof(e->info.audio_codec),"%s",avcodec_get_name(e->enc->codec_id));
  e->info.duration_ns=e->fmt->duration==AV_NOPTS_VALUE?0:av_rescale_q(e->fmt->duration,AV_TIME_BASE_Q,NS);
  e->info.start_ns=e->fmt->start_time==AV_NOPTS_VALUE?0:av_rescale_q(e->fmt->start_time,AV_TIME_BASE_Q,NS);
  e->target_ns=(int64_t)(start*1000000000.0);
  return start>0?dts_engine_seek(e,start):0;
}
/* Samsung remux (src/dts/dts_remux.c): only the container over the ranged
 * reader. No stream info probe (MKV and MP4 headers carry what the remux needs,
 * and probing would download seconds of 4K video before the first byte goes
 * out), no codec, no track choice: the caller reads dts_engine_avformat(). */
int dts_engine_open_demux(DtsEngine *e,const char *url,const char *headers) {
  if (!e || !url || !*url) return -1;
  __atomic_store_n(&e->range_live,0,__ATOMIC_RELAXED);
  close_media(e); memset(&e->info,0,sizeof(e->info)); e->error[0]=0;
  memset(&e->metrics,0,sizeof(e->metrics));
  e->pos=0; e->total=-1; e->cache_size=0; e->has_core=0;
  e->remux=1; e->range_bytes=RANGE_BYTES_DV; e->range_slots=RANGE_SLOTS_DV;
  e->final_url[0]=0; e->final_cross=0; e->passthrough=0;
  e->demux_eof=e->decoder_eof=e->resampler_eof=e->encoder_sent_eof=e->timeline=0;
  if (cancelled(e)) return fail(e,"Cancelled",0);
  e->url=strdup(url); e->headers=strdup(headers?headers:"");
  e->fmt=avformat_alloc_context();
  unsigned char *buffer=av_malloc(65536);
  if (!e->url || !e->headers || !e->fmt || !buffer) { av_free(buffer); return fail(e,"Demux allocation",0); }
  e->fx_req=-1;
  e->io=avio_alloc_context(buffer,65536,0,e,read_fluxo,NULL,seek_fluxo);
  if (!e->io) { av_free(buffer); return fail(e,"IO allocation",0); }
  e->fmt->pb=e->io; e->fmt->flags|=AVFMT_FLAG_CUSTOM_IO;
  e->fmt->interrupt_callback=(AVIOInterruptCB){cancelled,e};
  AVDictionary *opts=NULL;
  av_dict_set(&opts,"format_whitelist","matroska,webm,mov",0);
  av_dict_set(&opts,"probesize","1048576",0);
  int r=avformat_open_input(&e->fmt,NULL,NULL,&opts); av_dict_free(&opts);
  if(r<0) return fail(e,"Container open",r);
  if(e->fmt->duration==AV_NOPTS_VALUE || e->fmt->duration<=0) {
    /* MOV fills only the stream durations (the container's comes from
     * find_stream_info, which this path skips because it reads packets). */
    int64_t d=0;
    for(unsigned i=0;i<e->fmt->nb_streams;i++) {
      AVStream *s=e->fmt->streams[i];
      if(s->duration>0) { int64_t v=av_rescale_q(s->duration,s->time_base,AV_TIME_BASE_Q); if(v>d) d=v; }
    }
    if(d>0) e->fmt->duration=d;
  }
  if(e->fmt->duration==AV_NOPTS_VALUE || e->fmt->duration<=0)
    return fail(e,"Finite VOD duration required",0);
  e->info.duration_ns=av_rescale_q(e->fmt->duration,AV_TIME_BASE_Q,NS);
  e->info.start_ns=e->fmt->start_time==AV_NOPTS_VALUE?0:av_rescale_q(e->fmt->start_time,AV_TIME_BASE_Q,NS);
  return 0;
}
void *dts_engine_avformat(DtsEngine *e) { return e?e->fmt:NULL; }
void dts_engine_interrupt(DtsEngine *e,int on) {
  if(!e) return;
  __atomic_store_n(&e->interrupt,on?1:0,__ATOMIC_RELAXED);
  pthread_mutex_lock(&e->range_lock);
  /* A new run reads again: a reader that gave up (fx_err) gets a fresh set of
   * tries, from the run's own seek. */
  if(!on && e->fx_err) { e->fx_err=0; e->fx_attempts=0; e->fx_req=e->pos; }
  pthread_cond_broadcast(&e->range_wake);
  pthread_mutex_unlock(&e->range_lock);
}
/* Total size of the source, -1 while unknown (first range not answered). */
int64_t dts_engine_size(DtsEngine *e) { return e?e->total:-1; }
static int resample(DtsEngine *e,AVFrame *f) {
  int r;
  if(f && (!e->swr || e->input_rate!=f->sample_rate || e->input_format!=f->format || av_channel_layout_compare(&e->input_layout,&f->ch_layout))) {
    /* Layout changes midstream need a new playback session; no silent remapping. */
    if(e->swr) return fail(e,"DTS layout changed midstream",0);
    e->input_rate=f->sample_rate; e->input_format=f->format;
    av_channel_layout_copy(&e->input_layout,&f->ch_layout);
    r=swr_alloc_set_opts2(&e->swr,&e->enc->ch_layout,e->enc->sample_fmt,48000,&f->ch_layout,f->format,f->sample_rate,0,NULL);
    if(r<0 || (r=swr_init(e->swr))<0) return fail(e,"Channel resampler open",r);
  }
  if(!e->swr) return 0;
  if(f && e->timeline && f->best_effort_timestamp!=AV_NOPTS_VALUE) {
    int64_t incoming=av_rescale_q(f->best_effort_timestamp,e->dec->pkt_timebase,e->enc->time_base);
    int64_t expected=e->sample_pts+av_audio_fifo_size(e->fifo)
        +av_rescale_rnd(swr_get_delay(e->swr,e->input_rate),48000,e->input_rate,AV_ROUND_NEAR_INF);
    if (incoming>expected+4800 || incoming<expected-4800) {
#ifndef NV_DTS_WEBOS_RECOVERY
      return fail(e,DTS_ENGINE_TIMESTAMP_DISCONTINUITY,0);
#else
      /* Numeric diagnostics survive export redaction; no source/header data. */
      printf("[dts] timestamp-gap incomingMs=%.3f expectedMs=%.3f deltaMs=%.3f "
             "fifoSamples=%d delaySamples=%lld frameSamples=%d inputRate=%d targetMs=%.3f\n",
             (double)incoming/48.0, (double)expected/48.0,
             ((double)incoming-(double)expected)/48.0,
             av_audio_fifo_size(e->fifo), (long long)swr_get_delay(e->swr,e->input_rate),
             f->nb_samples,e->input_rate,(double)e->target_ns/1e6);
      fflush(stdout);
      /* A small FORWARD hole can be represented by silence without retiming
       * video or restarting the native pipeline. Keep the 100 ms jitter guard;
       * never conceal backward jumps, large gaps or a sustained broken clock.
       * At most four holes / one second of silence per 30 s of media progress.
       * swr_inject_silence takes INPUT samples; its buffered delay is included
       * in the output capacity below (also at 44.1/96 kHz). */
      int64_t gap=incoming-expected;
      const char *reason=NULL;
      if(gap<0) reason="backward";
      else if(gap>24000) reason="too-large";
      else {
        if(e->pad_count && expected-e->pad_anchor>=30LL*48000)
          e->pad_count=e->pad_samples=0;
        if(e->pad_count>=4 || gap>48000-e->pad_samples) reason="budget";
      }
      if(reason) {
        printf("[dts] timestamp-reject reason=%s pads=%d padMs=%.3f\n",
               reason,e->pad_count,(double)e->pad_samples/48.0);
        fflush(stdout);
        return fail(e,DTS_ENGINE_TIMESTAMP_DISCONTINUITY,0);
      }
      int input=(int)av_rescale_rnd(gap,e->input_rate,48000,AV_ROUND_NEAR_INF);
      r=swr_inject_silence(e->swr,input);
      if(r<0) {
        printf("[dts] timestamp-reject reason=inject code=%d\n",r);
        fflush(stdout);
        return fail(e,DTS_ENGINE_TIMESTAMP_DISCONTINUITY,0);
      }
      if(!e->pad_count) e->pad_anchor=expected;
      e->pad_count++; e->pad_samples+=(int)gap;
      printf("[dts] timestamp-pad deltaMs=%.3f inputSamples=%d rate=%d "
             "pads=%d padMs=%.3f\n",(double)gap/48.0,input,e->input_rate,
             e->pad_count,(double)e->pad_samples/48.0);
      fflush(stdout);
#endif
    }
  }
  int capacity=(int)av_rescale_rnd(swr_get_delay(e->swr,e->input_rate)+(f?f->nb_samples:0),48000,e->input_rate,AV_ROUND_UP)+32;
  uint8_t **data=NULL;
  r=av_samples_alloc_array_and_samples(&data,NULL,e->enc->ch_layout.nb_channels,capacity,e->enc->sample_fmt,0);
  if(r<0) return fail(e,"Resample buffer",r);
  int count=swr_convert(e->swr,data,capacity,f?(const uint8_t **)f->extended_data:NULL,f?f->nb_samples:0);
  if(count>=0 && f && !e->timeline) {
    int64_t pts=f->best_effort_timestamp;
    if(pts==AV_NOPTS_VALUE) { av_freep(&data[0]); av_freep(&data); return fail(e,"DTS sample timestamp absent",0); }
    e->sample_pts=av_rescale_q(pts,e->dec->pkt_timebase,e->enc->time_base); e->timeline=1;
    e->pad_anchor=e->sample_pts; e->pad_count=e->pad_samples=0;
  }
  if(count>0 && av_audio_fifo_write(e->fifo,(void **)data,count)!=count) count=AVERROR(ENOMEM);
  av_freep(&data[0]); av_freep(&data);
  return count<0?fail(e,"Audio resample",count):count;
}
static int encode_fifo(DtsEngine *e,int final) {
  int size=av_audio_fifo_size(e->fifo), count=e->enc->frame_size;
  if(size<count && (!final || size==0)) return 0;
  av_frame_unref(e->encoded);
  e->encoded->format=e->enc->sample_fmt; e->encoded->sample_rate=48000;
  av_channel_layout_copy(&e->encoded->ch_layout,&e->enc->ch_layout);
  e->encoded->nb_samples=count; e->encoded->pts=e->sample_pts;
  int r=av_frame_get_buffer(e->encoded,0);
  if(r<0) return fail(e,"Encoder sample buffer",r);
  int actual=size<count?size:count;
  av_samples_set_silence(e->encoded->extended_data,0,count,e->enc->ch_layout.nb_channels,e->enc->sample_fmt);
  if(av_audio_fifo_read(e->fifo,(void **)e->encoded->extended_data,actual)!=actual) return fail(e,"Audio queue read",0);
  e->sample_pts+=count;
  r=avcodec_send_frame(e->enc,e->encoded);
  return r<0?fail(e,"Audio encode",r):1;
}
static int frame_out(DtsEngine *e,DtsFrame *f,int kind,AVRational tb) {
  f->kind=kind; f->stream_index=kind==DTS_SUBTITLE?e->out->stream_index:kind==DTS_AUDIO?e->info.audio_stream:e->info.video_stream;
  f->data=e->out->data; f->size=e->out->size;
  f->pts_ns=ns(e->out->pts,tb); f->dts_ns=ns(e->out->dts,tb); f->duration_ns=ns(e->out->duration,tb);
  return 1;
}
/* Passthrough: demux only. Video still crosses the Annex-B filter; the selected
 * audio packets leave as they are in the file, trimmed to the seek target. */
static int next_frame_passthrough(DtsEngine *e,DtsFrame *f) {
  av_packet_unref(e->out);
  for(;;) {
    int r;
    if(cancelled(e)) return fail(e,"Cancelled",0);
    if(e->bsf) {
      r=av_bsf_receive_packet(e->bsf,e->out);
      if(r==0) return frame_out(e,f,DTS_VIDEO,e->bsf->time_base_out);
      if(r==AVERROR_EOF) return 0;
      if(r!=AVERROR(EAGAIN)) return fail(e,"Compressed video output",r);
    }
    if(e->demux_eof) return 0;
    r=av_read_frame(e->fmt,e->in);
    if(r==AVERROR_EOF) {
      e->demux_eof=1;
      if(e->bsf && (r=av_bsf_send_packet(e->bsf,NULL))<0) return fail(e,"Video filter drain",r);
      continue;
    }
    if(r<0) return fail(e,"Container read",r);
    if(av_packet_get_side_data(e->in,AV_PKT_DATA_ENCRYPTION_INFO,NULL)) { av_packet_unref(e->in); return fail(e,"Encrypted packet unsupported",0); }
    AVRational tb=e->fmt->streams[e->in->stream_index]->time_base;
    if(e->fmt->streams[e->in->stream_index]->codecpar->codec_type==AVMEDIA_TYPE_SUBTITLE) {
      av_packet_move_ref(e->out,e->in);
      return frame_out(e,f,DTS_SUBTITLE,tb);
    }
    if(e->in->stream_index==e->info.audio_stream) {
      if(e->in->pts==AV_NOPTS_VALUE || ns(e->in->pts+e->in->duration,tb)<=e->target_ns) { av_packet_unref(e->in); continue; }
      av_packet_move_ref(e->out,e->in);
      return frame_out(e,f,DTS_AUDIO,tb);
    }
    if(e->in->stream_index==e->info.video_stream) {
      if(!e->bsf) { av_packet_move_ref(e->out,e->in); return frame_out(e,f,DTS_VIDEO,tb); }
      r=av_bsf_send_packet(e->bsf,e->in);
      av_packet_unref(e->in);
      if(r<0) return fail(e,"Compressed video input",r);
      continue;
    }
    av_packet_unref(e->in);
  }
}
static int next_frame(DtsEngine *e,DtsFrame *f) {
  if(e && f && e->passthrough && e->in && e->out) return next_frame_passthrough(e,f);
  if(!e || !f || !e->enc) return -1;
  av_packet_unref(e->out);
  for(;;) {
    if(cancelled(e)) return fail(e,"Cancelled",0);
    int r=avcodec_receive_packet(e->enc,e->out);
    if(r==0) {
      /* Delay appears in packet timestamps; trim only complete preroll packets. */
      int64_t end=ns(e->out->pts+e->out->duration,e->enc->time_base);
      if(end<=e->target_ns) { av_packet_unref(e->out); continue; }
      return frame_out(e,f,DTS_AUDIO,e->enc->time_base);
    }
    if(r==AVERROR_EOF) return 0;
    if(r!=AVERROR(EAGAIN)) return fail(e,"Audio output",r);
    if(e->bsf) {
      r=av_bsf_receive_packet(e->bsf,e->out);
      if(r==0) return frame_out(e,f,DTS_VIDEO,e->bsf->time_base_out);
      if(r!=AVERROR(EAGAIN) && r!=AVERROR_EOF) return fail(e,"Compressed video output",r);
    }
    r=encode_fifo(e,e->resampler_eof);
    if(r<0) return -1;
    if(r>0) continue;
    if(!e->decoder_eof) {
      r=avcodec_receive_frame(e->dec,e->decoded);
      if(r==0) { r=resample(e,e->decoded); av_frame_unref(e->decoded); if(r<0) return -1; continue; }
      if(r==AVERROR_EOF) e->decoder_eof=1;
      else if(r!=AVERROR(EAGAIN)) return fail(e,"DTS decode",r);
    }
    if(e->decoder_eof && !e->resampler_eof) {
      r=resample(e,NULL); if(r<0) return -1; if(r==0) e->resampler_eof=1; continue;
    }
    if(e->resampler_eof && !e->encoder_sent_eof) {
      if((r=avcodec_send_frame(e->enc,NULL))<0) return fail(e,"Audio encoder drain",r);
      e->encoder_sent_eof=1; continue;
    }
    if(e->demux_eof) return fail(e,"Unexpected DTS drain state",0);
    r=av_read_frame(e->fmt,e->in);
    if(r==AVERROR_EOF) {
      e->demux_eof=1;
      if((r=avcodec_send_packet(e->dec,NULL))<0) return fail(e,"DTS decoder drain",r);
      if(e->bsf && (r=av_bsf_send_packet(e->bsf,NULL))<0) return fail(e,"Video filter drain",r);
      continue;
    }
    if(r<0) return fail(e,"Container read",r);
    if(av_packet_get_side_data(e->in,AV_PKT_DATA_ENCRYPTION_INFO,NULL)) { av_packet_unref(e->in); return fail(e,"Encrypted packet unsupported",0); }
    if(e->fmt->streams[e->in->stream_index]->codecpar->codec_type==AVMEDIA_TYPE_SUBTITLE) {
      AVRational tb=e->fmt->streams[e->in->stream_index]->time_base;
      av_packet_move_ref(e->out,e->in);
      return frame_out(e,f,DTS_SUBTITLE,tb);
    }
    if(e->in->stream_index==e->info.audio_stream) {
      if(e->in->size>=4) {
        const uint8_t *b=e->in->data;
        uint32_t sync=((uint32_t)b[0]<<24)|((uint32_t)b[1]<<16)|((uint32_t)b[2]<<8)|b[3];
        if(sync==UINT32_C(0x7ffe8001) || sync==UINT32_C(0xfe7f0180)
            || sync==UINT32_C(0x1fffe800) || sync==UINT32_C(0xff1f00e8)) e->has_core=1;
      }
      r=avcodec_send_packet(e->dec,e->in);
      if(r<0) { av_packet_unref(e->in); return fail(e,"DTS decode input",r); }
    }
    else if(e->in->stream_index==e->info.video_stream) {
      if(!e->bsf) {
        AVRational tb=e->fmt->streams[e->info.video_stream]->time_base;
        av_packet_move_ref(e->out,e->in);
        return frame_out(e,f,DTS_VIDEO,tb);
      }
      r=av_bsf_send_packet(e->bsf,e->in);
    }
    else r=0;
    av_packet_unref(e->in);
    if(r<0) return fail(e,"Compressed video input",r);
  }
}
int dts_engine_next(DtsEngine *e,DtsFrame *f) {
  int result;
  for (;;) {
    result=next_frame(e,f);
    if(result==1 && (f->kind==DTS_AUDIO || f->kind==DTS_VIDEO))
      __atomic_store_n(&e->range_live,1,__ATOMIC_RELAXED);
    if(result!=1 || !e->dv7 || f->kind!=DTS_VIDEO) break;
    size_t size=0;
    if(f->size>INT_MAX-8192) return fail(e,"DV7 packet too large",0);
    av_fast_malloc(&e->dv7_buffer,&e->dv7_capacity,(size_t)f->size+8192);
    if(!e->dv7_buffer) return fail(e,"DV7 conversion buffer",AVERROR(ENOMEM));
    if(nv_dv7_packet(f->data,(size_t)f->size,e->dv7_buffer,e->dv7_capacity,&size)<0)
      return fail(e,"DV7 RPU conversion failed",0);
    /* An EL-only packet is discarded, not mistaken for end of stream. */
    if(!size) continue;
    f->data=e->dv7_buffer; f->size=(int)size;
    break;
  }
#ifdef NV_DTS_DEBUG
  if(e) {
    uint64_t now=clock_ns(CLOCK_MONOTONIC);
    if(!e->logged_at_ns) e->logged_at_ns=now;
    if(now>=e->logged_at_ns+UINT64_C(5000000000)) {
      DtsEngineMetrics *m=&e->metrics, *last=&e->logged_metrics;
      int ready=0, active=0, queued=0;
      uint64_t buffered=0;
      pthread_mutex_lock(&e->range_lock);
      for(int i=0;i<RANGE_SLOTS_MAX;i++) {
        DtsRangeJob *job=&e->ranges[i];
        if(job->state==1) queued++;
        if(job->state==2) active++;
        if(job->state==3 && job->data && job->count>0) { ready++; buffered+=(uint64_t)job->count; }
      }
      pthread_mutex_unlock(&e->range_lock);
      printf("[dts-transport] lanes=%d ready=%d active=%d queued=%d bufferedKiB=%llu ranges=%llu bytesKiB=%llu httpMs=%llu blockedMs=%llu maxReadMs=%llu shortReads=%llu\n",
             e->range_threads, ready, active, queued, (unsigned long long)(buffered/1024),
             (unsigned long long)(m->range_requests-last->range_requests),
             (unsigned long long)((m->range_bytes-last->range_bytes)/1024),
             (unsigned long long)((m->range_wait_ns-last->range_wait_ns)/1000000),
             (unsigned long long)((m->range_blocked_ns-last->range_blocked_ns)/1000000),
             (unsigned long long)(m->range_max_wait_ns/1000000),
             (unsigned long long)(m->range_short_reads-last->range_short_reads));
      e->logged_metrics=*m; e->logged_at_ns=now;
    }
  }
#endif
  return result;
}
int dts_engine_seek(DtsEngine *e,double seconds) {
  if(!e || !e->fmt || !isfinite(seconds) || seconds<0 || seconds>INT64_MAX/1000000000.0) return -1;
  if(cancelled(e)) return fail(e,"Cancelled",0);
  __atomic_store_n(&e->range_live,0,__ATOMIC_RELAXED);
  int64_t timestamp=(int64_t)(seconds*1000000.0);
  int r=avformat_seek_file(e->fmt,-1,INT64_MIN,timestamp,timestamp,AVSEEK_FLAG_BACKWARD);
  if(r<0) return fail(e,"Container seek",r);
  if(e->passthrough) {
    if(e->bsf) av_bsf_flush(e->bsf);
    av_packet_unref(e->in); av_packet_unref(e->out);
    e->demux_eof=0; e->target_ns=(int64_t)(seconds*1000000000.0);
    return 0;
  }
  avcodec_flush_buffers(e->dec); if(e->bsf) av_bsf_flush(e->bsf);
  if(reset_encoder(e)<0) return -1;
  swr_free(&e->swr); av_channel_layout_uninit(&e->input_layout);
  av_audio_fifo_reset(e->fifo); av_packet_unref(e->in); av_packet_unref(e->out);
  e->demux_eof=e->decoder_eof=e->resampler_eof=e->encoder_sent_eof=e->timeline=0;
  e->target_ns=(int64_t)(seconds*1000000000.0);
  return 0;
}
#endif
