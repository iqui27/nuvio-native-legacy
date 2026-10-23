/* DTS / TrueHD -> Dolby Digital remux for the Samsung .tpk. See dts_remux.h and
 * docs/features/dts/README.md.
 *
 * What the PC prototype proved on the host, kept here:
 *  - VIDEO is never decoded: packets go to the muxer byte for byte. Matroska
 *    stores PTS only and there is no software video decoder to learn the
 *    reorder depth, so DTS = min(pts[n..n+LOOK]) - LOOK*frame, from an 8-packet
 *    lookahead. It depends only on the packets themselves, so a segment made
 *    alone and the same span of a continuous run get the same DTS.
 *  - Segments are cut in DECODE order at keyframes of the container index
 *    (open GOP: leading pictures after a CRA belong to the CRA's segment).
 *  - AUDIO is placed sample-exactly: container ticks are coarse (Matroska 1 ms)
 *    and the first frames after a seek can carry a stale timestamp (TrueHD), so
 *    each frame only narrows a window for the offset; the window is snapped to
 *    the codec frame grid and LOCKED, and from there the position is offset +
 *    samples counted. Only a jump > 100 ms resyncs.
 *  - AC3 frames sit on ONE global grid (n * 1536 samples from t = 0), so the
 *    audio cut at a segment boundary is the same in every run.
 *  - One fixed timeline: +10 s output offset, the muxer's own negative-time
 *    shift disabled (it shifted each run by a different amount).
 *
 * New here: a generator CONTINUES from segment N to N+1 without seeking. The
 * packets read past a boundary stay queued, tagged with their segment, and the
 * codec state carries on, so consecutive HLS segments cost no extra download.
 * A fresh start (first segment, seek, parallel request) seeks straight to the
 * segment's keyframe; its first audio frames are silence until the decoder
 * has real samples (~0.1 s), which is fine at a seek. */
#include "dts_remux.h"
#include "dts_engine.h"
#include "dts_qcap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Real code only in the Samsung .tpk build with FFmpeg (and the host smoke
 * test). webOS also defines NV_DTS_FFMPEG but never remuxes: stubs there keep
 * its binary as it was. */
#include "../audsync.h"
#include <stdatomic.h>
static atomic_int audioOffsetMs;
void dts_remux_set_audio_offset_ms(int ms) { atomic_store(&audioOffsetMs, audsync_offset_ms(ms)); }

#if !defined(NV_DTS_FFMPEG) || !(defined(NV_TPK) || defined(NV_DTS_REMUX_TEST))
int dts_remux_available(void) { return 0; }
DtsRemux *dts_remux_open(const char *u, const char *h, int t, int s, double g, volatile int *c, char *err, size_t n) {
  (void)u; (void)h; (void)t; (void)s; (void)g; (void)c;
  if (err && n) snprintf(err, n, "converter not built");
  return NULL;
}
DtsRemux *dts_remux_new(volatile int *c) { (void)c; return NULL; }
int dts_remux_probe(DtsRemux *r, const char *u, const char *h, int t, int s, double g, char *err, size_t n) {
  (void)r; (void)u; (void)h; (void)t; (void)s; (void)g;
  if (err && n) snprintf(err, n, "converter not built");
  return -1;
}
const DtsRemuxInfo *dts_remux_info(const DtsRemux *r) { (void)r; return NULL; }
double dts_remux_segment_start(const DtsRemux *r, int s) { (void)r; (void)s; return 0; }
double dts_remux_segment_duration(const DtsRemux *r, int s) { (void)r; (void)s; return 0; }
int dts_remux_segment_at(const DtsRemux *r, double t) { (void)r; (void)t; return -1; }
double dts_remux_keyframe_before(const DtsRemux *r, double t) { (void)r; return t; }
void dts_remux_cancel(DtsRemux *r) { (void)r; }
void dts_remux_close(DtsRemux *r) { (void)r; }
DtsRemuxGen *dts_remux_gen_new(DtsRemux *r, char *err, size_t n) {
  (void)r; if (err && n) snprintf(err, n, "converter not built"); return NULL;
}
int dts_remux_gen_next(const DtsRemuxGen *g) { (void)g; return -1; }
int dts_remux_gen_run(DtsRemuxGen *g, int s, double t, const char *f, DtsRemuxWrite w, void *o,
                      volatile int *stop, DtsRemuxStats *st, char *err, size_t n) {
  (void)g; (void)s; (void)t; (void)f; (void)w; (void)o; (void)stop; (void)st;
  if (err && n) snprintf(err, n, "converter not built");
  return -1;
}
void dts_remux_gen_free(DtsRemuxGen *g) { (void)g; }
void dts_remux_gen_interrupt(DtsRemuxGen *g) { (void)g; }
double dts_remux_gen_start_s(const DtsRemuxGen *g) { (void)g; return 0; }
void dts_remux_set_queue_budget_mib(int mib) { (void)mib; }
#else

#include <math.h>
#include <time.h>
#include <stdarg.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/resource.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/dovi_meta.h>
#include <libswresample/swresample.h>

#define LOOK 8                   /* video DTS lookahead (>= reorder depth) */
#define PREROLL 2                /* encoder frames before the first one sent */
#define WIN_MIN_SRC 2048         /* source samples seen before the anchor locks */
#define MAX_GAP_S 5              /* longer audio holes skip ahead instead of padding */
#define TS_OFFSET_S 10           /* fixed output offset (design 2.2-4) */
#define VQ_MAX_BYTES (192u << 20)
#define AQ_MAX_BYTES (16u << 20)
#define AQ_MAX_PKTS 16384
#define VQ_MAX_PKTS (1 << 18)
#define QUEUE_BUDGET_MIB 192
/* Encoded audio waiting for its video: in a sane file well under a segment.
 * Audio interleaved this far ahead of its video (or video gone while the audio
 * goes on) fails the run instead of growing until the TV runs out of memory:
 * 60 s is 4.8 MB of AC3 640k. */
#define AQ_MAX_S 60
#define ENGINES_MAX 8
#define UNSET INT64_MIN
#define GRID_LAZY (INT64_MIN + 1) /* b48: the grid starts at the first audio (seek by time) */

static double now_s(clockid_t c) {
  struct timespec t;
  if (clock_gettime(c, &t)) return 0;
  return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

/* FFmpeg's own errors reach the app log, a few per source, with anything that
 * looks like an address cut (the URL never reaches FFmpeg: custom I/O only,
 * but container metadata could still carry one). */
static atomic_int logBudget;
static void avLog(void *avcl, int level, const char *fmt, va_list vl) {
  char line[200], *h;
  (void)avcl;
  if (level > AV_LOG_ERROR || atomic_fetch_sub(&logBudget, 1) <= 0) return;
  vsnprintf(line, sizeof line, fmt, vl);
  if ((h = strstr(line, "://")) != NULL) snprintf(h, sizeof line - (size_t)(h - line), "<cut>\n");
  h = line + strlen(line);
  while (h > line && (h[-1] == '\n' || h[-1] == '\r')) *--h = 0;
  printf("[dts-tpk] ffmpeg: %s\n", line);
  fflush(stdout);
}
static pthread_once_t logOnce = PTHREAD_ONCE_INIT;
static void logInit(void) { av_log_set_callback(avLog); }

typedef struct { AVPacket *p; int seg; } QPkt;
typedef struct { QPkt *v; int n, cap; size_t bytes; } Q;

static atomic_size_t gQueued;
static atomic_size_t gBudget = (size_t)QUEUE_BUDGET_MIB << 20;
void dts_remux_set_queue_budget_mib(int mib) {
  if (mib >= 32 && mib <= 512) atomic_store(&gBudget, (size_t)mib << 20);
}
static const char *qFull(const Q *q, size_t add, int video) {
  switch (dtsQcapAdmit(q->bytes, q->n, add, video ? VQ_MAX_BYTES : AQ_MAX_BYTES,
                       video ? VQ_MAX_PKTS : AQ_MAX_PKTS, &gQueued, atomic_load(&gBudget))) {
    case DTS_QCAP_BYTES: return video ? "Video queue full (audio missing?)" : "Audio queue full (video missing?)";
    case DTS_QCAP_PKTS: return "Queue packet limit reached";
    case DTS_QCAP_BUDGET: return "Queue memory budget reached";
    default: return NULL;
  }
}
static int qPush(Q *q, AVPacket *p, int seg, int video) {
  if (qFull(q, (size_t)p->size, video)) return -1;
  /* Reserve atomically: parallel generators must not both admit the last bytes. */
  size_t total = atomic_load(&gQueued), add = (size_t)p->size;
  do {
    size_t budget = atomic_load(&gBudget);
    if (total > budget || add > budget - total) return -1;
  } while (!atomic_compare_exchange_weak(&gQueued, &total, total + add));
  if (q->n == q->cap) {
    int cap = q->cap ? q->cap * 2 : 64;
    QPkt *v = realloc(q->v, (size_t)cap * sizeof *v);
    if (!v) { atomic_fetch_sub(&gQueued, add); return -1; }
    q->v = v; q->cap = cap;
  }
  q->v[q->n].p = p; q->v[q->n].seg = seg; q->n++;
  q->bytes += (size_t)p->size;
  return 0;
}
static AVPacket *qPop(Q *q) {
  AVPacket *p = q->v[0].p;
  q->bytes -= (size_t)p->size;
  atomic_fetch_sub(&gQueued, (size_t)p->size);
  memmove(q->v, q->v + 1, (size_t)(q->n - 1) * sizeof *q->v);
  q->n--;
  return p;
}
static void qClear(Q *q) {
  while (q->n) { AVPacket *p = qPop(q); av_packet_free(&p); }
  q->bytes = 0;
}

struct DtsRemux {
  DtsRemuxInfo info;
  char *url, *headers;
  DtsEngine *probe;                 /* reader of dts_remux_open, handed to the 1st generator */
  int vs, as, mov, stereo_aac;
  int audio_offset_ms;             /* stable across this source's segments and seeks */
  AVCodecParameters *vpar, *apar;
  AVRational vtb, atb, vfps;
  char lang[16];
  int64_t *kf; int nkf;             /* keyframe index (video time base), sorted */
  /* HEVC from Matroska/MP4 (hvcC): its NAL arrays in Annex B and the NAL
   * length size, for dts_remux_hevc_annexb. */
  uint8_t *hvccB; int hvccBN, hvccLen;
  atomic_int gopLogged;             /* GOP line already printed for this source */
  int64_t *segTs, *segB48; double *segS; int nseg;
  double endS;
  pthread_mutex_t lock;
  DtsEngine *engines[ENGINES_MAX]; int nEngines;
  volatile int *extCancel;
  int cancelled;
};

struct DtsRemuxGen {
  DtsRemux *src;
  DtsEngine *eng;
  atomic_int interrupted;           /* dts_remux_gen_interrupt during this run */
  AVFormatContext *ic;
  AVStream *V, *A;
  AVCodecContext *dec, *enc;
  SwrContext *swr;
  AVChannelLayout swrIn, target; int swrRate, swrFmt;
  AVAudioFifo *fifo;
  AVFrame *fr, *ef;
  AVPacket *pk, *op;
  int frame;                        /* encoder frame size (1536 AC3, 1024 AAC) */
  int64_t grid;                     /* 48 kHz position of the next fifo sample */
  int gridLazy;                     /* grid not known yet: set by the first audio placed */
  int64_t lo, hi, cum, off, win;    /* audio anchor, source-rate samples */
  int warm, decoderLogged, layoutWarned;
  long badAudio;
  Q vq, aq;
  int vseg, vstarted;
  int64_t vdur, lastDts;
  int64_t pend[2 * LOOK + 2]; int npend; int64_t lastPop;   /* video DTS: emitted PTS not yet popped */
  int gopKey, gopRasl, gopRadl, gopPkts;   /* GOP diagnostics of this generator's first run (log only) */
  double lastDemuxV;                /* source seconds of the last video packet read */
  int demuxEof, encEof;
  int nextSeg;
  int prog; int64_t progKf, progB48; double progStartS;
  /* muxer kept across consecutive segments (continuity counters run on) */
  AVFormatContext *oc; AVStream *ov, *oa;
  int hevcFix;                      /* this muxer is MPEG-TS and the video is HEVC with hvcC */
  DtsRemuxWrite w; void *wop; int wfail; int64_t bytes;
  /* rolling log, every 30 s of media */
  double logMedia, logCpu, logAudio, logWall, logProc, logBlocked, mediaTotal;
  /* CPU of this generator's work. Requests run on different connection
   * threads, so thread CPU is summed per run, never compared across threads. */
  double audioCpu, cpuAcc, runCpu0;
  int inRun;
};

static void fail(char *err, size_t n, const char *what, int code) {
  char d[64] = "";
  if (!err || !n) return;
  if (code < 0) av_strerror(code, d, sizeof d);
  snprintf(err, n, "%s%s%s", what, d[0] ? ": " : "", d);
}

/* VIDEO CODECS THE CONVERTER TAKES: HEVC and H.264 only. VC-1 and MPEG-2 do
 * not play from the converted TS/HLS on the TV (tested on the S95C), so those
 * files stay on the original, as on master. */
static int videoOk(enum AVCodecID id) { return id == AV_CODEC_ID_HEVC || id == AV_CODEC_ID_H264; }

/* HEVC NAL types of one packet (hvcC length prefix or Annex B), for the GOP
 * line of the log. Bit k of the result = NAL type k seen (types 0..31 only). */
static uint32_t hevcTipos(const uint8_t *d, int n, int lenSize) {
  uint32_t m = 0;
  if (n >= 4 && !d[0] && !d[1] && (d[2] == 1 || (!d[2] && d[3] == 1))) {
    for (int i = 0; i + 3 < n; i++)
      if (!d[i] && !d[i + 1] && d[i + 2] == 1) { int t = (d[i + 3] >> 1) & 0x3F; if (t < 32) m |= 1u << t; i += 2; }
    return m;
  }
  for (int i = 0; lenSize > 0 && i + lenSize < n;) {
    uint32_t l = 0;
    for (int k = 0; k < lenSize; k++) l = (l << 8) | d[i + k];
    i += lenSize;
    if (l == 0 || l > (uint32_t)(n - i)) break;
    { int t = (d[i] >> 1) & 0x3F; if (t < 32) m |= 1u << t; }
    i += (int)l;
  }
  return m;
}

/* HEVC TO ANNEX B FOR THE TS, keeping the stream's own parameter sets.
 * FFmpeg's hevc_mp4toannexb (which the TS muxer would insert by itself) puts
 * the hvcC parameter sets in front of EVERY keyframe, right before the IRAP
 * slice, so AFTER any VPS/SPS/PPS the packet already carries. A film whose
 * parameter sets change along the way (encoded in parts; Interstellar, all
 * three releases, Round 2) then decodes the later parts with the PPS of the
 * first one: blocks of garbage and frozen half frames that come and go with
 * the keyframes. Here the hvcC sets go in only when the keyframe packet does
 * not bring all of VPS, SPS and PPS itself; otherwise the output is what the
 * filter writes (4-byte start codes, NALs untouched).
 * Exported for the host check. Returns the size written to *out (allocated
 * here), 0 when the packet is left as is (already Annex B), -1 on a malformed
 * packet (also left as is: the muxer's filter takes it, as before). */
static int dts_remux_hevc_annexb(const uint8_t *hvccB, int hvccBN, int lenSize,
                          const uint8_t *d, int n, uint8_t **out) {
  int i, have = 0, irap = -1, total = 0, k = 0;
  *out = NULL;
  if (lenSize < 1 || lenSize > 4 || n < lenSize) return 0;
  if (n >= 4 && !d[0] && !d[1] && (d[2] == 1 || (!d[2] && d[3] == 1))) return 0;
  for (i = 0; i < n;) {   /* pass 1: sizes, the first IRAP, the sets before it */
    uint32_t l = 0; int t;
    if (n - i < lenSize) return -1;
    for (int b = 0; b < lenSize; b++) l = (l << 8) | d[i + b];
    i += lenSize;
    if (l < 2 || l > (uint32_t)(n - i)) return -1;
    t = (d[i] >> 1) & 0x3F;
    if (irap < 0 && t >= 16 && t <= 23) irap = i;
    if (irap < 0 && t >= 32 && t <= 34) have |= 1 << (t - 32);
    total += 4 + (int)l;
    i += (int)l;
  }
  if (irap >= 0 && have != 7) total += hvccBN; else irap = -1;
  if (!(*out = av_malloc((size_t)total + AV_INPUT_BUFFER_PADDING_SIZE))) return -1;
  for (i = 0; i < n;) {   /* pass 2: write */
    uint32_t l = 0;
    for (int b = 0; b < lenSize; b++) l = (l << 8) | d[i + b];
    i += lenSize;
    if (i == irap && hvccBN > 0) { memcpy(*out + k, hvccB, (size_t)hvccBN); k += hvccBN; }
    (*out)[k++] = 0; (*out)[k++] = 0; (*out)[k++] = 0; (*out)[k++] = 1;
    memcpy(*out + k, d + i, l); k += (int)l;
    i += (int)l;
  }
  memset(*out + k, 0, AV_INPUT_BUFFER_PADDING_SIZE);
  return k;
}
/* hvcC -> its NAL arrays in Annex B (what hevc_mp4toannexb prepends).
 * 1 done, 0 not an hvcC, -1 malformed. */
static int hvccAnnexB(const uint8_t *x, int xn, uint8_t **out, int *outN, int *lenSize) {
  int arrays, cap = 0, k = 0;
  *out = NULL; *outN = 0;
  if (!x || xn < 23 || x[0] != 1) return 0;
  *lenSize = (x[21] & 3) + 1;
  arrays = x[22];
  for (int pass = 0; pass < 2; pass++) {
    int i = 23;
    k = 0;
    for (int a = 0; a < arrays; a++) {
      int cnt;
      if (xn - i < 3) return -1;
      cnt = (x[i + 1] << 8) | x[i + 2]; i += 3;
      for (int c = 0; c < cnt; c++) {
        int l;
        if (xn - i < 2) return -1;
        l = (x[i] << 8) | x[i + 1]; i += 2;
        if (l > xn - i) return -1;
        if (pass) { (*out)[k] = 0; (*out)[k + 1] = 0; (*out)[k + 2] = 0; (*out)[k + 3] = 1; memcpy(*out + k + 4, x + i, (size_t)l); }
        k += 4 + l; i += l;
      }
    }
    if (!pass) {
      cap = k;
      if (!cap) return 0;
      if (!(*out = malloc((size_t)cap))) return -1;
    }
  }
  *outN = k;
  return 1;
}

static int isConvertible(enum AVCodecID id) {
  return id == AV_CODEC_ID_DTS || id == AV_CODEC_ID_TRUEHD || id == AV_CODEC_ID_MLP;
}

int dts_remux_available(void) {
  return avcodec_find_decoder(AV_CODEC_ID_DTS) && avcodec_find_decoder(AV_CODEC_ID_TRUEHD) &&
         avcodec_find_encoder(AV_CODEC_ID_AC3) && av_guess_format("mpegts", NULL, NULL);
}

static void registerEngine(DtsRemux *r, DtsEngine *e) {
  pthread_mutex_lock(&r->lock);
  if (r->nEngines < ENGINES_MAX) r->engines[r->nEngines++] = e;
  if (r->cancelled) dts_engine_cancel(e);
  pthread_mutex_unlock(&r->lock);
}
static void unregisterEngine(DtsRemux *r, DtsEngine *e) {
  pthread_mutex_lock(&r->lock);
  for (int i = 0; i < r->nEngines; i++)
    if (r->engines[i] == e) { r->engines[i] = r->engines[--r->nEngines]; break; }
  pthread_mutex_unlock(&r->lock);
}

static int cmpI64(const void *a, const void *b) {
  int64_t x = *(const int64_t *)a, y = *(const int64_t *)b;
  return x < y ? -1 : x > y;
}

/* Largest k with t[k] <= v (t sorted), -1 when v < t[0]. */
static int floorIdx(const int64_t *t, int n, int64_t v) {
  int lo = 0, hi = n - 1, r = -1;
  while (lo <= hi) {
    int m = lo + (hi - lo) / 2;
    if (t[m] <= v) { r = m; lo = m + 1; } else hi = m - 1;
  }
  return r;
}

static void buildSegments(DtsRemux *r, double target) {
  double tb = av_q2d(r->vtb);
  r->nseg = 0;
  if (r->nkf < 1) return;
  r->segTs = malloc((size_t)r->nkf * sizeof *r->segTs);
  r->segB48 = malloc((size_t)r->nkf * sizeof *r->segB48);
  r->segS = malloc((size_t)r->nkf * sizeof *r->segS);
  if (!r->segTs || !r->segB48 || !r->segS) return;
  int64_t cur = r->kf[0];
  r->segTs[r->nseg++] = cur;
  for (int i = 1; i < r->nkf; i++) {
    double t = r->kf[i] * tb;
    if ((r->kf[i] - cur) * tb >= target && r->endS - t >= (target < 2.0 ? target : 2.0)) {
      cur = r->kf[i];
      r->segTs[r->nseg++] = cur;
    }
  }
  r->info.max_segment_s = 0;
  for (int k = 0; k < r->nseg; k++) {
    r->segS[k] = k ? r->segTs[k] * tb : 0.0;
    /* Segment 0 takes every audio frame before the second boundary. */
    r->segB48[k] = k ? llround(r->segTs[k] * tb * 48000.0) : UNSET;
  }
  for (int k = 0; k < r->nseg; k++) {
    double d = dts_remux_segment_duration(r, k);
    if (d > r->info.max_segment_s) r->info.max_segment_s = d;
  }
}

DtsRemux *dts_remux_new(volatile int *cancel) {
  DtsRemux *r = calloc(1, sizeof *r);
  if (!r) return NULL;
  pthread_mutex_init(&r->lock, NULL);
  r->vs = r->as = -1;
  r->extCancel = cancel;
  r->audio_offset_ms = atomic_load(&audioOffsetMs);
  return r;
}

static int isCancelled(DtsRemux *r) {
  int c;
  pthread_mutex_lock(&r->lock);
  c = r->cancelled || (r->extCancel && *r->extCancel);
  pthread_mutex_unlock(&r->lock);
  return c;
}

DtsRemux *dts_remux_open(const char *url, const char *headers, int ordinal, int stereoAac,
                         double segTarget, volatile int *cancel, char *err, size_t errn) {
  DtsRemux *r = dts_remux_new(cancel);
  if (!r) { fail(err, errn, "Out of memory", 0); return NULL; }
  if (dts_remux_probe(r, url, headers, ordinal, stereoAac, segTarget, err, errn) < 0) { dts_remux_close(r); return NULL; }
  return r;
}

int dts_remux_probe(DtsRemux *r, const char *url, const char *headers, int ordinal, int stereoAac,
                    double segTarget, char *err, size_t errn) {
  AVFormatContext *ic;
  int nAudio = 0, ordAs = -1, ordPedido = -1, primeiro = -1, primeiroOrd = -1;
  pthread_once(&logOnce, logInit);
  atomic_store(&logBudget, 40);
  if (err && errn) err[0] = 0;
  if (!r) { fail(err, errn, "No converter", 0); return -1; }
  if (!url || !*url) { fail(err, errn, "No source", 0); return -1; }
  r->stereo_aac = stereoAac;
  r->url = strdup(url); r->headers = strdup(headers ? headers : "");
  r->probe = dts_engine_create();
  if (!r->url || !r->headers || !r->probe) { fail(err, errn, "Out of memory", 0); goto bad; }
  registerEngine(r, r->probe);   /* cancelled already: cancels it at once */
  if (isCancelled(r)) { fail(err, errn, "Cancelled", 0); goto bad; }
  if (dts_engine_open_demux(r->probe, r->url, r->headers) < 0) {
    if (isCancelled(r)) fail(err, errn, "Cancelled", 0);
    else fail(err, errn, dts_engine_error(r->probe), 0);
    goto bad;
  }
  ic = dts_engine_avformat(r->probe);
  r->mov = ic->iformat && strstr(ic->iformat->name, "mov") != NULL;
  for (unsigned i = 0; i < ic->nb_streams; i++) {
    AVStream *s = ic->streams[i];
    AVCodecParameters *p = s->codecpar;
    if (av_packet_side_data_get(p->coded_side_data, p->nb_coded_side_data, AV_PKT_DATA_ENCRYPTION_INIT_INFO) ||
        p->codec_tag == MKTAG('e','n','c','a') || p->codec_tag == MKTAG('e','n','c','v')) {
      fail(err, errn, "Encrypted media", 0); goto bad;
    }
    if (p->codec_type == AVMEDIA_TYPE_VIDEO && r->vs < 0 && !(s->disposition & AV_DISPOSITION_ATTACHED_PIC)) r->vs = (int)i;
    if (p->codec_type == AVMEDIA_TYPE_AUDIO) {
      if (isConvertible(p->codec_id) && primeiro < 0) { primeiro = (int)i; primeiroOrd = nAudio; }
      if (nAudio == ordinal) { ordPedido = (int)i; if (isConvertible(p->codec_id)) ordAs = (int)i; }
      nAudio++;
    }
  }
  if (r->vs < 0) { fail(err, errn, "No video stream", 0); goto bad; }
  if (ordinal >= 0 && ordAs < 0) {
    /* The k-th audio of the app's MKV reader is not DTS/TrueHD here (FFmpeg
     * skipped a track it could not parse?): the first convertible one. */
    printf("[dts-tpk] audio %d pedido nao e DTS/TrueHD no FFmpeg (%s, %d faixas de audio); usando a %d\n", ordinal,
           ordPedido >= 0 ? avcodec_get_name(ic->streams[ordPedido]->codecpar->codec_id) : "ausente", nAudio, primeiroOrd);
    fflush(stdout);
  }
  r->as = ordAs >= 0 ? ordAs : primeiro;
  if (r->as < 0) { fail(err, errn, "No DTS/TrueHD track", 0); goto bad; }
  { int k = 0;
    for (unsigned i = 0; i < ic->nb_streams && (int)i < r->as; i++) if (ic->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) k++;
    r->info.audio_ordinal = k; }
  {
    AVStream *V = ic->streams[r->vs], *A = ic->streams[r->as];
    if (!videoOk(V->codecpar->codec_id)) {
      fail(err, errn, "Video codec not HEVC/H.264", 0); goto bad;
    }
    if (V->codecpar->codec_id == AV_CODEC_ID_HEVC &&
        hvccAnnexB(V->codecpar->extradata, V->codecpar->extradata_size, &r->hvccB, &r->hvccBN, &r->hvccLen) < 0) {
      free(r->hvccB); r->hvccB = NULL; r->hvccBN = 0;   /* unreadable hvcC: FFmpeg's filter does it, as before */
    }
    for (unsigned i = 0; i < ic->nb_streams; i++)
      if ((int)i != r->vs && (int)i != r->as) ic->streams[i]->discard = AVDISCARD_ALL;
    r->vpar = avcodec_parameters_alloc(); r->apar = avcodec_parameters_alloc();
    if (!r->vpar || !r->apar || avcodec_parameters_copy(r->vpar, V->codecpar) < 0 ||
        avcodec_parameters_copy(r->apar, A->codecpar) < 0) { fail(err, errn, "Out of memory", 0); goto bad; }
    r->vtb = V->time_base; r->atb = A->time_base;
    r->vfps = V->avg_frame_rate.num > 0 ? V->avg_frame_rate : V->r_frame_rate;
    { AVDictionaryEntry *l = av_dict_get(A->metadata, "language", NULL, 0);
      snprintf(r->lang, sizeof r->lang, "%s", l ? l->value : ""); }
    DtsRemuxInfo *in = &r->info;
    in->duration_s = ic->duration / (double)AV_TIME_BASE;
    r->endS = (ic->start_time == AV_NOPTS_VALUE ? 0 : ic->start_time / (double)AV_TIME_BASE) + in->duration_s;
    in->width = V->codecpar->width; in->height = V->codecpar->height;
    in->fps = r->vfps.num > 0 && r->vfps.den > 0 ? av_q2d(r->vfps) : 0;
    { const AVPacketSideData *dv = av_packet_side_data_get(V->codecpar->coded_side_data, V->codecpar->nb_coded_side_data,
                                                           AV_PKT_DATA_DOVI_CONF);
      const AVDOVIDecoderConfigurationRecord *c = dv && dv->size >= (int)sizeof *c ? (const void *)dv->data : NULL;
      in->dv_profile = c ? c->dv_profile : -1;
      in->dv_el = c ? c->el_present_flag : 0; in->dv_compat = c ? c->dv_bl_signal_compatibility_id : 0; }
    snprintf(in->video_codec, sizeof in->video_codec, "%s", avcodec_get_name(V->codecpar->codec_id));
    snprintf(in->audio_codec, sizeof in->audio_codec, "%s", avcodec_get_name(A->codecpar->codec_id));
    /* Container flag only (no probe): many MKVs leave it to the bitstream. */
    snprintf(in->hdr, sizeof in->hdr, "%s", V->codecpar->color_trc == AVCOL_TRC_SMPTE2084 ? "PQ" :
             V->codecpar->color_trc == AVCOL_TRC_ARIB_STD_B67 ? "HLG" :
             V->codecpar->color_trc != AVCOL_TRC_UNSPECIFIED ? "SDR" :
             av_packet_side_data_get(V->codecpar->coded_side_data, V->codecpar->nb_coded_side_data,
                                     AV_PKT_DATA_MASTERING_DISPLAY_METADATA) ? "PQ (HDR10 metadata)" :
             "not in the container (bitstream decides)");
    in->stream_index = r->as;
    in->in_channels = A->codecpar->ch_layout.nb_channels; in->in_rate = A->codecpar->sample_rate;
    int ic2 = in->in_channels;
    in->out_channels = stereoAac ? 2 : ic2 == 1 ? 1 : ic2 == 2 ? 2 : 6;
    snprintf(in->out_codec, sizeof in->out_codec, "%s", stereoAac ? "aac" : "ac3");
    in->out_bitrate = in->out_channels > 2 ? 640000 : in->out_channels == 2 ? 192000 : 96000;
    if (A->codecpar->codec_id == AV_CODEC_ID_DTS) snprintf(in->decoder_path, sizeof in->decoder_path, "DTS core only");
    else if (ic2 > in->out_channels || ic2 == 0)
      snprintf(in->decoder_path, sizeof in->decoder_path, "TrueHD %s presentation", in->out_channels == 6 ? "5.1" : "2.0");
    else snprintf(in->decoder_path, sizeof in->decoder_path, "TrueHD full decode");
    in->mp4 = r->mov;
    /* Matroska parses Cues only on the first seek (design 2.2-6). */
    int64_t t0 = V->start_time != AV_NOPTS_VALUE ? V->start_time : 0;
    avformat_seek_file(ic, r->vs, INT64_MIN, t0, t0, AVSEEK_FLAG_BACKWARD);
    if (isCancelled(r)) { fail(err, errn, "Cancelled", 0); goto bad; }
    int n = avformat_index_get_entries_count(V);
    r->kf = n > 0 ? malloc((size_t)n * sizeof *r->kf) : NULL;
    for (int i = 0; i < n && r->kf; i++) {
      const AVIndexEntry *e = avformat_index_get_entry(V, i);
      if (e && (e->flags & AVINDEX_KEYFRAME)) r->kf[r->nkf++] = e->timestamp;
    }
    if (r->nkf > 1) {
      int u = 1;
      qsort(r->kf, (size_t)r->nkf, sizeof *r->kf, cmpI64);
      for (int i = 1; i < r->nkf; i++) if (r->kf[i] != r->kf[u - 1]) r->kf[u++] = r->kf[i];
      r->nkf = u;
    }
    in->n_keyframes = r->nkf;
    /* A GOP is seconds long: an index whose last keyframe is a minute before
     * the end does not cover the file (partial Cues, fragmented MP4 indexed
     * only as far as the probe read). The playlist would end there. */
    in->index_complete = r->nkf > 0 && r->kf[r->nkf - 1] * av_q2d(r->vtb) >= r->endS - 60.0;
    buildSegments(r, segTarget > 0.5 ? segTarget : 6.0);
    in->n_segments = r->nseg;
    in->size_bytes = dts_engine_size(r->probe);
  }
  return 0;
bad:
  /* The reader goes now (its connection with it); the caller closes r. */
  if (r->probe) {
    DtsEngine *e = r->probe;
    r->probe = NULL;
    dts_engine_cancel(e); unregisterEngine(r, e); dts_engine_destroy(e);
  }
  return -1;
}

const DtsRemuxInfo *dts_remux_info(const DtsRemux *r) { return r ? &r->info : NULL; }
double dts_remux_segment_start(const DtsRemux *r, int k) {
  return r && k >= 0 && k < r->nseg ? r->segS[k] : 0.0;
}
double dts_remux_segment_duration(const DtsRemux *r, int k) {
  if (!r || k < 0 || k >= r->nseg) return 0.0;
  double end = k + 1 < r->nseg ? r->segS[k + 1] : r->endS;
  return end > r->segS[k] ? end - r->segS[k] : 0.001;
}
int dts_remux_segment_at(const DtsRemux *r, double t) {
  if (!r || r->nseg < 1) return -1;
  int k = 0;
  while (k + 1 < r->nseg && r->segS[k + 1] <= t) k++;
  return k;
}

double dts_remux_keyframe_before(const DtsRemux *r, double t) {
  if (!r || r->nkf < 1) return t;
  int64_t want = av_rescale_q((int64_t)llround(t * 1000.0), (AVRational){1, 1000}, r->vtb);
  if (!r->info.index_complete && want > r->kf[r->nkf - 1]) return t;   /* seek by time (gen_run) */
  int k = floorIdx(r->kf, r->nkf, want);
  double s = (k >= 0 ? r->kf[k] : r->kf[0]) * av_q2d(r->vtb);
  return s < 0 ? 0 : s;
}

void dts_remux_cancel(DtsRemux *r) {
  if (!r) return;
  pthread_mutex_lock(&r->lock);
  r->cancelled = 1;
  for (int i = 0; i < r->nEngines; i++) dts_engine_cancel(r->engines[i]);
  pthread_mutex_unlock(&r->lock);
}

void dts_remux_close(DtsRemux *r) {
  if (!r) return;
  if (r->probe) { dts_engine_cancel(r->probe); unregisterEngine(r, r->probe); dts_engine_destroy(r->probe); }
  avcodec_parameters_free(&r->vpar); avcodec_parameters_free(&r->apar);
  free(r->kf); free(r->hvccB); free(r->segTs); free(r->segB48); free(r->segS);
  free(r->url); free(r->headers);
  pthread_mutex_destroy(&r->lock);
  free(r);
}

/* ---------------------------------------------------------------- generator */

static int openAudio(DtsRemuxGen *g, char *err, size_t errn) {
  DtsRemux *r = g->src;
  const AVCodec *dc = avcodec_find_decoder(r->apar->codec_id);
  const AVCodec *ec = avcodec_find_encoder(r->stereo_aac ? AV_CODEC_ID_AAC : AV_CODEC_ID_AC3);
  int rr;
  avcodec_free_context(&g->dec); avcodec_free_context(&g->enc);
  if (!dc || !ec) { fail(err, errn, "Decoder or encoder not in this build", 0); return -1; }
  g->dec = avcodec_alloc_context3(dc);
  if (!g->dec) { fail(err, errn, "Out of memory", 0); return -1; }
  avcodec_parameters_to_context(g->dec, r->apar);
  g->dec->pkt_timebase = r->atb;
  av_channel_layout_uninit(&g->target);
  av_channel_layout_default(&g->target, r->info.out_channels);
  /* DTS: the core is the backward-compatible 5.1/2.0 mix and already holds more
   * than AC3 640k carries; XLL would cost CPU for nothing (design 2.3).
   * TrueHD/MLP: ask for the presentation the studio authored at that width. */
  if (r->apar->codec_id == AV_CODEC_ID_DTS) av_opt_set_int(g->dec->priv_data, "core_only", 1, 0);
  else if (r->info.in_channels == 0 || r->info.in_channels > r->info.out_channels)
    av_opt_set_chlayout(g->dec->priv_data, "downmix", &g->target, 0);
  if ((rr = avcodec_open2(g->dec, dc, NULL)) < 0) { fail(err, errn, "Audio decoder open", rr); return -1; }
  g->enc = avcodec_alloc_context3(ec);
  if (!g->enc) { fail(err, errn, "Out of memory", 0); return -1; }
  av_channel_layout_copy(&g->enc->ch_layout, &g->target);
  g->enc->sample_rate = 48000; g->enc->time_base = (AVRational){1, 48000};
  g->enc->sample_fmt = AV_SAMPLE_FMT_FLTP;
  g->enc->bit_rate = r->info.out_bitrate;
  if (r->stereo_aac) { g->enc->profile = AV_PROFILE_AAC_LOW; g->enc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; }
  if ((rr = avcodec_open2(g->enc, ec, NULL)) < 0) { fail(err, errn, "Audio encoder open", rr); return -1; }
  g->frame = g->enc->frame_size > 0 ? g->enc->frame_size : 1536;
  av_audio_fifo_free(g->fifo);
  g->fifo = av_audio_fifo_alloc(AV_SAMPLE_FMT_FLTP, r->info.out_channels, 8192);
  if (!g->fifo) { fail(err, errn, "Out of memory", 0); return -1; }
  swr_free(&g->swr); av_channel_layout_uninit(&g->swrIn);
  return 0;
}

DtsRemuxGen *dts_remux_gen_new(DtsRemux *r, char *err, size_t errn) {
  DtsRemuxGen *g;
  if (!r) return NULL;
  g = calloc(1, sizeof *g);
  if (!g) { fail(err, errn, "Out of memory", 0); return NULL; }
  g->src = r; g->nextSeg = -1;
  pthread_mutex_lock(&r->lock);
  g->eng = r->probe; r->probe = NULL;
  pthread_mutex_unlock(&r->lock);
  if (!g->eng) {
    double t0 = now_s(CLOCK_MONOTONIC);
    g->eng = dts_engine_create();
    if (!g->eng) { fail(err, errn, "Out of memory", 0); free(g); return NULL; }
    registerEngine(r, g->eng);
    if (dts_engine_open_demux(g->eng, r->url, r->headers) < 0) {
      fail(err, errn, dts_engine_error(g->eng), 0); dts_remux_gen_free(g); return NULL;
    }
    AVFormatContext *ic = dts_engine_avformat(g->eng);
    if ((unsigned)r->vs >= ic->nb_streams || (unsigned)r->as >= ic->nb_streams) {
      fail(err, errn, "Source changed between opens", 0); dts_remux_gen_free(g); return NULL;
    }
    for (unsigned i = 0; i < ic->nb_streams; i++)
      if ((int)i != r->vs && (int)i != r->as) ic->streams[i]->discard = AVDISCARD_ALL;
    printf("[dts-tpk] generator: own reader opened in %.0f ms (the first one reuses the probe's)\n",
           (now_s(CLOCK_MONOTONIC) - t0) * 1000.0);
    fflush(stdout);
  }
  g->ic = dts_engine_avformat(g->eng);
  g->V = g->ic->streams[r->vs]; g->A = g->ic->streams[r->as];
  g->pk = av_packet_alloc(); g->op = av_packet_alloc(); g->fr = av_frame_alloc(); g->ef = av_frame_alloc();
  if (!g->pk || !g->op || !g->fr || !g->ef) { fail(err, errn, "Out of memory", 0); dts_remux_gen_free(g); return NULL; }
  if (openAudio(g, err, errn) < 0) { dts_remux_gen_free(g); return NULL; }
  g->vdur = r->vfps.num > 0 ? av_rescale_q(1, av_inv_q(r->vfps), r->vtb) : 0;
  if (g->vdur <= 0) g->vdur = av_rescale_q(40, (AVRational){1, 1000}, r->vtb);
  if (g->vdur <= 0) g->vdur = 1;
  return g;
}

static void closeMux(DtsRemuxGen *g, int trailer) {
  if (!g->oc) return;
  if (trailer && !g->wfail) av_write_trailer(g->oc);
  if (g->oc->pb) { avio_flush(g->oc->pb); av_freep(&g->oc->pb->buffer); avio_context_free(&g->oc->pb); }
  avformat_free_context(g->oc);
  g->oc = NULL; g->ov = g->oa = NULL;
}

void dts_remux_gen_free(DtsRemuxGen *g) {
  if (!g) return;
  closeMux(g, 0);
  qClear(&g->vq); qClear(&g->aq); free(g->vq.v); free(g->aq.v);
  av_packet_free(&g->pk); av_packet_free(&g->op); av_frame_free(&g->fr); av_frame_free(&g->ef);
  avcodec_free_context(&g->dec); avcodec_free_context(&g->enc);
  swr_free(&g->swr); av_audio_fifo_free(g->fifo);
  av_channel_layout_uninit(&g->swrIn); av_channel_layout_uninit(&g->target);
  if (g->eng) {
    dts_engine_cancel(g->eng);
    unregisterEngine(g->src, g->eng);
    dts_engine_destroy(g->eng);
  }
  free(g);
}

int dts_remux_gen_next(const DtsRemuxGen *g) { return g ? g->nextSeg : -1; }
void dts_remux_gen_interrupt(DtsRemuxGen *g) {
  if (!g) return;
  atomic_store(&g->interrupted, 1);
  dts_engine_interrupt(g->eng, 1);
}
double dts_remux_gen_start_s(const DtsRemuxGen *g) { return g ? g->progStartS : 0.0; }

static int sinkWrite(void *o, const uint8_t *b, int n) {
  DtsRemuxGen *g = o;
  if (g->wfail) return AVERROR(EPIPE);
  if (!g->w) return n;
  if (g->w(g->wop, b, n) < 0) { g->wfail = 1; return AVERROR(EPIPE); }
  g->bytes += n;
  return n;
}

static int openMux(DtsRemuxGen *g, const char *fmt, char *err, size_t errn) {
  DtsRemux *r = g->src;
  AVDictionary *o = NULL;
  unsigned char *buf;
  int rr;
  closeMux(g, 0);
  if ((rr = avformat_alloc_output_context2(&g->oc, NULL, fmt, NULL)) < 0 || !g->oc) {
    fail(err, errn, "Muxer not in this build", rr); return -1;
  }
  g->ov = avformat_new_stream(g->oc, NULL); g->oa = avformat_new_stream(g->oc, NULL);
  if (!g->ov || !g->oa) { fail(err, errn, "Out of memory", 0); return -1; }
  avcodec_parameters_copy(g->ov->codecpar, r->vpar);
  g->ov->codecpar->codec_tag = 0;
  /* Only HDR10 static metadata goes along. No Dolby Vision configuration: the
   * S95C has no Dolby Vision, and the base layer plays as HDR10 like today. */
  av_packet_side_data_free(&g->ov->codecpar->coded_side_data, &g->ov->codecpar->nb_coded_side_data);
  for (int i = 0; i < r->vpar->nb_coded_side_data; i++) {
    const AVPacketSideData *sd = &r->vpar->coded_side_data[i];
    if (sd->type != AV_PKT_DATA_MASTERING_DISPLAY_METADATA && sd->type != AV_PKT_DATA_CONTENT_LIGHT_LEVEL) continue;
    uint8_t *c = av_memdup(sd->data, sd->size);
    if (c && !av_packet_side_data_add(&g->ov->codecpar->coded_side_data, &g->ov->codecpar->nb_coded_side_data,
                                      sd->type, c, sd->size, 0)) av_free(c);
  }
  g->ov->time_base = r->vtb;
  avcodec_parameters_from_context(g->oa->codecpar, g->enc);
  g->oa->time_base = (AVRational){1, 48000};
  if (r->lang[0]) av_dict_set(&g->oa->metadata, "language", r->lang, 0);
  buf = av_malloc(65536);
  g->oc->pb = buf ? avio_alloc_context(buf, 65536, 1, g, NULL, sinkWrite, NULL) : NULL;
  if (!g->oc->pb) { av_free(buf); fail(err, errn, "Out of memory", 0); return -1; }
  g->oc->avoid_negative_ts = AVFMT_AVOID_NEG_TS_DISABLED;
  g->oc->output_ts_offset = (int64_t)TS_OFFSET_S * AV_TIME_BASE;
  g->hevcFix = !strcmp(fmt, "mpegts") && r->hvccBN > 0;
  if (!strcmp(fmt, "mp4")) av_dict_set(&o, "movflags", "frag_keyframe+empty_moov+default_base_moof+delay_moov", 0);
  rr = avformat_write_header(g->oc, &o);
  av_dict_free(&o);
  if (rr < 0) { fail(err, errn, g->wfail ? "Client closed" : "Muxer header", rr); return -1; }
  return 0;
}

/* Segment tags. HLS: the segment whose boundary is the last one at or before
 * the timestamp. Progressive: 0 from the start keyframe on, -1 before. */
static int tagVideo(DtsRemuxGen *g, int64_t kfts) {
  if (g->prog) return kfts >= g->progKf ? 0 : -1;
  return floorIdx(g->src->segTs, g->src->nseg, kfts);
}
static int tagAudio(DtsRemuxGen *g, int64_t pts48) {
  DtsRemux *r = g->src;
  if (g->prog) return pts48 >= g->progB48 ? 0 : -1;
  int k = floorIdx(r->segB48 + 1, r->nseg - 1, pts48);   /* segB48[0] = -inf */
  return k + 1;
}

static int resetAudio(DtsRemuxGen *g, char *err, size_t errn) {
  /* Fresh decoder and encoder: AC3/AAC encoders cannot flush. */
  if (openAudio(g, err, errn) < 0) return -1;
  g->lo = INT64_MIN; g->hi = INT64_MAX; g->cum = 0; g->off = UNSET; g->win = 0;
  g->warm = 0; g->encEof = 0;
  return 0;
}

static int encodeReady(DtsRemuxGen *g, int final, char *err, size_t errn);

static int drainEncoder(DtsRemuxGen *g, char *err, size_t errn) {
  for (;;) {
    int rr = avcodec_receive_packet(g->enc, g->op);
    if (rr == AVERROR(EAGAIN)) return 0;
    if (rr == AVERROR_EOF) { g->encEof = 1; return 0; }
    if (rr < 0) { fail(err, errn, "Audio encode", rr); return -1; }
    if ((int64_t)(g->aq.n + 1) * g->frame > (int64_t)AQ_MAX_S * 48000) {
      av_packet_unref(g->op); fail(err, errn, "Audio queue over 60 s (video missing?)", 0); return -1;
    }
    { const char *why = qFull(&g->aq, (size_t)g->op->size, 0);
      if (why) { fail(err, errn, why, 0); return -1; } }
    AVPacket *p = av_packet_clone(g->op);
    av_packet_unref(g->op);
    if (!p || qPush(&g->aq, p, tagAudio(g, p->pts), 0) < 0) { av_packet_free(&p); fail(err, errn, "Out of memory", 0); return -1; }
  }
}

/* Encodes every full frame in the fifo (and, when final, the partial rest
 * padded with silence) on the global grid. */
static int encodeReady(DtsRemuxGen *g, int final, char *err, size_t errn) {
  int ch = g->src->info.out_channels;
  while (av_audio_fifo_size(g->fifo) >= g->frame || (final && av_audio_fifo_size(g->fifo) > 0)) {
    int have = av_audio_fifo_size(g->fifo), take = have < g->frame ? have : g->frame, rr;
    av_frame_unref(g->ef);
    g->ef->format = AV_SAMPLE_FMT_FLTP; g->ef->sample_rate = 48000;
    av_channel_layout_copy(&g->ef->ch_layout, &g->target);
    g->ef->nb_samples = g->frame;
    if ((rr = av_frame_get_buffer(g->ef, 0)) < 0) { fail(err, errn, "Audio buffer", rr); return -1; }
    av_samples_set_silence(g->ef->extended_data, 0, g->frame, ch, AV_SAMPLE_FMT_FLTP);
    if (av_audio_fifo_read(g->fifo, (void **)g->ef->extended_data, take) != take) { fail(err, errn, "Audio queue", 0); return -1; }
    g->ef->pts = g->grid; g->grid += g->frame;
    if ((rr = avcodec_send_frame(g->enc, g->ef)) < 0) { fail(err, errn, "Audio encode", rr); return -1; }
    av_frame_unref(g->ef);
    if (drainEncoder(g, err, errn) < 0) return -1;
  }
  return 0;
}

static int fifoSilence(DtsRemuxGen *g, int64_t n) {
  int ch = g->src->info.out_channels;
  while (n > 0) {
    int k = n > 4096 ? 4096 : (int)n;
    uint8_t **z = NULL;
    if (av_samples_alloc_array_and_samples(&z, NULL, ch, k, AV_SAMPLE_FMT_FLTP, 0) < 0) return -1;
    av_samples_set_silence(z, 0, k, ch, AV_SAMPLE_FMT_FLTP);
    int w = av_audio_fifo_write(g->fifo, (void **)z, k);
    av_freep(&z[0]); av_freep(&z);
    if (w != k) return -1;
    n -= k;
  }
  return 0;
}

/* Puts n converted samples that start at 48 kHz position p into the fifo:
 * overlap trimmed, holes padded (a long hole skips ahead on the grid). */
static int place(DtsRemuxGen *g, uint8_t **buf, int n, int64_t p, char *err, size_t errn) {
  int ch = g->src->info.out_channels, skip = 0;
  if (g->gridLazy) {
    /* Seek by time (no index there): the keyframe the demuxer found is not
     * known in advance, so the grid starts on the first audio after it. */
    g->gridLazy = 0;
    g->grid = (p / g->frame) * g->frame;
    if (g->grid < 0) g->grid = 0;
  }
  int64_t end = g->grid + av_audio_fifo_size(g->fifo);
  if (p + n <= end) return 0;
  if (p < end) skip = (int)(end - p);
  else if (p > end) {
    int64_t gap = p - end;
    if (gap > (int64_t)MAX_GAP_S * 48000) {
      printf("[dts-tpk] audio: hole of %.1f s in the source, skipping ahead\n", gap / 48000.0);
      fflush(stdout);
      if (encodeReady(g, 1, err, errn) < 0) return -1;
      g->grid = (p / g->frame) * g->frame;
      gap = p - g->grid;
    }
    if (fifoSilence(g, gap) < 0) { fail(err, errn, "Out of memory", 0); return -1; }
  }
  if (n > skip) {
    uint8_t *planes[8];
    for (int c = 0; c < ch && c < 8; c++) planes[c] = buf[c] + (size_t)skip * sizeof(float);
    if (av_audio_fifo_write(g->fifo, (void **)planes, n - skip) != n - skip) { fail(err, errn, "Out of memory", 0); return -1; }
  }
  return 0;
}

static int frameIn(DtsRemuxGen *g, AVFrame *f, char *err, size_t errn) {
  DtsRemux *r = g->src;
  int rate = f->sample_rate > 0 ? f->sample_rate : 48000, rr;
  if (!g->swr || g->swrRate != rate || g->swrFmt != f->format || av_channel_layout_compare(&g->swrIn, &f->ch_layout)) {
    char d[64] = "?";
    av_channel_layout_describe(&f->ch_layout, d, sizeof d);
    if (g->swr && !g->layoutWarned++) {
      printf("[dts-tpk] audio: layout changed midstream to %s %d Hz (resampler reset)\n", d, rate);
      fflush(stdout);
    }
    swr_free(&g->swr);
    if ((rr = swr_alloc_set_opts2(&g->swr, &g->target, AV_SAMPLE_FMT_FLTP, 48000, &f->ch_layout, f->format, rate, 0, NULL)) < 0 ||
        (rr = swr_init(g->swr)) < 0) { fail(err, errn, "Audio resampler", rr); return -1; }
    av_channel_layout_uninit(&g->swrIn); av_channel_layout_copy(&g->swrIn, &f->ch_layout);
    g->swrRate = rate; g->swrFmt = f->format;
    if (!g->decoderLogged) {
      const char *prof = avcodec_profile_name(r->apar->codec_id, g->dec->profile);
      g->decoderLogged = 1;
      printf("[dts-tpk] decoder: %s %s -> %s %d ch %d Hz%s%s (path: %s)\n", r->info.audio_codec, d,
             r->info.out_codec, r->info.out_channels, rate, prof ? ", profile " : "", prof ? prof : "",
             r->info.decoder_path);
      fflush(stdout);
    }
  }
  /* Anchor in source-rate samples (see the top of this file). */
  int64_t half = av_rescale_q(1, g->A->time_base, (AVRational){1, rate}) / 2 + 1;
  int64_t praw = av_rescale_q(f->best_effort_timestamp, g->A->time_base, (AVRational){1, rate});
  int64_t psrc;
  if (g->off == UNSET) {
    int64_t l = praw - half - g->cum, h = praw + half - g->cum;
    if (l > g->hi || h < g->lo) { g->lo = l; g->hi = h; g->win = 0; }
    else { if (l > g->lo) g->lo = l; if (h < g->hi) g->hi = h; }
    g->win += f->nb_samples;
    if (g->win >= WIN_MIN_SRC) {
      int64_t fs = f->nb_samples > 0 ? f->nb_samples : 1, mid = g->lo + (g->hi - g->lo) / 2;
      int64_t snap = (int64_t)floor(((double)mid + fs / 2.0) / (double)fs) * fs;
      g->off = (snap >= g->lo && snap <= g->hi) ? snap : mid;
    }
    psrc = g->off != UNSET ? g->off + g->cum : praw;
  } else if (llabs(praw - (g->off + g->cum)) > rate / 10) {
    printf("[dts-tpk] audio: timestamp jump %+.3f s, resync\n", (praw - (g->off + g->cum)) / (double)rate);
    fflush(stdout);
    g->off = UNSET; g->cum = 0; g->lo = praw - half; g->hi = praw + half; g->win = f->nb_samples;
    psrc = praw;
  } else psrc = g->off + g->cum;
  g->cum += f->nb_samples;
  int64_t p48 = av_rescale_rnd(psrc, 48000, rate, AV_ROUND_NEAR_INF) - swr_get_delay(g->swr, 48000);
  int cap = (int)av_rescale_rnd(f->nb_samples, 48000, rate, AV_ROUND_UP) + 256;
  uint8_t **buf = NULL;
  if ((rr = av_samples_alloc_array_and_samples(&buf, NULL, r->info.out_channels, cap, AV_SAMPLE_FMT_FLTP, 0)) < 0) {
    fail(err, errn, "Audio buffer", rr); return -1;
  }
  int n = swr_convert(g->swr, buf, cap, (const uint8_t **)f->extended_data, f->nb_samples);
  rr = n < 0 ? (fail(err, errn, "Audio resample", n), -1) : place(g, buf, n, p48, err, errn);
  av_freep(&buf[0]); av_freep(&buf);
  if (rr < 0) return -1;
  return encodeReady(g, 0, err, errn);
}

static int receiveFrames(DtsRemuxGen *g, char *err, size_t errn) {
  int bad = 0;
  for (;;) {
    int rr = avcodec_receive_frame(g->dec, g->fr);
    if (rr == AVERROR(EAGAIN) || rr == AVERROR_EOF) return 0;
    if (rr < 0) { g->badAudio++; if (++bad > 64) return 0; continue; }   /* corrupt unit while syncing */
    /* The first frames after open/seek can carry the timestamp of a packet the
     * decoder skipped while hunting for a sync (TrueHD: 13 ms early). */
    if (g->warm < 2 || g->fr->best_effort_timestamp == AV_NOPTS_VALUE) { g->warm++; av_frame_unref(g->fr); continue; }
    rr = frameIn(g, g->fr, err, errn);
    av_frame_unref(g->fr);
    if (rr < 0) return -1;
  }
}

static int audioPacket(DtsRemuxGen *g, AVPacket *p, char *err, size_t errn) {
  double c0 = now_s(CLOCK_THREAD_CPUTIME_ID);
  int rr = avcodec_send_packet(g->dec, p);
  if (rr == AVERROR(EAGAIN)) {            /* output pending: take it, then retry */
    if (receiveFrames(g, err, errn) < 0) return -1;
    rr = avcodec_send_packet(g->dec, p);
  }
  if (rr < 0) g->badAudio++;
  rr = receiveFrames(g, err, errn);
  g->audioCpu += now_s(CLOCK_THREAD_CPUTIME_ID) - c0;
  return rr;
}

/* End of the source: decoder, resampler and encoder drained into the queue. */
static int audioEof(DtsRemuxGen *g, char *err, size_t errn) {
  double c0 = now_s(CLOCK_THREAD_CPUTIME_ID);
  int rr = 0;
  avcodec_send_packet(g->dec, NULL);
  if (receiveFrames(g, err, errn) < 0) rr = -1;
  if (!rr && g->swr) {
    uint8_t **buf = NULL;
    int ch = g->src->info.out_channels;
    int64_t pos = g->grid + av_audio_fifo_size(g->fifo);
    if (av_samples_alloc_array_and_samples(&buf, NULL, ch, 4096, AV_SAMPLE_FMT_FLTP, 0) >= 0) {
      int n = swr_convert(g->swr, buf, 4096, NULL, 0);
      if (n > 0) rr = place(g, buf, n, pos, err, errn);
      av_freep(&buf[0]); av_freep(&buf);
    }
  }
  if (!rr) rr = encodeReady(g, 1, err, errn);
  if (!rr) { avcodec_send_frame(g->enc, NULL); rr = drainEncoder(g, err, errn); }
  g->encEof = 1;
  g->audioCpu += now_s(CLOCK_THREAD_CPUTIME_ID) - c0;
  return rr;
}

static int64_t kfTs(DtsRemuxGen *g, const AVPacket *p) {
  /* MP4/MOV index entries are DTS; Matroska Cues are block timecodes (PTS). */
  if (g->src->mov && p->dts != AV_NOPTS_VALUE) return p->dts;
  return p->pts != AV_NOPTS_VALUE ? p->pts : p->dts;
}

static int videoPacket(DtsRemuxGen *g, AVPacket *p, char *err, size_t errn) {
  int key = (p->flags & AV_PKT_FLAG_KEY) != 0;
  if (p->pts == AV_NOPTS_VALUE) p->pts = p->dts;
  if (p->pts == AV_NOPTS_VALUE) return 0;
  if (!g->vstarted) {
    if (!key) return 0;          /* nothing before the first keyframe decodes */
    g->vstarted = 1;
  }
  if (key) {
    int t = tagVideo(g, kfTs(g, p));
    if (t > g->vseg) g->vseg = t;
  }
  if (p->duration <= 0) p->duration = g->vdur;
  g->lastDemuxV = p->pts * av_q2d(g->src->vtb);
  /* GOP of the first keyframe of the session: IDR (closed) or CRA (open, with
   * RASL pictures that need the previous GOP). One log line per source; it is
   * the first suspect when one file lags on HLS and plays smooth on the TS. */
  { DtsRemux *r = g->src;
    if (!atomic_load(&r->gopLogged) && g->gopPkts < 48 && r->vpar->codec_id == AV_CODEC_ID_HEVC) {
      int ls = r->vpar->extradata_size > 22 && r->vpar->extradata[0] == 1 ? (r->vpar->extradata[21] & 3) + 1 : 0;
      uint32_t m = hevcTipos(p->data, p->size, ls);
      if (key && !g->gopKey) g->gopKey = (m & (3u << 19)) ? 19 : (m & (1u << 21)) ? 21 : (m & (7u << 16)) ? 16 : -1;
      if (g->gopKey) {
        if (m & (3u << 8)) g->gopRasl++;
        if (m & (3u << 6)) g->gopRadl++;
        if (++g->gopPkts >= 48 && !atomic_exchange(&r->gopLogged, 1)) {
          printf("[dts-tpk] gop: quadro-chave %s, %d RASL e %d RADL nos 48 pacotes seguintes%s\n",
                 g->gopKey == 19 ? "IDR (GOP fechado)" : g->gopKey == 21 ? "CRA (GOP aberto)" : g->gopKey == 16 ? "BLA" : "?",
                 g->gopRasl, g->gopRadl, g->gopRasl ? ": quadros que dependem do GOP anterior" : "");
          fflush(stdout);
        }
      }
    } }
  { const char *why = qFull(&g->vq, (size_t)p->size, 1);
    if (why) { fail(err, errn, why, 0); return -1; } }
  AVPacket *c = av_packet_alloc();
  if (!c) { fail(err, errn, "Out of memory", 0); return -1; }
  av_packet_move_ref(c, p);
  if (qPush(&g->vq, c, g->vseg, 1) < 0) { av_packet_free(&c); fail(err, errn, "Out of memory", 0); return -1; }
  return 0;
}

static int writePkt(DtsRemuxGen *g, AVPacket *p, AVRational tb, AVStream *os, DtsRemuxStats *st, int video) {
  int rr;
  av_packet_rescale_ts(p, tb, os->time_base);
  if (!video && g->src->audio_offset_ms) {
    int64_t shift = av_rescale_q(g->src->audio_offset_ms, (AVRational){1, 1000}, os->time_base);
    if (p->pts != AV_NOPTS_VALUE) p->pts += shift;
    if (p->dts != AV_NOPTS_VALUE) p->dts += shift;
  }
  p->stream_index = os->index;
  rr = av_interleaved_write_frame(g->oc, p);
  av_packet_free(&p);
  if (rr >= 0 && st) { if (video) st->video_pkts++; else st->audio_pkts++; }
  return rr;
}

/* Video of segment `target` whose lookahead is complete. */
static int emitVideo(DtsRemuxGen *g, int target, DtsRemuxStats *st, char *err, size_t errn) {
  double tb = av_q2d(g->src->vtb);
  while (g->vq.n > 0) {
    QPkt *h = &g->vq.v[0];
    if (h->seg < target) { AVPacket *p = qPop(&g->vq); av_packet_free(&p); continue; }
    if (h->seg > target) break;
    if (!g->demuxEof && g->vq.n <= LOOK) break;
    int64_t d;
    if (g->src->mov && h->p->dts != AV_NOPTS_VALUE) d = h->p->dts;
    else {
      /* The n-th packet in decode order gets the n-th SMALLEST pts (the
       * presentation timeline, one frame per step), minus LOOK frames. The
       * first version took min(pts[n..n+LOOK]), which with B-pyramids stalls
       * and jumps: DTS came out in pairs 1 tick apart, then +2 frames (seen in
       * every TS made here). Popping the smallest pending pts keeps them one
       * frame apart and still depends only on the packets: a segment made
       * alone pops the same values as the same span of a continuous run. */
      int64_t mn = INT64_MAX;
      for (int i = 0; i <= LOOK && i < g->vq.n; i++) {
        int64_t q = g->vq.v[i].p->pts;
        if ((g->lastPop == UNSET || q > g->lastPop) && q < mn) mn = q;
      }
      for (int i = 0; i < g->npend; i++)
        if ((g->lastPop == UNSET || g->pend[i] > g->lastPop) && g->pend[i] < mn) mn = g->pend[i];
      if (mn == INT64_MAX) mn = h->p->pts;
      g->lastPop = mn;
      { int k = 0;   /* drop the popped; keep this packet's pts if still pending */
        for (int i = 0; i < g->npend; i++) if (g->pend[i] > mn) g->pend[k++] = g->pend[i];
        g->npend = k;
        if (h->p->pts > mn && g->npend < (int)(sizeof g->pend / sizeof *g->pend)) g->pend[g->npend++] = h->p->pts; }
      d = mn - LOOK * g->vdur;
    }
    if (g->lastDts != UNSET && d <= g->lastDts) d = g->lastDts + 1;
    if (d > h->p->pts) d = h->p->pts;
    g->lastDts = d;
    AVPacket *p = qPop(&g->vq);
    p->dts = d;
    double ps = p->pts * tb, dur = p->duration * tb;
    if (st) {
      if (st->video_pkts == 0) st->first_pts_s = ps;
      if (ps > st->last_pts_s) st->last_pts_s = ps;
    }
    g->logMedia += dur; g->mediaTotal += dur;
    if (g->hevcFix) {
      uint8_t *nb = NULL;
      int nn = dts_remux_hevc_annexb(g->src->hvccB, g->src->hvccBN, g->src->hvccLen, p->data, p->size, &nb);
      if (nn > 0) {
        AVBufferRef *b = av_buffer_create(nb, (size_t)nn + AV_INPUT_BUFFER_PADDING_SIZE, av_buffer_default_free, NULL, 0);
        if (!b) { av_free(nb); av_packet_free(&p); fail(err, errn, "Out of memory", 0); return -1; }
        av_buffer_unref(&p->buf);
        p->buf = b; p->data = b->data; p->size = nn;
      }
      /* nn <= 0: left as is; the muxer's own filter converts it, as before */
    }
    int rr = writePkt(g, p, g->src->vtb, g->ov, st, 1);
    if (rr < 0) { fail(err, errn, g->wfail ? "Client closed" : "Mux video", rr); return g->wfail ? -2 : -1; }
  }
  return 0;
}

static int emitAudio(DtsRemuxGen *g, int target, DtsRemuxStats *st, char *err, size_t errn) {
  while (g->aq.n > 0) {
    QPkt *h = &g->aq.v[0];
    if (h->seg < target) { AVPacket *p = qPop(&g->aq); av_packet_free(&p); continue; }
    if (h->seg > target) break;
    int rr = writePkt(g, qPop(&g->aq), (AVRational){1, 48000}, g->oa, st, 0);
    if (rr < 0) { fail(err, errn, g->wfail ? "Client closed" : "Mux audio", rr); return g->wfail ? -2 : -1; }
  }
  return 0;
}

/* Seek to the keyframe `kf` (video time base); audio grid from b48. */
static int freshStart(DtsRemuxGen *g, int64_t kf, int64_t b48, char *err, size_t errn) {
  int rr;
  qClear(&g->vq); qClear(&g->aq);
  closeMux(g, 0);
  if (resetAudio(g, err, errn) < 0) return -1;
  g->demuxEof = 0; g->vseg = -1; g->vstarted = 0; g->lastDts = UNSET; g->nextSeg = -1;
  g->npend = 0; g->lastPop = UNSET;
  g->gridLazy = b48 == GRID_LAZY;
  if (b48 == UNSET || b48 <= 0) g->grid = 0;
  else {
    int64_t g0 = (b48 / g->frame) * g->frame - (int64_t)PREROLL * g->frame;
    g->grid = g0 < 0 ? 0 : g0;
  }
  /* A stopped or interrupted run leaves the read error sticky in the AVIO
   * context; the seek below starts clean. */
  if (g->ic->pb) { g->ic->pb->error = 0; g->ic->pb->eof_reached = 0; }
  rr = avformat_seek_file(g->ic, g->src->vs, INT64_MIN, kf, kf, AVSEEK_FLAG_BACKWARD);
  if (rr < 0) {
    /* No index entry: seek by time on any stream. */
    int64_t t = av_rescale_q(kf, g->src->vtb, AV_TIME_BASE_Q);
    rr = avformat_seek_file(g->ic, -1, INT64_MIN, t, t, AVSEEK_FLAG_BACKWARD);
  }
  if (rr < 0) { fail(err, errn, "Container seek", rr); return -1; }
  return 0;
}

static double genCpu(DtsRemuxGen *g) {
  return g->cpuAcc + (g->inRun ? now_s(CLOCK_THREAD_CPUTIME_ID) - g->runCpu0 : 0.0);
}

static void rollingLog(DtsRemuxGen *g, int force) {
  if (g->logWall > 0 && g->logMedia < 30.0 && !(force && g->logMedia > 1.0)) return;
  struct rusage ru;
  double cpu = genCpu(g), wall = now_s(CLOCK_MONOTONIC), proc = 0;
  DtsEngineMetrics m;
  if (!getrusage(RUSAGE_SELF, &ru))
    proc = ru.ru_utime.tv_sec + ru.ru_utime.tv_usec / 1e6 + ru.ru_stime.tv_sec + ru.ru_stime.tv_usec / 1e6;
  dts_engine_metrics(g->eng, &m);
  double blocked = m.range_blocked_ns / 1e9;
  if (g->logWall > 0) {
    double media = g->logMedia, dw = wall - g->logWall;
    printf("[dts-tpk] cpu: converter %.1f%% of one core (audio %.1f%%), app total %.1f%% over %.0f s of media; "
           "made in %.1f s (%.1fx realtime), waited %.1f s for the network\n",
           100.0 * (cpu - g->logCpu) / media, 100.0 * (g->audioCpu - g->logAudio) / media,
           dw > 0 ? 100.0 * (proc - g->logProc) / dw : 0.0, media, dw, dw > 0 ? media / dw : 0.0,
           blocked - g->logBlocked);
    fflush(stdout);
  }
  g->logCpu = cpu; g->logAudio = g->audioCpu; g->logWall = wall; g->logProc = proc;
  g->logBlocked = blocked; g->logMedia = 0;
}

int dts_remux_gen_run(DtsRemuxGen *g, int seg, double startS, const char *fmt,
                      DtsRemuxWrite w, void *opaque, volatile int *stop,
                      DtsRemuxStats *st, char *err, size_t errn) {
  DtsRemux *r;
  DtsRemuxStats local;
  int target, rr = 0, fresh;
  double c0, w0;
  DtsEngineMetrics m0, m1;
  if (err && errn) err[0] = 0;
  if (!g) { fail(err, errn, "No generator", 0); return -1; }
  r = g->src;
  if (!st) st = &local;
  memset(st, 0, sizeof *st);
  c0 = now_s(CLOCK_THREAD_CPUTIME_ID); w0 = now_s(CLOCK_MONOTONIC);
  double a0 = g->audioCpu;
  g->runCpu0 = c0; g->inRun = 1;
  dts_engine_metrics(g->eng, &m0);
  if (g->logWall <= 0) rollingLog(g, 0);   /* starts the 30 s window */
  g->w = w; g->wop = opaque; g->wfail = 0; g->bytes = 0;
  /* A previous interrupt is over: this run reads again. Cleared BEFORE *stop
   * is read, so an interrupt racing with the start is never lost (the caller
   * sets *stop first, then interrupts). */
  atomic_store(&g->interrupted, 0);
  dts_engine_interrupt(g->eng, 0);
  if (stop && *stop) { rr = -2; goto out; }
  if (seg >= 0) {
    if (seg >= r->nseg) { fail(err, errn, "No such segment", 0); rr = -1; goto out; }
    fresh = g->nextSeg != seg || !g->oc;
    st->fresh = fresh;
    g->prog = 0; target = seg;
    if (fresh && freshStart(g, r->segTs[seg], r->segB48[seg], err, errn) < 0) { rr = -1; goto out; }
    if (fresh) { if (openMux(g, "mpegts", err, errn) < 0) { rr = g->wfail ? -2 : -1; goto out; } }
    else av_opt_set(g->oc->priv_data, "mpegts_flags", "resend_headers", 0);  /* PAT/PMT open the segment */
  } else {
    int64_t want = av_rescale_q((int64_t)llround(startS * 1000.0), (AVRational){1, 1000}, r->vtb);
    int k = r->nkf > 0 ? floorIdx(r->kf, r->nkf, want) : -1;
    /* Past the end of an incomplete index (MKV without Cues): seek by time; the
     * demuxer reads on to the keyframe at or before `want`. */
    int pastIdx = !r->info.index_complete && (r->nkf < 1 || want > r->kf[r->nkf - 1]);
    int64_t kf = pastIdx ? want : k >= 0 ? r->kf[k] : (r->nkf > 0 ? r->kf[0] : want);
    fresh = 1; g->prog = 1; target = 0;
    st->fresh = 1;
    /* By time, the keyframe the demuxer lands on (at or before `want`) is
     * not known here: every one after the seek counts. */
    g->progKf = pastIdx ? INT64_MIN : kf; g->progStartS = kf * av_q2d(r->vtb);
    g->progB48 = startS <= 0.05 ? UNSET : pastIdx ? GRID_LAZY : llround(g->progStartS * 48000.0);
    if (g->progStartS < 0) g->progStartS = 0;
    if (freshStart(g, kf, g->progB48, err, errn) < 0) { rr = -1; goto out; }
    if (openMux(g, fmt && *fmt ? fmt : "mpegts", err, errn) < 0) { rr = g->wfail ? -2 : -1; goto out; }
  }
  for (;;) {
    if ((stop && *stop) || (r->extCancel && *r->extCancel)) { fail(err, errn, "Stopped", 0); rr = -2; break; }
    if ((rr = emitVideo(g, target, st, err, errn)) < 0) break;
    if ((rr = emitAudio(g, target, st, err, errn)) < 0) break;
    rollingLog(g, 0);
    int vDone = (g->vq.n > 0 && g->vq.v[0].seg > target) || (g->demuxEof && g->vq.n == 0);
    int aDone = (g->aq.n > 0 && g->aq.v[0].seg > target) || (g->encEof && g->aq.n == 0);
    /* No audio past the boundary although the video is 4 s beyond it: the
     * track ended or has a hole. The segment goes out without it. */
    if (vDone && !aDone && !g->prog && seg + 1 < r->nseg && g->lastDemuxV > r->segS[seg + 1] + 4.0) aDone = 1;
    if (vDone && aDone) break;
    if (g->demuxEof) {
      if (!g->encEof) { if ((rr = audioEof(g, err, errn)) < 0) break; continue; }
      break;
    }
    rr = av_read_frame(g->ic, g->pk);
    if (rr == AVERROR_EOF) { g->demuxEof = 1; rr = 0; continue; }
    if (rr < 0) {
      if (rr == AVERROR_EXIT || (r->extCancel && *r->extCancel)) { fail(err, errn, "Stopped", 0); rr = -2; }
      else { fail(err, errn, "Container read", rr); rr = -1; }
      break;
    }
    if (g->pk->stream_index == r->vs) rr = videoPacket(g, g->pk, err, errn);
    else if (g->pk->stream_index == r->as) rr = audioPacket(g, g->pk, err, errn);
    av_packet_unref(g->pk);
    if (rr < 0) break;
  }
  if (rr >= 0) {
    /* End of segment: interleaving queue and the TS muxer's PES buffers out. */
    av_interleaved_write_frame(g->oc, NULL);
    av_write_frame(g->oc, NULL);
    if (g->oc->pb) avio_flush(g->oc->pb);
    if (g->wfail) { fail(err, errn, "Client closed", 0); rr = -2; }
  }
out:
  /* Whatever failed while the player had already given up on this run (its
   * reads were cut by the interrupt, a seek among them) is a stop, not an
   * error: no 500 for a client that is gone, no error in the summary. */
  if (rr == -1 && ((stop && *stop) || atomic_load(&g->interrupted) || (r->extCancel && *r->extCancel))) rr = -2;
  if (rr == -2) fail(err, errn, "Stopped", 0);
  if (rr >= 0 && !g->prog && seg + 1 < r->nseg && !g->demuxEof) {
    g->nextSeg = seg + 1;           /* continuation: muxer and queues stay */
  } else {
    closeMux(g, rr >= 0);
    g->nextSeg = -1;
  }
  st->bytes_out = g->bytes;
  st->wall_s = now_s(CLOCK_MONOTONIC) - w0;
  st->cpu_s = now_s(CLOCK_THREAD_CPUTIME_ID) - c0;
  g->cpuAcc += st->cpu_s; g->inRun = 0;
  st->audio_cpu_s = g->audioCpu - a0;
  dts_engine_metrics(g->eng, &m1);
  st->range_wait_s = (m1.range_blocked_ns - m0.range_blocked_ns) / 1e9;
  if (g->badAudio) {
    printf("[dts-tpk] audio: %ld corrupt unit(s) skipped by the decoder\n", g->badAudio);
    fflush(stdout);
    g->badAudio = 0;
  }
  if (g->prog || rr < 0) rollingLog(g, 1);
  g->w = NULL; g->wop = NULL;
  return rr < 0 ? rr : 0;
}

#endif
