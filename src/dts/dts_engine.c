#include "dts_engine.h"
#include "dts_audio_codec.h"
#include "dv7.h"
#include "dv7_mp4.h"
#include "hevc_annexb.h"
#include "dts_picture_diag.h"
#ifdef NV_DTS_PICTURE_DIAG
#include "dts_video_diag.h"
#endif
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
static int g_conversion_workers=4, g_source_workers=1;
static int valid_workers(int n,int fallback) { return n>=1 && n<=4?n:fallback; }
void dts_engine_set_http_workers(int n) { __atomic_store_n(&g_conversion_workers,valid_workers(n,4),__ATOMIC_RELAXED); }
void dts_engine_set_source_workers(int n) { __atomic_store_n(&g_source_workers,valid_workers(n,1),__ATOMIC_RELAXED); }
static volatile int g_ahead_mib;
void dts_engine_set_ahead_mib(int mib) { g_ahead_mib = mib > 0 && mib <= 256 ? mib : 0; }
/* Settings > From another source > Other version buffer: its reader's ring. */
static volatile int g_ext_ring_mib = DTS_ENGINE_EXTERNAL_RING_MIB;
void dts_engine_set_external_ring_mib(int mib) { g_ext_ring_mib = mib >= 8 && mib <= 32 ? mib : DTS_ENGINE_EXTERNAL_RING_MIB; }
/* Test option (webOS buffer settings): conversion opened after this call
 * reads the source over ONE continuous HTTP request into a ring, the reader
 * the Samsung remux and the native webOS cache already use, instead of 4 MiB
 * range requests. Per-request latency (~220 ms on TorBox) no longer caps a
 * single connection. */
static volatile int g_conversion_stream;
void dts_engine_set_conversion_stream(int on) { g_conversion_stream = on ? 1 : 0; }
/* Settings > DTS "Off": engines opened after this never select or switch to a
 * DTS track (the controller then hands the source back to the TV player). */
static volatile int g_dts_off;
/* 1 = DTS off; 2 = off and the TV decodes DTS (tv.model.edidType): on the
 * Dolby Vision path the DTS stream then passes through untouched. */
void dts_engine_set_dts_off(int on) { g_dts_off = on == 2 ? 2 : on ? 1 : 0; }
int dts_engine_dts_off(void) { return g_dts_off; }
static int (*g_lang_match)(const char *code, const char *pref);
void dts_engine_set_lang_match(int (*match)(const char *code, const char *pref)) { g_lang_match = match; }
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
void dts_engine_dv_audio(DtsEngine *e,int mode) { (void)e;(void)mode; }
void dts_engine_allow_p7(DtsEngine *e, int enabled) { (void)e; (void)enabled; }
void dts_engine_p7_mapping(DtsEngine *e,int enabled) { (void)e;(void)enabled; }
void dts_engine_mp4_test(DtsEngine *e,int enabled) { (void)e;(void)enabled; }
void dts_engine_metrics(DtsEngine *e,DtsEngineMetrics *out) { (void)e; if(out) memset(out,0,sizeof(*out)); }
int dts_engine_open(DtsEngine *e,const char *u,const char *h,int a,int c,double s) {
  (void)e;(void)u;(void)h;(void)a;(void)c;(void)s; return -1;
}
const DtsMediaInfo *dts_engine_info(DtsEngine *e) { return e ? &e->info : NULL; }
int dts_engine_has_core(DtsEngine *e) { (void)e; return 0; }
const void *dts_engine_subtitle_parameters(DtsEngine *e,int i) { (void)e;(void)i; return NULL; }
int dts_engine_next(DtsEngine *e,DtsFrame *f) { (void)e;(void)f; return -1; }
int dts_engine_audio(DtsEngine *e,int a,int c,double t) { (void)e;(void)a;(void)c;(void)t;return -1; }
int dts_engine_seek(DtsEngine *e,double s) { (void)e;(void)s; return -1; }
void dts_engine_cancel(DtsEngine *e) { (void)e; }
void dts_engine_destroy(DtsEngine *e) { free(e); }
const char *dts_engine_error(DtsEngine *e) { (void)e; return "DTS conversion not built"; }
int dts_engine_open_demux(DtsEngine *e,const char *u,const char *h) { (void)e;(void)u;(void)h; return -1; }
void *dts_engine_avformat(DtsEngine *e) { (void)e; return NULL; }
int64_t dts_engine_size(DtsEngine *e) { (void)e; return -1; }
void dts_engine_interrupt(DtsEngine *e,int on) { (void)e; (void)on; }
void dts_engine_external_audio(DtsEngine *e,const char *u,const char *h,const char *l,int o) { (void)e;(void)u;(void)h;(void)l;(void)o; }
void dts_engine_external_offset(DtsEngine *e,int o) { (void)e;(void)o; }
void dts_engine_external_audio_only(DtsEngine *e,int on) { (void)e;(void)on; }
void dts_engine_external_track(DtsEngine *e,int s) { (void)e;(void)s; }
void dts_engine_external_mismatch(DtsEngine *e,int *c,int *s,int *r) { (void)e; *c=0; *s=0; if(r) *r=0; }
int dts_engine_external_shift(DtsEngine *e,int o) { (void)e;(void)o; return -1; }
int dts_engine_external_shift_quiet(DtsEngine *e,int o) { (void)e;(void)o; return -1; }
int dts_engine_probe_audio(const char *u,const char *h,DtsTrack *t,int m) { (void)u;(void)h;(void)t;(void)m; return -1; }
int dts_engine_external_match_arm(DtsEngine *e) { (void)e; return -1; }
int dts_engine_external_match_poll(DtsEngine *e,DtsMatchInfo *o) { (void)e; memset(o,0,sizeof *o); return -1; }
void dts_engine_external_speed(DtsEngine *e,int p) { (void)e;(void)p; }
int dts_engine_prefetch(DtsEngine *e,double s) { (void)e;(void)s; return -1; }
void dts_engine_prefetch_tidy(DtsEngine *e) { (void)e; }
void dts_engine_reconnect(DtsEngine *e) { (void)e; }
int dts_engine_external_wake(DtsEngine *e) { (void)e; return 0; }
int dts_engine_external_woken(DtsEngine *e) { (void)e; return 0; }
int dts_engine_external_failed(DtsEngine *e) { (void)e; return 0; }
void dts_engine_forget_final(const char *u,const char *h) { (void)u; (void)h; }
void dts_engine_external_follow(DtsEngine *e,int on) { (void)e;(void)on; }
void dts_engine_external_track_hint(DtsEngine *e,int s,const char *l,const char *c,int ch,const char *t) { (void)e;(void)s;(void)l;(void)c;(void)ch;(void)t; }
void dts_engine_external_automatch(DtsEngine *e,int on) { (void)e;(void)on; }
int dts_engine_external_follow_poll(DtsEngine *e,int *s,int *o) { (void)e; *s=0; *o=0; return 0; }
int64_t dts_engine_external_bytes(DtsEngine *e,int64_t *t) { (void)e; if(t) *t=-1; return 0; }
int dts_engine_external_active(DtsEngine *e) { (void)e; return 0; }
int dts_engine_external_stream(DtsEngine *e) { (void)e; return -1; }
void dts_engine_subtitle_only(DtsEngine *e,int stream) { (void)e; (void)stream; }
int dts_engine_external_replace(DtsEngine *e,const char *u,const char *h,const char *l,int o,int t,int a,double s) { (void)e;(void)u;(void)h;(void)l;(void)o;(void)t;(void)a;(void)s; return -1; }
#else
#include "../rede.h"
#include <limits.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/syscall.h>
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
#ifdef NV_MP4_BD_TEST
#define MP4_BASE_QUEUE 8
#define MP4_RPU_QUEUE 64
typedef struct {
  AVPacket *base[MP4_BASE_QUEUE];
  int base_count, rpu_count, aligned, align_drops, el_stream, base_length, el_length;
  size_t base_bytes;
  int anchor_valid, first_base_seen, preroll_skipped;
  int64_t anchor_dts, anchor_pts;
  size_t preroll_bytes;
  struct { uint8_t data[NV_MP4_RPU_MAX]; size_t size; int64_t pts,dts; } rpu[MP4_RPU_QUEUE];
} NvMp4Pair;
#endif
struct DtsEngine {
#ifdef NV_HTTP_BUFFER_DIAG
  unsigned diag_id;
#endif
#ifdef NV_DTS_PICTURE_DIAG
  unsigned picture_epoch, picture_count;
  unsigned demux_picture_count;
  NvVideoTiming demux_timing, output_timing;
#endif
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
  int range_threads, range_stop, http_workers;
  int source_range;
  int64_t source_position, source_available;
  volatile int range_live;
  int64_t range_next;
  uint64_t rate_until_ns[2]; /* resolver chain / cached media address */
  unsigned rate_strikes[2];
  long range_bytes;
  int dv, dv_audio, passthrough, range_slots, final_cross, opening;
  int source_stream; /* Native HTTP cache; conversion readers retain their behavior. */
  int conv_stream;   /* Conversion over one continuous request (test option). */
  int dts_off;       /* Settings > DTS "Off", snapshot at open. */
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
  /* Audio of another version (parent set): a wait of the reader that lasts
   * too long reconnects at the same place (xs_tries at xs_at, a fresh link
   * from the add-on on the second), then gives up (ext_dead). xs_waiting:
   * the reader is waiting now (a track change or seek may wake it). */
  int64_t xs_at; int xs_tries, ext_dead, xs_quiet; volatile int xs_waiting;
  /* Speed guard (speed_guard): the current connection's first byte (time and
   * ring end then), and a window of the reader's waiting: since sg_win_ms,
   * fx_hi was sg_win_hi, sg_blocked_ms spent waiting, waiting now since
   * sg_wait_ms (0: not waiting). */
  uint64_t sg_first_ms, sg_win_ms, sg_blocked_ms, sg_wait_ms; int64_t sg_win_hi;
  /* Versions of another source (its reader and the track probe): the final
   * address behind the add-on is remembered between engines (final_cache);
   * final_seeded: this one started from a remembered address that has not
   * sent a byte yet. */
  int remember_final, final_seeded;
  unsigned final_age_s;      /* age of the remembered address when this engine started from it */
  /* Main engine: the other version's read was woken for a pending request
   * (ext_woken, until its seek or swap) or gave up (ext_failed). Either way
   * the picture goes on without its packets. */
  int ext_woken, ext_failed;
  char final_url[8192];
  /* PREFETCH (owner's log 07/11: after an intro skip the slow source starved
   * the DV path for 20 s): the bytes at a coming jump target, fetched over a
   * request of their own while playback goes on. A seek landing there is
   * served from them while the stream restarts after them. */
  uint8_t *pf_buf; int64_t pf_lo, pf_hi, pf_cap; double pf_seconds;
  int64_t pf_ts, pf_p0, pf_t0, pf_tend; AVRational pf_tb;   /* where to look when the index has no entry near */
  pthread_t pf_thread; int pf_thread_ok; volatile int pf_cancel, pf_done; int pf_used; int64_t pf_served;
  AVFormatContext *fmt;
  AVIOContext *io;
  AVCodecContext *dec, *enc;
  AVBSFContext *bsf;
  /* HEVC hvcC: own Annex-B conversion instead of bsf (keeps the stream's
   * VPS/SPS/PPS, see hevc_annexb.h). hevc_annexb.sets!=NULL means active. */
  NvHevcAnnexB hevc_annexb;
#ifdef NV_DTS_PICTURE_DIAG
  unsigned annexb_modes[4], annexb_logged;
#endif
  SwrContext *swr;
  AVAudioFifo *fifo;
  AVPacket *in, *out;
  int allow_p7, dv7, preserve_p7_mapping, mapping_reported;
  uint8_t *dv7_buffer;
  unsigned dv7_capacity;
#ifdef NV_MP4_BD_TEST
  int allow_mp4, mp4_p8_compat_verified;
  NvMp4Pair *mp4;
#endif
  AVFrame *decoded, *encoded;
  AVChannelLayout input_layout;
  enum AVSampleFormat input_format;
  int input_rate, demux_eof, decoder_eof, resampler_eof, encoder_sent_eof;
  int64_t sample_pts, target_ns;
  int64_t pad_anchor; /* Output samples; budget follows decoded media time. */
  int pad_count, pad_samples;
  int timeline, has_core;
  /* External audio (dts_engine_external_audio). The configuration survives
   * reopen; `ext` is the audio-only reader of the other file, owned by the
   * worker and detached under range_lock so cancel can reach it. */
  char *ext_url, *ext_headers, ext_langs[64];
  int64_t ext_offset_ns;
  int64_t ext_base_ns;   /* this source's start minus the other's (MPEG-TS starts at ~1.4 s) */
  struct DtsEngine *ext, *parent;
  int fx_mib;                /* ring of this reader in MiB (0: g_fx_cap_mib) */
  int ext_stream, ext_have, ext_eof;
  int ext_force;   /* donor stream + 1 chosen in the menu; 0 = by language */
  /* Series memory (owner 09/11): the track chosen in the last episode's
   * version, looked for in this one when nothing is forced (ext_hint_pick). */
  int ext_hint_on, ext_hint_idx, ext_hint_ch;
  char ext_hint_lang[16], ext_hint_codec[24], ext_hint_title[64];
  int ext_idx;     /* the donor has an index: 0 unknown, 1 yes, 2 no */
  int ext_cut_s, ext_speed;   /* other version minus this one, seconds; other speed */
  int ext_speed_auto;         /* the speed came from the frame rates (configure_external) */
  int64_t ext_last_end_ns, ext_floor_ns;   /* end of the last audio given out; drop audio before */
  struct MatchCap *mt;   /* "Match automatically": capture in progress */
  int mt_state, mt_offset_ms, mt_score;   /* 0 none, 1 capturing, 2 found, 3 not trusted / failed */
  int mt_ratio_ppm, mt_how, mt_hint_ms, mt_heavy;   /* the rest of DtsMatchInfo */
  int mt_ncand, mt_cand_ms[3], mt_cand_sc[3];
  uint64_t mt_started_ms;
  struct MatchJob *mj;   /* background search over a reader of its own */
  int mt_check;          /* the capture in progress is the second look (speed) */
  /* The sync checked all through the episode (owner's log 16/11, Chicago
   * Fire: a version 2.2 s shorter, its offset growing in steps at the
   * breaks): every 3 minutes, 6 once it held twice. mt_later marks a find of
   * such a check for the app (DtsMatchInfo.later). */
  int mt_watch, mt_same, mt_later, mt_stepped;
  /* A sync change from such a check waits for a quiet moment of the other
   * version's audio (struct Quiet), up to QUIET_MAX_MS. */
  struct Quiet *qs; int qs_offset_ms; uint64_t qs_since_ms;
  double mt_t1, mt_o1;   /* first find: this file's time and raw offset, s (mt_t1 < 0: none) */
  int64_t mt_o1_user_ns; /* the offset that find reported (a manual change forgets it) */
  double ext_ratio;      /* other version's speed (0 or 1: the same; dts_engine_external_speed) */
  /* FOLLOW ANOTHER CUT (Settings, off by default): struct Follow below. */
  int fw_on, fw_state, fw_autoarm, mt_auto;
  /* The follow started by itself for versions whose gaps differ (mt_watch):
   * small shifts only, each made at a pause; never this file's own audio,
   * so it needs no conversion path (owner 16/11: checks minutes apart left
   * each break's 0.6 s heard for minutes). */
  int fw_small;
  int mt_main_pref;      /* this file's track that compared best, +1 (0: none; see match_alt) */
  int ext_automatch;     /* Settings: "Match automatically" by itself when the other version starts */
  int mt_retry;          /* automatic captures started again after a jump cut them short */
  struct Follow *fw;
  int fw_event, fw_event_ms;   /* last event for the app: 1 own audio, 2 resumed, 3 gave up, 4 small shift corrected */
  double ext_fps_ratio;  /* other version's frame rate / this one's (0: unknown) */
  int64_t fx_live_total;   /* stream size from the response headers (rede_fluxo64) */
  /* ext_conv: the donor's audio is decoded and re-encoded (DTS, TrueHD,
   * Opus, FLAC, 44.1 kHz AAC...) through this engine's own dec/swr/fifo/enc,
   * idle while external audio is on; ext_in holds the donor's packets. */
  int ext_conv, ext_demux_eof, ext_dec_eof, ext_swr_eof, ext_enc_eof;
  int dv_strip;              /* external audio over an unsupported DV layer: base layer only */
  int ext_audio_only;        /* Settings: fetch only the donor's audio where the file allows it */
  /* AUDIO-ONLY READER of a donor MP4 (on the donor's own engine): the byte
   * ranges of its audio chunks, read from the index, fetched ahead by one
   * thread into a few slots; the video between them is never downloaded. */
  int64_t *sp_pos; int *sp_size; int sp_n, sp_want, sp_err, sp_stop, sp_thread_ok;
  struct { int idx, state; char *data; } sp_slot[12];   /* state 0 free, 1 fetching, 2 ready */
  pthread_t sp_thread;
  int64_t sp_bytes;
  AVPacket *ext_pkt, *ext_in;
  int64_t ext_pts, ext_clock_ns; /* pending packet time; newest video handed out */
  DtsEngineMetrics metrics;
#if defined(NV_DTS_DEBUG) || defined(NV_HTTP_BUFFER_DIAG)
  DtsEngineMetrics logged_metrics;
  uint64_t logged_at_ns;
#endif
};
static uint64_t clock_ns(clockid_t clock) {
  struct timespec t;
  if(clock_gettime(clock,&t)) return 0;
  return (uint64_t)t.tv_sec*UINT64_C(1000000000)+(uint64_t)t.tv_nsec;
}
#ifdef NV_HTTP_BUFFER_DIAG
static unsigned http_diag_next;
/* Only lifecycle events: no polling thread, URLs or per-packet allocations. */
static void http_session_diag(DtsEngine *e, const char *event, uint64_t elapsed_ns) {
  int queued=0, active=0, ready=0, workers;
  uint64_t bytes=0, cooldown=0, now=clock_ns(CLOCK_MONOTONIC);
  pthread_mutex_lock(&e->range_lock);
  workers=e->range_threads;
  for(int i=0;i<RANGE_SLOTS_MAX;i++) {
    DtsRangeJob *j=&e->ranges[i];
    queued+=j->state==1; active+=j->state==2; ready+=j->state==3;
    if(j->state==3 && j->count>0)bytes+=(uint64_t)j->count;
  }
  for(int i=0;i<2;i++)if(e->rate_until_ns[i]>now && e->rate_until_ns[i]-now>cooldown)
    cooldown=e->rate_until_ns[i]-now;
  pthread_mutex_unlock(&e->range_lock);
  printf("[http-engine] id=%u event=%s selected=%d workers=%d active=%d queued=%d ready=%d readyKiB=%llu cooldownMs=%llu elapsedMs=%llu\n",
    e->diag_id,event,e->http_workers,workers,active,queued,ready,
    (unsigned long long)(bytes/1024),(unsigned long long)(cooldown/1000000),
    (unsigned long long)(elapsed_ns/1000000));
  fflush(stdout);
}
#else
#define http_session_diag(e,event,elapsed_ns) ((void)0)
#endif
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
  DtsEngine *e=opaque;
  return __atomic_load_n(&e->cancelled,__ATOMIC_RELAXED) ||
         (e->parent && __atomic_load_n(&e->parent->cancelled,__ATOMIC_RELAXED));
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
/* Shared, cancellable pauses for resolver and media traffic. An API limit must
 * not block a working CDN address published by another in-flight lane. */
static int range_target(DtsEngine *e,DtsRangeJob *job,int original,
                        char target[8192],int *direct,int *cross) {
  pthread_mutex_lock(&e->range_lock);
  for (;;) {
    if(e->range_stop || cancelled(e) || __atomic_load_n(&e->interrupt,__ATOMIC_RELAXED) ||
       __atomic_load_n(&job->cancel,__ATOMIC_RELAXED)) {
      pthread_mutex_unlock(&e->range_lock); return 0;
    }
    uint64_t now=clock_ns(CLOCK_MONOTONIC);
    *direct=!original && e->final_url[0];
    if(now>=e->rate_until_ns[*direct]) {
      *cross=*direct?e->final_cross:0;
      snprintf(target,8192,"%s",*direct?e->final_url:e->url);
      pthread_mutex_unlock(&e->range_lock); return 1;
    }
    /* Short waits also observe a slot cancellation during seek/clear_ranges. */
    struct timespec until;clock_gettime(CLOCK_REALTIME,&until);
    until.tv_nsec+=100000000;
    if(until.tv_nsec>=1000000000) {until.tv_sec++;until.tv_nsec-=1000000000;}
    pthread_cond_timedwait(&e->range_wake,&e->range_lock,&until);
  }
}
static void range_rate_result(DtsEngine *e,int status,int retry_after,int direct) {
  pthread_mutex_lock(&e->range_lock);
  uint64_t now=clock_ns(CLOCK_MONOTONIC);
  if(status==429) {
    if(e->rate_strikes[direct]<5)e->rate_strikes[direct]++;
    unsigned seconds=retry_after>0?(unsigned)retry_after:
      (e->rate_strikes[direct]<5?1u<<e->rate_strikes[direct]:30u);
    uint64_t deadline=now+(uint64_t)seconds*UINT64_C(1000000000);
    if(deadline>e->rate_until_ns[direct])e->rate_until_ns[direct]=deadline;
    printf("[range-cooldown] http=429 retrySec=%d waitMs=%llu lanes=shared direct=%d\n",
      retry_after,(unsigned long long)((e->rate_until_ns[direct]-now)/1000000),direct);
    pthread_cond_broadcast(&e->range_wake);
  } else if(status==206 && now>=e->rate_until_ns[direct])e->rate_strikes[direct]=0;
  pthread_mutex_unlock(&e->range_lock);
}
static void range_fetch(DtsEngine *e,DtsRangeJob *job,int fast,int *stalled) {
  uint64_t begun=clock_ns(CLOCK_MONOTONIC);
#ifndef NV_DTS_WEBOS_RECOVERY
  (void)fast;
#endif
  {
    /* After the first answer, go straight to the address behind the add-on
     * redirects. Credential headers follow only on the same origin. A final
     * link that stops answering (expired: 403/410, or any failure) is
     * dropped and the request repeats through the original URL. SDR/DTS uses
     * this same path: do not call the resolver/API again for each 4 MiB. */
    char target[8192], fin[8192]; int direct, cross=0, retry_after=-1;
    volatile int *guard=NULL;
#ifdef NV_DTS_WEBOS_RECOVERY
    guard=fast?&job->urgent:NULL;
#endif
    if(!range_target(e,job,0,target,&direct,&cross))return;
    job->data=rede_baixar_trecho64_relato(target,direct&&cross?(e->source_range && cross!=2?e->source_public_headers:""):e->headers,job->start,job->end,
      &job->count,&job->total,&job->status,&job->cancel,fin,&cross,
      guard,2500,stalled,&retry_after);
    range_rate_result(e,job->status,retry_after,direct);
    uint64_t first=clock_ns(CLOCK_MONOTONIC);
    int first_status=job->status, first_ok=job->data!=NULL;
    if(direct && !job->data && job->status!=429 && !*stalled && !__atomic_load_n(&job->cancel,__ATOMIC_RELAXED)) {
      pthread_mutex_lock(&e->range_lock);
      if(!strcmp(e->final_url,target)) e->final_url[0]=0;
      pthread_mutex_unlock(&e->range_lock);
      if(!range_target(e,job,1,target,&direct,&cross))return;
      job->data=rede_baixar_trecho64_relato(target,e->headers,job->start,job->end,
        &job->count,&job->total,&job->status,&job->cancel,fin,&cross,
        guard,2500,stalled,&retry_after);
      range_rate_result(e,job->status,retry_after,direct);
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
    if(!direct && job->data && fin[0] && !__atomic_load_n(&job->cancel,__ATOMIC_RELAXED)) {
      pthread_mutex_lock(&e->range_lock);
      snprintf(e->final_url,sizeof e->final_url,"%s",fin); e->final_cross=cross;
      printf("[range-route] cached=1 sdr=%d cross=%d\n",!e->dv&&!e->remux,cross);
      pthread_cond_broadcast(&e->range_wake);
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
          if(candidate->state!=1)continue;
          int priority=0,best=0;
#ifdef NV_DTS_WEBOS_RECOVERY
          priority=__atomic_load_n(&candidate->urgent,__ATOMIC_RELAXED)>=2;
          best=job && __atomic_load_n(&job->urgent,__ATOMIC_RELAXED)>=2;
#endif
          if(!job || priority>best || (priority==best && candidate->start<job->start))job=candidate;
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
  for(int i=0;i<e->http_workers;i++) {
    DtsRangeWorker *worker=&e->workers[i]; worker->engine=e;
    if(pthread_create(&worker->thread,NULL,range_worker,worker)) break;
    e->range_threads++;
  }
  e->range_next=e->pos;
  int ok=e->range_threads>0;
  pthread_mutex_unlock(&e->range_lock);
  http_session_diag(e,"range-start",0);
  return ok;
}
#ifdef NV_NATIVE_SEEK_TEST
/* Worker-owned repositions may need an earlier keyframe that has just left
 * the active range. Reuse a small window of slots, never the whole cache.
 * No new payload allocation or slot: retain useful read-ahead within v12's cap.
 * Startup, distant seeks, SDR/DTS and remux retain their existing behavior. */
static int reuse_missing_range(DtsEngine *e) {
  if(!e->dv || e->remux || !__atomic_load_n(&e->range_live,__ATOMIC_RELAXED) ||
     !e->cache || e->cache_size<=0)return 0;
  int64_t high=e->cache_start+e->cache_size;
  for(int i=0;i<e->range_slots;i++) {
    DtsRangeJob *j=&e->ranges[i];
    if(j->state && j->end>high)high=j->end;
  }
  int64_t back=(int64_t)(e->range_slots+1)*e->range_bytes;
  if(e->pos<e->cache_start-back || e->pos>high)return 0;
  DtsRangeJob *slot=NULL;uint64_t farthest=0;
  for(int i=0;i<e->range_slots;i++) {
    DtsRangeJob *j=&e->ranges[i];
    if(j->state==2)continue; // An HTTP lane owns this slot until completion.
    if(!j->state || (j->state==3 && !j->data)) { slot=j;break; }
    uint64_t distance=j->start>e->pos?(uint64_t)(j->start-e->pos):(uint64_t)(e->pos-j->start);
    if(!slot || distance>farthest) { slot=j;farthest=distance; }
  }
  if(!slot)return 0;
  free(slot->data);slot->data=NULL;slot->state=0;
  int64_t end=e->pos>INT64_MAX-e->range_bytes?INT64_MAX:e->pos+e->range_bytes-1;
  for(int i=0;i<e->range_slots;i++) {
    DtsRangeJob *j=&e->ranges[i];
    if(j->state && j->start>e->pos && j->start<=end)end=j->start-1;
  }
  slot->start=e->pos;slot->end=end;slot->count=0;slot->total=-1;slot->status=0;
  __atomic_store_n(&slot->cancel,0,__ATOMIC_RELAXED);
  __atomic_store_n(&slot->urgent,2,__ATOMIC_RELAXED);
  slot->state=1;e->range_next=end==INT64_MAX?INT64_MAX:end+1;
  /* A nearly full cache can consist entirely of ranges AFTER the gap. With
   * only one recycled slot the gap then downloads serially, ~4 MiB per RTT,
   * regardless of the large ready-byte count. Give the existing HTTP lanes
   * a contiguous window to fill; sacrifice at most three farthest payloads.
   * Active slots remain owned by their workers, and the allocation cap stays
   * exactly the same. Ordinary reads recycle these slots as they are consumed. */
  int64_t window=e->pos>INT64_MAX-(int64_t)RANGE_WORKERS*e->range_bytes?
    INT64_MAX:e->pos+(int64_t)RANGE_WORKERS*e->range_bytes;
  for(int i=0;i<e->range_slots;i++) {
    DtsRangeJob *j=&e->ranges[i];
    if(j!=slot && j->state && !(j->state==3 && !j->data) &&
       j->start>e->pos && j->start<window)window=j->start;
  }
  int needed=(int)((window-e->pos-1)/e->range_bytes)+1,idle=0;
  for(int i=0;i<e->range_slots;i++)idle+=e->ranges[i].state==0;
  int recycled=0;
  while(idle<needed-1) {
    DtsRangeJob *far=NULL;uint64_t dist_max=0;
    for(int i=0;i<e->range_slots;i++) {
      DtsRangeJob *j=&e->ranges[i];
      if(!j->state || j->state==2 || j==slot || j->start<window)continue;
      uint64_t dist=(uint64_t)(j->start-e->pos);
      if(!far || dist>dist_max) { far=j;dist_max=dist; }
    }
    if(!far)break;
    free(far->data);far->data=NULL;far->state=0;idle++;recycled++;
  }
  // If every lane is blocked on speculative data, the needed keyframe must
  // not wait behind all of them. Ask only the farthest owner to cancel; its
  // slot remains owned by that worker until it publishes completion.
  int busy=0,interrupted=0;DtsRangeJob *victim=NULL;uint64_t distance_max=0;
  for(int i=0;i<e->range_slots;i++)if(e->ranges[i].state==2) {
    DtsRangeJob *j=&e->ranges[i];busy++;
    uint64_t distance=j->start>e->pos?(uint64_t)(j->start-e->pos):(uint64_t)(e->pos-j->start);
    if(!victim || distance>distance_max) { victim=j;distance_max=distance; }
  }
  if(victim && busy==e->range_threads) {
    __atomic_store_n(&victim->cancel,1,__ATOMIC_RELAXED);interrupted=1;
  }
  int retained=0;int64_t bytes=0;
  for(int i=0;i<e->range_slots;i++)if(e->ranges[i].state==3 && e->ranges[i].data) {
    retained++;bytes+=e->ranges[i].count;
  }
  printf("[range-reuse] target=%lld retained=%d retainedBytes=%lld interrupted=%d refillSlots=%d recycled=%d\n",
    (long long)e->pos,retained,(long long)bytes,interrupted,needed<idle+1?needed:idle+1,recycled);
  fflush(stdout);return 1;
}
/* Skip already queued/ready intervals after filling a hole. Do not overlap
 * retained payloads or turn a capped short HTTP response into skipped bytes. */
static void skip_queued_ranges(DtsEngine *e) {
  for(int pass=0;pass<=e->range_slots;pass++) {
    int moved=0;
    for(int i=0;i<e->range_slots;i++) {
      DtsRangeJob *j=&e->ranges[i];
      if(!j->state || (j->state==3 && (!j->data || j->count<=0)))continue;
      int64_t end=j->state==3?j->start+j->count-1:j->end;
      if(e->range_next>=j->start && e->range_next<=end && end<INT64_MAX) {
        e->range_next=end+1;moved=1;break;
      }
    }
    if(!moved)break;
  }
}
#endif
/* Called under range_lock; only idle slots can receive new work. */
static void queue_ranges(DtsEngine *e) {
  for(int i=0;i<e->range_slots && !cancelled(e);i++) {
    DtsRangeJob *job=&e->ranges[i];
    if(job->state)continue;
#ifdef NV_NATIVE_SEEK_TEST
    int preserve=e->dv && !e->remux && __atomic_load_n(&e->range_live,__ATOMIC_RELAXED);
    if(preserve)skip_queued_ranges(e);
#endif
    if(e->total>=0 && e->range_next>=e->total)continue;
    job->start=e->range_next;
    job->end=job->start>INT64_MAX-e->range_bytes?INT64_MAX:job->start+e->range_bytes-1;
#ifdef NV_NATIVE_SEEK_TEST
    if(preserve)for(int k=0;k<e->range_slots;k++) {
      DtsRangeJob *j=&e->ranges[k];
      if(j!=job && j->state && j->start>job->start && j->start<=job->end)job->end=j->start-1;
    }
#endif
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
/* Long-lived ring (native cache or the conversion test option): the ring is
 * the read-ahead buffer, and a jump outside it restarts the one request.
 * The Samsung remux keeps its short adaptive read-ahead. */
static int fx_long(const DtsEngine *e) { return e->source_stream || e->conv_stream; }
/* An MP4 converted with audio from another source (owner, 17/11: only there;
 * any other conversion and the TV player's path as before). */
static int ext_mp4(const DtsEngine *e) {
  return e->conv_stream && !e->parent && e->ext_url && e->fmt && e->fmt->iformat && e->fmt->iformat->name &&
         strstr(e->fmt->iformat->name,"mp4");
}
/* Final addresses of another source's versions, remembered for the next
 * engine of the same version (owner, 14/11): a Torrentio link goes
 * torrentio.strem.fun -> api.torbox.app -> TorBox CDN, 1.2-2.4 s before the
 * first byte, at every reopen, swap back, reload or track probe. Straight to
 * the CDN instead; a remembered address that fails before its first byte goes
 * back through the add-on at once and is forgotten. Kept FINAL_CACHE_MS: TorBox
 * links lasted about an hour in 2024; today's servers are unknown, so 3 h and
 * the log says how old a remembered address was when it failed (owner, 14/11). */
#define FINAL_CACHE_N 16
#define FINAL_CACHE_MS (3u*60u*60u*1000u)
static struct { char *url, *fin; uint32_t hh; int cross; uint64_t at; } g_final[FINAL_CACHE_N];
static pthread_mutex_t g_final_lock=PTHREAD_MUTEX_INITIALIZER;
static uint64_t fx_now_ms(void);
static uint32_t final_hash(const char *h) { uint32_t x=2166136261u; for(;h&&*h;h++){ x^=(unsigned char)*h; x*=16777619u; } return x; }
static int final_slot(const char *url,uint32_t hh) {
  for(int i=0;i<FINAL_CACHE_N;i++) if(g_final[i].url && g_final[i].hh==hh && !strcmp(g_final[i].url,url)) return i;
  return -1;
}
static void final_cache_put(const char *url,const char *headers,const char *fin,int cross) {
  if(!url || !*url || !fin || !*fin || !strcmp(url,fin)) return;   /* no redirect: nothing to save */
  uint32_t hh=final_hash(headers);
  pthread_mutex_lock(&g_final_lock);
  int i=final_slot(url,hh);
  if(i<0) {   /* a free slot, else the oldest */
    i=0;
    for(int k=0;k<FINAL_CACHE_N;k++) { if(!g_final[k].url) { i=k; break; } if(g_final[k].at<g_final[i].at) i=k; }
    free(g_final[i].url); g_final[i].url=strdup(url); g_final[i].hh=hh;
  }
  free(g_final[i].fin); g_final[i].fin=strdup(fin); g_final[i].cross=cross; g_final[i].at=fx_now_ms();
  if(!g_final[i].url || !g_final[i].fin) { free(g_final[i].url); free(g_final[i].fin); g_final[i].url=g_final[i].fin=NULL; }
  pthread_mutex_unlock(&g_final_lock);
}
/* 1 and the address when one younger than FINAL_CACHE_MS is remembered. */
static int final_cache_get(const char *url,const char *headers,char fin[8192],int *cross,unsigned *age_s) {
  int ok=0;
  if(!url || !*url) return 0;
  pthread_mutex_lock(&g_final_lock);
  int i=final_slot(url,final_hash(headers));
  if(i>=0) {
    uint64_t age=fx_now_ms()-g_final[i].at;
    if(age<FINAL_CACHE_MS) { snprintf(fin,8192,"%s",g_final[i].fin); *cross=g_final[i].cross; *age_s=(unsigned)(age/1000); ok=1; }
    else { free(g_final[i].url); free(g_final[i].fin); g_final[i].url=g_final[i].fin=NULL; }
  }
  pthread_mutex_unlock(&g_final_lock);
  return ok;
}
static void final_cache_forget(const char *url,const char *headers) {
  if(!url) return;
  pthread_mutex_lock(&g_final_lock);
  int i=final_slot(url,final_hash(headers));
  if(i>=0) { free(g_final[i].url); free(g_final[i].fin); g_final[i].url=g_final[i].fin=NULL; }
  pthread_mutex_unlock(&g_final_lock);
}
/* An engine of another source's version starts at the remembered address. */
static void final_seed(DtsEngine *e) {
  char fin[8192]; int cross=0; unsigned age=0;
  if(!e->remember_final || !final_cache_get(e->url,e->headers,fin,&cross,&age)) return;
  pthread_mutex_lock(&e->range_lock);
  snprintf(e->final_url,sizeof e->final_url,"%s",fin); e->final_cross=cross; e->final_seeded=1; e->final_age_s=age;
  pthread_mutex_unlock(&e->range_lock);
  printf("[ext-audio] this version's server address remembered %u s ago: straight there, no add-on redirects\n",age);
  fflush(stdout);
}
/* Its request can be dropped at once (source_cancel), not only once a byte
 * arrives: the long rings and the other version's reader (owner's log 14/11:
 * a silent connection to it held the converter, picture included). */
static int fx_abortable(const DtsEngine *e) { return fx_long(e) || e->parent; }
/* Read-ahead allowed past the reader: twice what was read since the request
 * started, at least 2 MiB. A probe that only needs the header and the Cues
 * then costs a few MiB, not a full ring, at each jump. */
static int64_t fx_ahead(DtsEngine *e) {
  if(fx_long(e)) return FX_CAP-FX_HIST;
  int64_t used=e->pos>e->fx_from?e->pos-e->fx_from:0, a=2*used;
  if(a<2*1024*1024) a=2*1024*1024;
  if(a>FX_CAP-FX_HIST) a=FX_CAP-FX_HIST;
  return a;
}
/* Called under range_lock by the streaming thread: room for more bytes? */
static int64_t fx_room(DtsEngine *e) {
  int64_t floor=e->pos<e->fx_hi?e->pos:e->fx_hi;
  floor-=FX_HIST; if(floor<e->fx_lo) floor=e->fx_lo;
  int64_t room=FX_CAP-(e->fx_hi-e->fx_lo);
  if(!fx_long(e) || (room<=0 && floor>e->fx_lo)) {
    e->fx_lo=floor; // Reclaim only when overwriting; seek alone discards nothing.
    room=FX_CAP-(e->fx_hi-e->fx_lo);
  }
  int64_t ahead=fx_ahead(e)-(e->fx_hi-(e->pos>e->fx_hi?e->fx_hi:e->pos));
  if(e->pos>e->fx_hi) ahead=FX_CAP;                /* reader waits past the end: fill */
  return room<ahead?room:ahead;
}
static int fx_sink(void *opaque,const char *data,size_t n) {
  DtsEngine *e=opaque;
  pthread_mutex_lock(&e->range_lock);
  while(n>0) {
    if(e->fx_stop || cancelled(e) || e->fx_req>=0) { pthread_mutex_unlock(&e->range_lock); return 0; }
    if(e->total<0 && e->fx_live_total>=0) e->total=e->fx_live_total;   /* known from the headers */
    int64_t room=fx_room(e);
    if(room<=0) { pthread_cond_wait(&e->range_wake,&e->range_lock); continue; }
    size_t k=n<(size_t)room?n:(size_t)room;
    size_t at=(size_t)(e->fx_hi%FX_CAP), first=FX_CAP-at<k?FX_CAP-at:k;
    memcpy(e->fx_buf+at,data,first);
    if(k>first) memcpy(e->fx_buf,data+first,k-first);
    e->fx_hi+=(int64_t)k; data+=k; n-=k;
    e->metrics.range_bytes+=(uint64_t)k;
    if(!e->sg_first_ms) e->sg_first_ms=fx_now_ms();
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
    if(fx_abortable(e))__atomic_store_n(&e->source_cancel,0,__ATOMIC_RELAXED);
    e->sg_first_ms=0;   /* a new connection: the speed guard waits for its first byte */
    pthread_mutex_unlock(&e->range_lock);
    uint64_t t0=fx_now_ms();
    e->fx_live_total=-1;
    int r=rede_fluxo64(target,strip?(e->source_stream && e->final_cross!=2?e->source_public_headers:""):e->headers,at,fx_sink,e,&e->fx_live_total,&status,
      fx_abortable(e)?&e->source_cancel:&e->cancelled,fin,&cross,FX_STALL_MS,&got);
    uint64_t ms=fx_now_ms()-t0;
    pthread_mutex_lock(&e->range_lock);
    total=e->fx_live_total;
    e->metrics.range_requests++;
    if(total>=0) e->total=total;
    if(!direct && got>0 && fin[0]) {
      snprintf(e->final_url,sizeof e->final_url,"%s",fin); e->final_cross=cross;
      if(e->remember_final) final_cache_put(e->url,e->headers,fin,cross);
    }
    if(direct && got>0) e->final_seeded=0;   /* the remembered address answers */
    if(r==1 || e->fx_stop || cancelled(e) || (fx_abortable(e) && e->fx_req>=0)) continue; /* seek/closing */
    if(r!=0 && direct && e->final_seeded && got==0) {
      /* A remembered address that fails before its first byte (expired,
       * another server): through the add-on now, no retry schedule. */
      e->final_seeded=0; e->final_url[0]=0; e->fx_resolved_ms=fx_now_ms();
      final_cache_forget(e->url,e->headers);
      printf("[ext-audio] the remembered server address failed (remembered %u min ago, http=%d after %llu ms): through the add-on again\n",
             e->final_age_s/60,status,(unsigned long long)ms);
      fflush(stdout);
      continue;
    }
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
#define SP_SLOTS 12
#define SP_BUDGET (8LL * 1024 * 1024)
static int sp_find(const DtsEngine *e,int64_t pos) {
  int lo=0, hi=e->sp_n-1;
  while(lo<=hi) {
    int mid=(lo+hi)/2;
    if(pos<e->sp_pos[mid]) hi=mid-1;
    else if(pos>=e->sp_pos[mid]+e->sp_size[mid]) lo=mid+1;
    else return mid;
  }
  return -1;
}
/* One HTTP range of the donor, the final address once known (as the stream
 * reader does). NULL on failure; *got = bytes. */
static char *sp_fetch(DtsEngine *e,int64_t start,int64_t end,long *got) {
  char fin[8192]; int status=0, cross=0, direct; int64_t total=-1;
  char target[8192];
  pthread_mutex_lock(&e->range_lock);
  direct=e->final_url[0]!=0;
  snprintf(target,sizeof target,"%s",direct?e->final_url:e->url);
  int strip=direct && e->final_cross;
  pthread_mutex_unlock(&e->range_lock);
  *got=0;
  char *d=rede_baixar_trecho64_final(target,strip?"":e->headers,start,end,got,&total,&status,&e->cancelled,fin,&cross);
  pthread_mutex_lock(&e->range_lock);
  e->metrics.range_requests++;
  if(d) { e->metrics.range_bytes+=(uint64_t)*got; e->sp_bytes+=*got; }
  if(!direct && d && fin[0]) {
    snprintf(e->final_url,sizeof e->final_url,"%s",fin); e->final_cross=cross;
    if(e->remember_final) final_cache_put(e->url,e->headers,fin,cross);
  }
  if(direct && d) e->final_seeded=0;
  if(direct && !d && e->final_seeded && !cancelled(e)) {   /* remembered address gone: the add-on's next time */
    e->final_seeded=0; e->final_url[0]=0; final_cache_forget(e->url,e->headers);
  }
  pthread_mutex_unlock(&e->range_lock);
  if(!d && !cancelled(e)) {
    printf("[ext-audio] audio only: range %lld-%lld failed http=%d direct=%d\n",(long long)start,(long long)end,status,direct);
    fflush(stdout);
  }
  return d;
}
static void *sp_worker(void *opaque) {
  DtsEngine *e=opaque;
  pthread_mutex_lock(&e->range_lock);
  while(!e->sp_stop && !cancelled(e)) {
    int64_t held=0; int target=-1, slot=-1;
    for(int s=0;s<SP_SLOTS;s++) {
      int i=e->sp_slot[s].idx;
      if(e->sp_slot[s].state==2 && (i<e->sp_want || i>=e->sp_want+SP_SLOTS)) {
        free(e->sp_slot[s].data); e->sp_slot[s].data=NULL; e->sp_slot[s].state=0;
      }
      if(e->sp_slot[s].state) held+=e->sp_size[e->sp_slot[s].idx];
      else if(slot<0) slot=s;
    }
    for(int i=e->sp_want;i<e->sp_n && i<e->sp_want+SP_SLOTS && target<0;i++) {
      int have=0;
      for(int s=0;s<SP_SLOTS;s++) if(e->sp_slot[s].state && e->sp_slot[s].idx==i) have=1;
      if(!have) target=i;
    }
    if(target<0 || slot<0 || e->sp_err || (held && held+e->sp_size[target]>SP_BUDGET)) {
      pthread_cond_wait(&e->range_wake,&e->range_lock); continue;
    }
    e->sp_slot[slot].idx=target; e->sp_slot[slot].state=1;
    int64_t start=e->sp_pos[target], end=start+e->sp_size[target]-1;
    char *d=NULL; long got=0;
    for(int t=0;t<3 && !d && !e->sp_stop && !cancelled(e);t++) {
      pthread_mutex_unlock(&e->range_lock);
      d=sp_fetch(e,start,end,&got);
      if(d && got!=end-start+1) { free(d); d=NULL; }
      pthread_mutex_lock(&e->range_lock);
    }
    if(d) { e->sp_slot[slot].data=d; e->sp_slot[slot].state=2; }
    else { e->sp_slot[slot].state=0; if(!e->sp_stop && !cancelled(e)) e->sp_err=1; }
    pthread_cond_broadcast(&e->range_wake);
  }
  pthread_cond_broadcast(&e->range_wake);
  pthread_mutex_unlock(&e->range_lock);
  return NULL;
}
static void sp_close(DtsEngine *e) {
  pthread_mutex_lock(&e->range_lock);
  e->sp_stop=1; pthread_cond_broadcast(&e->range_wake);
  pthread_mutex_unlock(&e->range_lock);
  if(e->sp_thread_ok) pthread_join(e->sp_thread,NULL);
  for(int s=0;s<SP_SLOTS;s++) { free(e->sp_slot[s].data); e->sp_slot[s].data=NULL; e->sp_slot[s].state=0; }
  free(e->sp_pos); free(e->sp_size); e->sp_pos=NULL; e->sp_size=NULL;
  e->sp_n=e->sp_want=e->sp_err=e->sp_stop=e->sp_thread_ok=0;
}
/* AVIO read in audio-only mode: an audio chunk from its slot; anything else
 * (headers, the index, a probe) as a small direct range. */
static int read_sparse(DtsEngine *e,uint8_t *dst,int n) {
  pthread_mutex_lock(&e->range_lock);
  if(e->total>=0 && e->pos>=e->total) { pthread_mutex_unlock(&e->range_lock); return AVERROR_EOF; }
  int idx=sp_find(e,e->pos);
  if(idx<0) {
    int64_t at=e->pos, k=n>65536?65536:n;
    if(e->total>=0 && at+k>e->total) k=e->total-at;
    pthread_mutex_unlock(&e->range_lock);
    long got=0; char *d=sp_fetch(e,at,at+k-1,&got);
    if(!d || got<=0) { free(d); return cancelled(e)?AVERROR_EXIT:AVERROR(EIO); }
    memcpy(dst,d,(size_t)got); free(d);
    pthread_mutex_lock(&e->range_lock); e->pos=at+got; pthread_mutex_unlock(&e->range_lock);
    return (int)got;
  }
  if(!e->sp_thread_ok && !e->sp_stop) {
    if(pthread_create(&e->sp_thread,NULL,sp_worker,e)) { pthread_mutex_unlock(&e->range_lock); return AVERROR(ENOMEM); }
    e->sp_thread_ok=1;
  }
  if(e->sp_want!=idx) { e->sp_want=idx; pthread_cond_broadcast(&e->range_wake); }
  for(;;) {
    if(cancelled(e) || __atomic_load_n(&e->interrupt,__ATOMIC_RELAXED)) { pthread_mutex_unlock(&e->range_lock); return AVERROR_EXIT; }
    for(int s=0;s<SP_SLOTS;s++)
      if(e->sp_slot[s].state==2 && e->sp_slot[s].idx==idx) {
        int64_t off=e->pos-e->sp_pos[idx], k=e->sp_size[idx]-off;
        if(k>n) k=n;
        memcpy(dst,e->sp_slot[s].data+off,(size_t)k); e->pos+=k;
        pthread_mutex_unlock(&e->range_lock);
        return (int)k;
      }
    if(e->sp_err) { pthread_mutex_unlock(&e->range_lock); return AVERROR(EIO); }
    struct timespec ts; clock_gettime(CLOCK_REALTIME,&ts); ts.tv_sec+=1;
    pthread_cond_timedwait(&e->range_wake,&e->range_lock,&ts);
  }
}
static int mkv_cluster_in(const uint8_t *b,int n,int64_t at,int64_t *cpos,int64_t *ctc);
/* No index entry near the target (Matroska Cues not read yet): find a Cluster
 * at or before it from the file itself, 1 MiB probes placed by
 * interpolation between known (position, time) points. -1 not found. */
static int64_t pf_locate(DtsEngine *e) {
  int64_t lo_p=e->pf_p0, lo_t=e->pf_t0, hi_p=e->total, hi_t=e->pf_tend, best=-1;
  for(int tries=0;tries<6 && !__atomic_load_n(&e->pf_cancel,__ATOMIC_RELAXED);tries++) {
    if(hi_t<=lo_t || hi_p<=lo_p) break;
    int64_t p=lo_p+(int64_t)((double)(hi_p-lo_p)*(double)(e->pf_ts-lo_t)/(double)(hi_t-lo_t));
    if(p<lo_p) p=lo_p;
    if(p>hi_p-1024*1024) p=hi_p-1024*1024;
    long got=0; int64_t total=-1; int status=0, cross=0; char fin[8192];
    char *d=rede_baixar_trecho64_final(e->url,e->headers,p,p+1024*1024-1,&got,&total,&status,&e->pf_cancel,fin,&cross);
    int64_t cpos, ctc;
    int ok=d && got>0 && mkv_cluster_in((const uint8_t *)d,(int)got,p,&cpos,&ctc)==0;
    free(d);
    if(!ok) break;
    int64_t err=av_rescale_q(ctc-e->pf_ts,e->pf_tb,AV_TIME_BASE_Q);
    if(ctc<=e->pf_ts) { best=cpos; lo_p=cpos; lo_t=ctc; } else { hi_p=cpos; hi_t=ctc; }
    if(err<=0 && err>-8LL*AV_TIME_BASE) break;   /* a cluster up to 8 s before: start there */
  }
  return best;
}
static void *pf_worker(void *arg) {
  DtsEngine *e=arg;
  if(e->pf_lo<0) {
    int64_t at0=pf_locate(e);
    printf("[prefetch] %.1fs: cluster %s\n",e->pf_seconds,at0>=0?"found by position":"not found"); fflush(stdout);
    pthread_mutex_lock(&e->range_lock);
    if(at0>=0) e->pf_lo=e->pf_hi=at0;
    pthread_mutex_unlock(&e->range_lock);
    if(at0<0) { __atomic_store_n(&e->pf_done,1,__ATOMIC_RELAXED); return NULL; }
  }
  int64_t at=e->pf_lo, end=e->pf_lo+e->pf_cap;
  while(at<end && !__atomic_load_n(&e->pf_cancel,__ATOMIC_RELAXED) && !cancelled(e)) {
    long got=0; int64_t total=-1; int status=0, cross=0; char fin[8192];
    int64_t last=at+4*1024*1024-1; if(last>=end) last=end-1;
    char *d=rede_baixar_trecho64_final(e->url,e->headers,at,last,&got,&total,&status,&e->pf_cancel,fin,&cross);
    if(!d || got<=0) { free(d); break; }
    pthread_mutex_lock(&e->range_lock);
    memcpy(e->pf_buf+(at-e->pf_lo),d,(size_t)got); at+=got; e->pf_hi=at;
    pthread_mutex_unlock(&e->range_lock);
    free(d);
  }
  printf("[prefetch] %.1fs: %lld KiB ready\n",e->pf_seconds,(long long)((at-e->pf_lo)/1024)); fflush(stdout);
  __atomic_store_n(&e->pf_done,1,__ATOMIC_RELAXED);
  return NULL;
}
static void pf_stop(DtsEngine *e) {
  if(e->pf_thread_ok) { __atomic_store_n(&e->pf_cancel,1,__ATOMIC_RELAXED); pthread_join(e->pf_thread,NULL); e->pf_thread_ok=0; }
  pthread_mutex_lock(&e->range_lock);
  if(e->pf_buf) { printf("[prefetch] %.1fs: %lld KiB of it read\n",e->pf_seconds,(long long)(e->pf_served/1024)); fflush(stdout); }
  free(e->pf_buf); e->pf_buf=NULL; e->pf_lo=e->pf_hi=e->pf_cap=0; e->pf_used=0; e->pf_served=0;
  pthread_mutex_unlock(&e->range_lock);
}
/* A fresh connection from where the stream is (a slow one may be a slow
 * route: owner's log 07/11). Range readers are left as they are. */
void dts_engine_reconnect(DtsEngine *e) {
  if(!e || !fx_long(e)) return;
  pthread_mutex_lock(&e->range_lock);
  if(e->fx_thread_ok) {
    e->fx_req=e->fx_hi>e->pos?e->fx_hi:e->pos;   /* keep what already arrived */
    __atomic_store_n(&e->source_cancel,1,__ATOMIC_RELAXED);
    pthread_cond_broadcast(&e->range_wake);
  }
  pthread_mutex_unlock(&e->range_lock);
}
/* The prefetched bytes go once used and read past, or once playback itself
 * went beyond them (the RAM back). */
void dts_engine_prefetch_tidy(DtsEngine *e) {
  if(!e || !e->pf_buf || !__atomic_load_n(&e->pf_done,__ATOMIC_RELAXED)) return;
  int64_t pos; int used;
  pthread_mutex_lock(&e->range_lock); pos=e->pos; used=e->pf_used; pthread_mutex_unlock(&e->range_lock);
  if((used && pos>=e->pf_hi) || pos>=e->pf_lo+e->pf_cap) pf_stop(e);
}
int dts_engine_prefetch(DtsEngine *e,double seconds) {
  if(!e || !e->fmt || !fx_long(e) || e->info.video_stream<0 || !e->url || seconds<=0) return -1;
  if(e->pf_buf && fabs(seconds-e->pf_seconds)<1.0) return 0;
  AVStream *v=e->fmt->streams[e->info.video_stream];
  int64_t ts=av_rescale_q((int64_t)(seconds*AV_TIME_BASE),AV_TIME_BASE_Q,v->time_base);
  int i=av_index_search_timestamp(v,ts,AVSEEK_FLAG_BACKWARD);
  const AVIndexEntry *ie=i>=0?avformat_index_get_entry(v,i):NULL;
  /* Only a keyframe of the index near the target (a Matroska file whose Cues
   * were not read yet has a few early entries only). */
  int near=ie && ie->pos>0 && av_rescale_q(ts-ie->timestamp,v->time_base,AV_TIME_BASE_Q)<=15*AV_TIME_BASE;
  int mkv=e->fmt->iformat && strstr(e->fmt->iformat->name,"matroska");
  if(!near && (!mkv || e->total<=16*1024*1024 || e->fmt->duration<=0)) {
    printf("[prefetch] %.1fs: no index entry near it (%d entries), skipped\n",seconds,avformat_index_get_entries_count(v));
    fflush(stdout); return -1;
  }
  pf_stop(e);
  /* About 10 s of the file (8..24 MiB of RAM), from just before that keyframe. */
  int64_t rate=e->total>0 && e->fmt->duration>0?(int64_t)(e->total/(e->fmt->duration/1e6)):0;
  int64_t cap=rate*10; if(cap<8*1024*1024) cap=8*1024*1024; if(cap>24*1024*1024) cap=24*1024*1024;
  uint8_t *b=malloc((size_t)cap);
  if(!b) return -1;
  pthread_mutex_lock(&e->range_lock);
  e->pf_buf=b; e->pf_cap=cap; e->pf_seconds=seconds; e->pf_used=0;
  if(near) e->pf_lo=ie->pos>65536?ie->pos-65536:0;
  else {   /* found by the worker (pf_locate); reads skip the buffer until then */
    const AVIndexEntry *first=avformat_index_get_entry(v,0);
    e->pf_lo=-1; e->pf_ts=ts; e->pf_tb=v->time_base;
    e->pf_p0=first && first->pos>0?first->pos:0; e->pf_t0=first?first->timestamp:0;
    e->pf_tend=av_rescale_q(e->fmt->duration,AV_TIME_BASE_Q,v->time_base);
  }
  e->pf_hi=e->pf_lo;
  pthread_mutex_unlock(&e->range_lock);
  e->pf_cancel=0; e->pf_done=0;
  if(pthread_create(&e->pf_thread,NULL,pf_worker,e)==0) e->pf_thread_ok=1;
  else { pf_stop(e); return -1; }
  printf("[prefetch] %.1fs: fetching %lld KiB at %lld\n",seconds,(long long)(cap/1024),(long long)e->pf_lo); fflush(stdout);
  return 0;
}
/* Guards of a reader waiting on a slow source (owner's logs 14/11: TorBox CDN
 * connections that went quiet, or trickled at 15-25 KB/s while a fresh one
 * got MB/s at once). Two readers: the other version's (parent set), and this
 * source's in a conversion (one continuous stream; the TV player's own path
 * is not watched). Both reconnect at the reader's place, the second time
 * through the add-on (a fresh link), EXT_STALL_TRIES times per place; then the
 * other version gives up (the app goes back to the file's own audio), this
 * source just waits on (the rebuffer and fallback logic stays as it was). */
#ifndef EXT_STALL_S
#define EXT_STALL_S 8   /* tests build with shorter ones */
#endif
#ifndef SG_WINDOW_MS
#define SG_WINDOW_MS 4000
#endif
#define EXT_STALL_TRIES 3
static int guarded(const DtsEngine *e) { return e->parent || (e->conv_stream && !e->source_stream); }
/* Under range_lock. -1: the other version gives up. */
static int xs_reconnect(DtsEngine *e,const char *why) {
  const char *tag=e->parent?"ext-audio":"dts-guard", *who=e->parent?"the other version":"this source";
  if(e->xs_tries && llabs(e->pos-e->xs_at)>1024*1024) e->xs_tries=0;   /* another place: tries anew */
  if(e->xs_tries>=EXT_STALL_TRIES) {
    if(!e->parent) {
      if(!e->xs_quiet) {
        printf("[%s] %s %s at %lld MiB after %d reconnections: waiting on\n",tag,who,why,(long long)(e->pos>>20),EXT_STALL_TRIES);
        fflush(stdout); e->xs_quiet=1;
      }
      return 0;
    }
    e->ext_dead=1;
    printf("[%s] %s %s at %lld MiB after %d reconnections: giving up\n",tag,who,why,(long long)(e->pos>>20),EXT_STALL_TRIES);
    fflush(stdout);
    return -1;
  }
  e->xs_tries++; e->xs_at=e->pos; e->xs_quiet=0;
  int fresh=(e->xs_tries==2 || e->final_seeded) && e->final_url[0];
  if(fresh) {
    e->final_url[0]=0; e->final_seeded=0; e->fx_resolved_ms=fx_now_ms();
    if(e->remember_final) final_cache_forget(e->url,e->headers);   /* not this server again next time */
  }
  printf("[%s] %s %s at %lld MiB: reconnecting (%d/%d)%s\n",tag,who,why,(long long)(e->pos>>20),
         e->xs_tries,EXT_STALL_TRIES,fresh?", a fresh link from the add-on":"");
  fflush(stdout);
  e->fx_req=e->pos;
  __atomic_store_n(&e->source_cancel,1,__ATOMIC_RELAXED);
  pthread_cond_broadcast(&e->range_wake);
  return 0;
}
/* Silent: the reader has waited EXT_STALL_S s since *wait_started without
 * its byte. */
static int ext_stall_guard(DtsEngine *e,uint64_t *wait_started) {
  uint64_t now=clock_ns(CLOCK_MONOTONIC);
  if(e->fx_req>=0 || now<*wait_started || now-*wait_started<(uint64_t)EXT_STALL_S*1000000000u) return 0;
  char why[48]; snprintf(why,sizeof why,"silent %d s",EXT_STALL_S);
  *wait_started=now;   /* the new connection gets its own EXT_STALL_S */
  return xs_reconnect(e,why);
}
/* Bytes per second this file needs (size over length), 0 before it is open. */
static double sg_need(const DtsEngine *e) {
  return e->total>0 && e->fmt && e->fmt->duration>0 ? (double)e->total*AV_TIME_BASE/(double)e->fmt->duration : 0;
}
static void sg_window(DtsEngine *e,uint64_t now) {
  e->sg_win_ms=now; e->sg_win_hi=e->fx_hi; e->sg_blocked_ms=0;
  if(e->sg_wait_ms) e->sg_wait_ms=now;
}
/* Trickle: over SG_WINDOW_MS from the connection's first byte, the reader
 * waited most of the time and the connection brought less than a quarter of
 * what the file needs (128 KB/s before the file is open; 48..512 KB/s, so a
 * home line slower than a big file is not reconnected over and over). A
 * connection silent from its start is the other guard's. */
static int speed_guard(DtsEngine *e,uint64_t *wait_started) {
  uint64_t now=fx_now_ms();
  if(e->fx_req>=0 || !e->sg_first_ms) return 0;
  if(e->sg_win_ms<e->sg_first_ms) { sg_window(e,e->sg_first_ms<now?e->sg_first_ms:now); return 0; }
  uint64_t el=now-e->sg_win_ms;
  if(el<SG_WINDOW_MS) return 0;
  uint64_t from=e->sg_wait_ms>e->sg_win_ms?e->sg_wait_ms:e->sg_win_ms;
  uint64_t blocked=e->sg_blocked_ms+(e->sg_wait_ms && now>from?now-from:0);
  double rate=(double)(e->fx_hi-e->sg_win_hi)*1000.0/(double)el, need=sg_need(e);
  double thr=need>0?need/4:128*1024;
  if(thr<48*1024) thr=48*1024;
  if(thr>512*1024) thr=512*1024;   /* a trickle, not a line slower than a remux */
  int slow=blocked*10>=el*6 && rate<thr;
  sg_window(e,now);
  if(!slow) return 0;
  char why[64]; snprintf(why,sizeof why,"slow (%.0f KB/s, needs %.0f)",rate/1024,(need>0?need:thr*4)/1024);
  *wait_started=clock_ns(CLOCK_MONOTONIC);   /* the new connection gets its own EXT_STALL_S */
  return xs_reconnect(e,why);
}
/* The reader stops waiting (every return of read_fluxo_pending). */
static void xs_leave(DtsEngine *e) {
  e->xs_waiting=0;
  if(e->sg_wait_ms) {
    uint64_t now=fx_now_ms(), from=e->sg_wait_ms>e->sg_win_ms?e->sg_wait_ms:e->sg_win_ms;
    if(now>from) e->sg_blocked_ms+=now-from;
    e->sg_wait_ms=0;
  }
}
static int read_fluxo_pending(void *opaque,uint8_t *dst,int n,int pending_fd) {
  DtsEngine *e=opaque;
  uint64_t wait_started=0;
  if(e->sp_n) {
    int r=read_sparse(e,dst,n);
    if(r!=AVERROR(EIO)) return r;
    /* A chunk the server would not give (owner's log 07/11): the audio comes
     * from the whole file from here instead of failing the session. */
    printf("[ext-audio] audio only: a chunk failed at %lld, reading the whole file from here\n",(long long)e->pos);
    fflush(stdout);
    sp_close(e);
  }
  pthread_mutex_lock(&e->range_lock);
  if(!e->fx_thread_ok && !e->fx_stop) {
    if(!e->fx_buf) { if(!fx_long(e))e->fx_cap=(int64_t)(e->fx_mib>0?e->fx_mib:g_fx_cap_mib)*1024*1024; e->fx_buf=malloc((size_t)e->fx_cap); if(e->fx_buf) rings_add(e->fx_cap); }
    if(!e->fx_buf) { pthread_mutex_unlock(&e->range_lock); return AVERROR(ENOMEM); }
    e->fx_req=e->pos;
    if(pthread_create(&e->fx_thread,NULL,fx_worker,e)) { pthread_mutex_unlock(&e->range_lock); return AVERROR(ENOMEM); }
    e->fx_thread_ok=1;
  }
  for(;;) {
    struct pollfd pending={pending_fd,POLLIN,0};
    if(cancelled(e) || __atomic_load_n(&e->interrupt,__ATOMIC_RELAXED) ||
       (pending_fd>=0 && poll(&pending,1,0)>0)) { xs_leave(e); pthread_mutex_unlock(&e->range_lock); return AVERROR_EXIT; }
    if(e->total>=0 && e->pos>=e->total) { xs_leave(e); pthread_mutex_unlock(&e->range_lock); return AVERROR_EOF; }
    if(e->ext_dead && !(e->fx_req<0 && e->pos>=e->fx_lo && e->pos<e->fx_hi)) {   /* gave up: no new wait */
      xs_leave(e); pthread_mutex_unlock(&e->range_lock); return AVERROR(EIO);
    }
    if(e->pf_buf && e->pos>=e->pf_lo && e->pos<e->pf_hi && !(e->fx_req<0 && e->pos>=e->fx_lo && e->pos<e->fx_hi)) {
      xs_leave(e);
      int64_t k=e->pf_hi-e->pos; if(k>n) k=n;
      memcpy(dst,e->pf_buf+(e->pos-e->pf_lo),(size_t)k); e->pos+=k; e->pf_served+=k;
      pthread_mutex_unlock(&e->range_lock);
      return (int)k;
    }
    if(e->fx_req<0 && e->pos>=e->fx_lo && e->pos<e->fx_hi) {
      xs_leave(e);
      int64_t k=e->fx_hi-e->pos; if(k>n) k=n;
      size_t at=(size_t)(e->pos%FX_CAP); if((int64_t)(FX_CAP-at)<k) k=FX_CAP-at;
      memcpy(dst,e->fx_buf+at,(size_t)k); e->pos+=k;
      if(wait_started) { uint64_t w=clock_ns(CLOCK_MONOTONIC); if(w>=wait_started) e->metrics.range_blocked_ns+=w-wait_started; }
      pthread_cond_broadcast(&e->range_wake);         /* room for the stream */
      pthread_mutex_unlock(&e->range_lock);
      return (int)k;
    }
    if(e->fx_req<0 && e->fx_eof && e->pos>=e->fx_hi) { xs_leave(e); pthread_mutex_unlock(&e->range_lock); return AVERROR_EOF; }
    if(e->fx_req<0 && e->fx_err) {
      if(e->parent) e->ext_dead=1;   /* the stream's own retries ran out */
      xs_leave(e); pthread_mutex_unlock(&e->range_lock); return AVERROR(EIO);
    }
    /* Outside what the stream has or will soon have: start over there. */
    if(e->fx_req<0 && (e->pos<e->fx_lo || e->pos>e->fx_hi+(fx_long(e)?0:FX_SKIP))) {
      e->fx_req=e->pos;
      if(fx_abortable(e))__atomic_store_n(&e->source_cancel,1,__ATOMIC_RELAXED);
      pthread_cond_broadcast(&e->range_wake);
    }
    if(!wait_started) wait_started=clock_ns(CLOCK_MONOTONIC);
    if(guarded(e)) {
      if(!e->sg_wait_ms) e->sg_wait_ms=fx_now_ms();
      /* This source's first connection may wait on the add-on's redirects:
       * the silent guard starts once anything arrived. */
      int silent=e->parent || e->metrics.range_bytes>0;
      if((silent && ext_stall_guard(e,&wait_started)<0) || speed_guard(e,&wait_started)<0) {
        xs_leave(e); pthread_mutex_unlock(&e->range_lock); return AVERROR(EIO);
      }
    }
    e->xs_waiting=1;
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
  if(fx_long(e) && e->fx_thread_ok &&
     (e->pos<e->fx_lo || e->pos>e->fx_hi)) {
    /* A jump into the prefetched bytes: they serve it, the stream starts
     * again right after them. */
    int pf=e->pf_buf && e->pos>=e->pf_lo && e->pos<e->pf_hi && e->pf_hi-e->pos>=1024*1024;
    e->fx_req=pf?e->pf_hi:e->pos;
    if(pf && !e->pf_used) { e->pf_used=1; printf("[prefetch] jump to %.1fs served from %lld KiB fetched ahead\n",e->pf_seconds,(long long)((e->pf_hi-e->pos)/1024)); fflush(stdout); }
    __atomic_store_n(&e->source_cancel,1,__ATOMIC_RELAXED);
  }
  pthread_cond_broadcast(&e->range_wake);
  pthread_mutex_unlock(&e->range_lock);
  return e->pos;
}
static void fx_close(DtsEngine *e) {
  pthread_mutex_lock(&e->range_lock);
  e->fx_stop=1; pthread_cond_broadcast(&e->range_wake);
  if(fx_abortable(e))__atomic_store_n(&e->source_cancel,1,__ATOMIC_RELAXED);
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
/* Under range_lock. Only the actively awaited DV interval can escalate; a
 * paused consumer or an ordinary speculative download keeps its full budget.
 * Later payloads identify a hole, but do not prove the origin is fast. The
 * HTTP guard retains a progressing body instead of restarting every 3 s. */
static void range_wait_priority(DtsEngine *e,DtsRangeJob *job) {
#if defined(NV_NATIVE_SEEK_TEST) && defined(NV_DTS_WEBOS_RECOVERY)
  if(!e->dv || e->remux || !__atomic_load_n(&e->range_live,__ATOMIC_RELAXED) ||
     __atomic_load_n(&job->urgent,__ATOMIC_RELAXED)!=2)return;
  int later=0;int64_t bytes=0;
  for(int i=0;i<e->range_slots;i++) {
    DtsRangeJob *j=&e->ranges[i];
    if(j->start>job->end && j->state==3 && j->data && j->count>0){later++;bytes+=j->count;}
  }
  if(later>=2) {
    __atomic_store_n(&job->urgent,3,__ATOMIC_RELAXED);
    printf("[range-gap] start=%lld later=%d laterBytes=%lld budgetMs=3000\n",
      (long long)job->start,later,(long long)bytes);fflush(stdout);
  }
#else
  (void)e;(void)job;
#endif
}
static int read_range_pending(void *opaque,uint8_t *dst,int n,int pending_fd) {
  DtsEngine *e=opaque;
  if (cancelled(e) || __atomic_load_n(&e->interrupt,__ATOMIC_RELAXED)) return AVERROR_EXIT;
  if (e->total>=0 && e->pos>=e->total) return AVERROR_EOF;
  if (!e->cache || e->pos<e->cache_start || e->pos-e->cache_start>=e->cache_size) {
    uint64_t wait_started=clock_ns(CLOCK_MONOTONIC);
    if(!start_ranges(e)) return AVERROR(ENOMEM);
    pthread_mutex_lock(&e->range_lock);
    DtsRangeJob *job=NULL;
    for(int i=0;i<RANGE_SLOTS_MAX;i++)
      if(e->ranges[i].state && e->pos>=e->ranges[i].start &&
         e->pos-e->ranges[i].start<(e->ranges[i].state==3?e->ranges[i].count:e->ranges[i].end-e->ranges[i].start+1)) job=&e->ranges[i];
    if(!job) {
#ifdef NV_NATIVE_SEEK_TEST
      if(reuse_missing_range(e)) {
        queue_ranges(e);
        for(int i=0;i<e->range_slots;i++)if(e->ranges[i].state && e->ranges[i].start==e->pos)job=&e->ranges[i];
      }
      if(!job) {
#endif
      pthread_mutex_unlock(&e->range_lock);
      /* Seek or short 206: discard obsolete speculative ranges and restart
       * at the exact next byte. Never jump over a short response's tail. */
      clear_ranges(e,"missing");
      pthread_mutex_lock(&e->range_lock); queue_ranges(e);
      for(int i=0;i<RANGE_SLOTS_MAX;i++)
        if(e->ranges[i].state && e->pos>=e->ranges[i].start &&
         e->pos-e->ranges[i].start<(e->ranges[i].state==3?e->ranges[i].count:e->ranges[i].end-e->ranges[i].start+1)) job=&e->ranges[i];
#ifdef NV_NATIVE_SEEK_TEST
      }
#endif
    }
    if(!job) { pthread_mutex_unlock(&e->range_lock); return AVERROR_EXIT; }
    if(__atomic_load_n(&e->range_live,__ATOMIC_RELAXED)) {
      __atomic_store_n(&job->urgent,1,__ATOMIC_RELAXED);
#ifdef NV_NATIVE_SEEK_TEST
      /* A slow trickle can evade the idle guard and block DV behind one
       * missing range for the whole 15s transport timeout. Bound only the
       * actively awaited DV interval; retries keep every other cached slot. */
      if(e->dv && !e->remux) __atomic_store_n(&job->urgent,2,__ATOMIC_RELAXED);
#endif
    }
    int optional=optional_p2p_tail(e);
    struct timespec deadline;
    if(optional) { clock_gettime(CLOCK_REALTIME,&deadline); deadline.tv_sec++; }
    while(job->state!=3 && !cancelled(e) && !__atomic_load_n(&e->interrupt,__ATOMIC_RELAXED)) {
      if(pending_fd>=0) {
        struct pollfd pending={pending_fd,POLLIN,0};
        if(poll(&pending,1,0)>0) { pthread_mutex_unlock(&e->range_lock); return AVERROR_EXIT; }
      }
      range_wait_priority(e,job);
      if(optional) {
        if(pthread_cond_timedwait(&e->range_wake,&e->range_lock,&deadline)==ETIMEDOUT && job->state!=3) {
          pthread_mutex_unlock(&e->range_lock);
          /* Cancel speculative tail reads too, so the next seek cannot wait
           * for an abandoned request's 15-second transport timeout. */
          clear_ranges(e,"p2p-tail");
          return cancelled(e)?AVERROR_EXIT:AVERROR(EIO);
        }
      } else if(pending_fd>=0) {
        struct timespec tick;clock_gettime(CLOCK_REALTIME,&tick);tick.tv_nsec+=100000000;
        if(tick.tv_nsec>=1000000000){tick.tv_sec++;tick.tv_nsec-=1000000000;}
        pthread_cond_timedwait(&e->range_wake,&e->range_lock,&tick);
      } else pthread_cond_wait(&e->range_wake,&e->range_lock);
    }
    if(cancelled(e) || __atomic_load_n(&e->interrupt,__ATOMIC_RELAXED)) { pthread_mutex_unlock(&e->range_lock); return AVERROR_EXIT; }
    long count=job->count; int status=job->status; int64_t total=job->total, end=job->end, start=job->start;
    if(job->data && count>0 && e->pos-start>=count) {
      // A seek can enter a pending interval before the origin reveals its
      // smaller range cap. Its completed prefix does not cover that offset.
      if(count<e->range_bytes)e->range_bytes=count;
#ifdef NV_NATIVE_SEEK_TEST
      if(e->dv && !e->remux && __atomic_load_n(&e->range_live,__ATOMIC_RELAXED)) {
        free(job->data);job->data=NULL;job->state=0;
        pthread_mutex_unlock(&e->range_lock);return read_range_pending(e,dst,n,pending_fd);
      }
#endif
      pthread_mutex_unlock(&e->range_lock);
      clear_ranges(e,"seek-short");
      return read_range_pending(e,dst,n,pending_fd);
    }
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
    if(e->cache && count>0 && count<end-start+1 &&
        !(total>=0 && start<=total && count>=total-start)) {
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
      if (status==416 && total>=0 && e->pos>=total) { return AVERROR_EOF; }
      return AVERROR(EIO);
    }
    e->cache_start=start; e->cache_size=count;
  }
  int64_t available=e->cache_size-(e->pos-e->cache_start);
  if (n>available) n=(int)available;
  if(e->pos>INT64_MAX-n) return AVERROR(EOVERFLOW);
  memcpy(dst,e->cache+(size_t)(e->pos-e->cache_start),n); e->pos+=n;
  return n;
}
static int read_range(void *opaque,uint8_t *dst,int n) { return read_range_pending(opaque,dst,n,-1); }
static int64_t seek_range(void *opaque,int64_t offset,int whence) {
  DtsEngine *e=opaque;
  if (cancelled(e)) return AVERROR_EXIT;
  if (whence==AVSEEK_SIZE) return e->total>=0?e->total:AVERROR(ENOSYS);
  whence &= ~AVSEEK_FORCE;
  int64_t base=whence==SEEK_SET?0:whence==SEEK_CUR?e->pos:whence==SEEK_END?e->total:-1;
  if (base<0 || (offset>0 && base>INT64_MAX-offset) || (offset<0 && offset<-base)) return AVERROR(EINVAL);
  e->pos=base+offset;
  // Positioning alone is not a cache miss. FFmpeg probes end/index offsets
  // and can return without reading; evicting here threw away useful read-ahead.
  // read_range replaces the same bounded queue only if bytes are really needed.
  return e->pos;
}
static int64_t ns(int64_t value,AVRational tb) {
  return value==AV_NOPTS_VALUE?INT64_MIN:av_rescale_q(value,tb,NS);
}
#ifdef NV_DTS_PICTURE_DIAG
static void video_diag_reset(DtsEngine *e) {
  e->picture_epoch++;e->picture_count=e->demux_picture_count=0;
  memset(&e->demux_timing,0,sizeof e->demux_timing);
  memset(&e->output_timing,0,sizeof e->output_timing);
}
static void video_demux_diag(DtsEngine *e) {
  if(e->demux_picture_count>=120)return;
  AVPacket *p=e->in;AVStream *v=e->fmt->streams[e->info.video_stream];
  AVCodecParameters *c=v->codecpar;
  unsigned ordinal=++e->demux_picture_count;
  int emit=nv_video_picture_sample(ordinal);
  int64_t pts=ns(p->pts,v->time_base),dts=ns(p->dts,v->time_base),duration=ns(p->duration,v->time_base);
  nv_video_timing_log(&e->demux_timing,"demux",e->picture_epoch,pts,dts,duration,emit);
  if(!emit)return;
  size_t extra_size=0;const uint8_t *extra=av_packet_get_side_data(p,AV_PKT_DATA_NEW_EXTRADATA,&extra_size);
  printf("[video-demux] epoch=%u packet=%u stream=%d pos=%lld flags=%d tb=%d/%d extradataBytes=%d extradataHash=%08x newExtraBytes=%zu newExtraHash=%08x\n",
    e->picture_epoch,ordinal,p->stream_index,(long long)p->pos,p->flags,v->time_base.num,v->time_base.den,
    c->extradata_size,(unsigned)nv_video_picture_hash(c->extradata,c->extradata_size>0?(size_t)c->extradata_size:0),
    extra_size,(unsigned)nv_video_picture_hash(extra,extra_size));
  if(c->codec_id==AV_CODEC_ID_HEVC) {
    unsigned length=c->extradata && c->extradata_size>=23 && c->extradata[0]==1?(c->extradata[21]&3)+1:0;
    nv_hevc_diag_log("demux",e->picture_epoch,ordinal,p->data,p->size,length,pts,dts,duration,
      !!(p->flags&AV_PKT_FLAG_KEY),!!(p->flags&AV_PKT_FLAG_CORRUPT));
  }
  fflush(stdout);
}
#endif
static void copy_metadata(char *out,size_t size,AVDictionary *m,const char *key) {
  AVDictionaryEntry *v=av_dict_get(m,key,NULL,0);
  snprintf(out,size,"%s",v?v->value:"");
}
#ifdef NV_MP4_BD_TEST
#include "dts_mp4_dual.inc"
#endif
static int read_packet(DtsEngine *e) {
#ifdef NV_MP4_BD_TEST
  if(e->mp4)return mp4_read_packet(e);
#endif
  return av_read_frame(e->fmt,e->in);
}
static int packet_read_error(DtsEngine *e,int code) {
#ifdef NV_MP4_BD_TEST
  if(e->mp4 && e->error[0])return -1;
#endif
  return fail(e,"Container read",code);
}
static void match_disarm(DtsEngine *e);
static void quiet_free(DtsEngine *e);
static void quiet_reset(struct Quiet *q);
static void quiet_poll(DtsEngine *e);
static void match_cancel(DtsEngine *e);
static void match_stop(DtsEngine *e);
static void pf_stop(DtsEngine *e);
static void close_media(DtsEngine *e) {
#ifdef NV_HTTP_BUFFER_DIAG
  uint64_t httpCloseBeg=clock_ns(CLOCK_MONOTONIC);
  http_session_diag(e,"media-close-start",0);
#endif
  sp_close(e);
  pf_stop(e);
  fx_close(e);
  match_stop(e); e->mt_state=0;
  pthread_mutex_lock(&e->range_lock);
  DtsEngine *ext=e->ext; e->ext=NULL;
  pthread_mutex_unlock(&e->range_lock);
  if(ext) { __atomic_store_n(&ext->cancelled,1,__ATOMIC_RELAXED); dts_engine_destroy(ext); }
  av_packet_free(&e->ext_pkt); av_packet_free(&e->ext_in); e->ext_have=e->ext_eof=0; e->ext_stream=-1;
  e->ext_conv=e->dv_strip=0; e->ext_idx=0;
  pthread_mutex_lock(&e->range_lock);
  e->range_stop=1;
  for(int i=0;i<RANGE_SLOTS_MAX;i++) __atomic_store_n(&e->ranges[i].cancel,1,__ATOMIC_RELAXED);
  pthread_cond_broadcast(&e->range_wake);
  pthread_mutex_unlock(&e->range_lock);
  for(int i=0;i<e->range_threads;i++) pthread_join(e->workers[i].thread,NULL);
  for(int i=0;i<RANGE_SLOTS_MAX;i++) free(e->ranges[i].data);
  pthread_mutex_lock(&e->range_lock);
  memset(e->ranges,0,sizeof e->ranges); e->range_threads=0; e->range_stop=0;
  memset(e->rate_until_ns,0,sizeof e->rate_until_ns);
  memset(e->rate_strikes,0,sizeof e->rate_strikes);
  pthread_mutex_unlock(&e->range_lock);
  av_packet_free(&e->in); av_packet_free(&e->out);
  av_frame_free(&e->decoded); av_frame_free(&e->encoded);
  avcodec_free_context(&e->dec); avcodec_free_context(&e->enc);
  av_bsf_free(&e->bsf); swr_free(&e->swr); av_audio_fifo_free(e->fifo); e->fifo=NULL;
  nv_hevc_annexb_close(&e->hevc_annexb);
  av_freep(&e->dv7_buffer); e->dv7_capacity=0; e->dv7=0; e->mapping_reported=0;
#ifdef NV_MP4_BD_TEST
  e->mp4_p8_compat_verified=0;
  if(e->mp4) {
    for(int i=0;i<MP4_BASE_QUEUE;i++)av_packet_free(&e->mp4->base[i]);
    av_freep(&e->mp4);
  }
#endif
  av_channel_layout_uninit(&e->input_layout);
  avformat_close_input(&e->fmt);
  if (e->io) { av_freep(&e->io->buffer); avio_context_free(&e->io); }
  free(e->cache); e->cache=NULL; free(e->url); e->url=NULL;
  free(e->headers); e->headers=NULL;
  free(e->source_public_headers); e->source_public_headers=NULL;
#ifdef NV_HTTP_BUFFER_DIAG
  http_session_diag(e,"media-close-done",clock_ns(CLOCK_MONOTONIC)-httpCloseBeg);
#endif
}
DtsEngine *dts_engine_create(void) {
  DtsEngine *e=calloc(1,sizeof(*e));
  if(e) { e->http_workers=__atomic_load_n(&g_conversion_workers,__ATOMIC_RELAXED);
    if(e->http_workers>RANGE_WORKERS)e->http_workers=RANGE_WORKERS;
    e->total=-1; e->range_bytes=RANGE_BYTES; e->range_slots=RANGE_SLOTS; e->ext_stream=-1; e->mt_t1=-1; pthread_mutex_init(&e->range_lock,NULL); pthread_cond_init(&e->range_wake,NULL); }
#ifdef NV_HTTP_BUFFER_DIAG
  if(e) { e->diag_id=__atomic_add_fetch(&http_diag_next,1,__ATOMIC_RELAXED);
    http_session_diag(e,"create",0); }
#endif
  return e;
}
void dts_engine_buffer_info(DtsEngine *e,DtsBufferInfo *b) {
  if(!b)return;
  memset(b,0,sizeof *b);
  if(!e)return;
  pthread_mutex_lock(&e->range_lock);
  if(fx_long(e)) {
    b->http_workers=e->http_workers;
    b->position=e->pos;b->total=e->total;b->capacity_bytes=e->fx_buf?e->fx_cap:0;
    if(e->fx_req<0 && e->pos>=e->fx_lo && e->pos<=e->fx_hi)b->ready_bytes=e->fx_hi-e->pos;
    else if(e->pf_buf && e->pos>=e->pf_lo && e->pos<e->pf_hi) {
      /* A jump served from the prefetched bytes (owner's log 11/11: intro
       * skip, the stream restarts after them): they are ready too, and so
       * is what the stream already has right after them. Not counting them
       * left the rebuffer waiting on 0 KiB while playback stood still. */
      b->ready_bytes=e->pf_hi-e->pos;
      if(e->fx_req<0 && e->fx_lo<=e->pf_hi && e->fx_hi>e->pf_hi)b->ready_bytes+=e->fx_hi-e->pf_hi;
    }
    b->contiguous_bytes=b->ready_bytes;
  } else if(!e->remux && e->range_threads && !cancelled(e)) {
    b->http_workers=e->http_workers;
    // Called only by the conversion worker: it owns pos/cache. Completed
    // speculative ranges belong to the network workers and need range_lock.
    b->position=e->source_range?e->source_position:e->pos; b->total=e->total;
    b->capacity_bytes=(int64_t)(e->range_slots+1)*e->range_bytes;
    if(e->source_range)b->ready_bytes=e->source_available;
    else if(e->cache && e->pos>=e->cache_start && e->pos-e->cache_start<e->cache_size)
      b->ready_bytes=e->cache_size-(e->pos-e->cache_start);
    b->contiguous_bytes=b->ready_bytes;
    for(int i=0;i<e->range_slots;i++) {
      DtsRangeJob *j=&e->ranges[i];
      if(j->state==3 && j->data && j->count>0 && j->start>=b->position)
        b->ready_bytes+=j->count;
    }
    int64_t next=b->position+b->contiguous_bytes;
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
  e->http_workers=__atomic_load_n(&g_source_workers,__ATOMIC_RELAXED);
  if(e->http_workers>RANGE_WORKERS)e->http_workers=RANGE_WORKERS;
  e->source_stream=e->http_workers==1;e->source_range=!e->source_stream;
  http_session_diag(e,"native-configure",0);
  if(e->source_range) {
    e->range_bytes=RANGE_BYTES_DV;
    e->range_slots=(int)((int64_t)buffer_mib(mib)*1024*1024/e->range_bytes)-1;
    if(e->range_slots>=RANGE_SLOTS_MAX)e->range_slots=RANGE_SLOTS_MAX-1;
  }
  printf("[http-buffer] path=native connections=%d mode=%s bufferMiB=%d\n",e->http_workers,e->source_stream?"sequential":"ranges",buffer_mib(mib));fflush(stdout);
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
    int probe_n=e->source_range && offset==0 && got>0 ? (n<got?n:(int)got):0;
    if(probe_n)memcpy(data,probe,(size_t)probe_n);
    free(probe);
    pthread_mutex_lock(&e->range_lock);
    e->total=total;
    if(fin[0]) { snprintf(e->final_url,sizeof e->final_url,"%s",fin); e->final_cross=cross; }
    pthread_mutex_unlock(&e->range_lock);
    if(probe_n)return probe_n;
  }
  if(e->source_range) {
    if(seek_range(e,offset,SEEK_SET)<0)return -1;
    pthread_mutex_lock(&e->range_lock);e->source_position=offset;e->source_available=0;pthread_mutex_unlock(&e->range_lock);
    int got=read_range_pending(e,data,n,pending_fd);
    pthread_mutex_lock(&e->range_lock);
    e->source_position=e->pos;
    e->source_available=e->cache && e->pos>=e->cache_start && e->pos-e->cache_start<e->cache_size?e->cache_size-(e->pos-e->cache_start):0;
    pthread_mutex_unlock(&e->range_lock);
    return got;
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
void dts_engine_dv_audio(DtsEngine *e,int mode) {
#ifdef NV_DV_FIXED_AUDIO_TEST
  if(e) e->dv_audio=mode==1 || mode==2 ? mode : 0;
#else
  (void)e;(void)mode;
#endif
}
void dts_engine_allow_p7(DtsEngine *e, int enabled) { if (e) e->allow_p7 = !!enabled; }
void dts_engine_p7_mapping(DtsEngine *e,int enabled) {
#ifdef NV_DV7_MAPPING_TEST
  if(e)e->preserve_p7_mapping=!!enabled;
#else
  (void)e;(void)enabled;
#endif
}
void dts_engine_mp4_test(DtsEngine *e,int enabled) {
#ifdef NV_MP4_BD_TEST
  if(e)e->allow_mp4=!!enabled;
#else
  (void)e;(void)enabled;
#endif
}
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
  http_session_diag(e,"cancel",0);
  __atomic_store_n(&e->cancelled,1,__ATOMIC_RELAXED);
  pthread_mutex_lock(&e->range_lock);
  __atomic_store_n(&e->source_cancel,1,__ATOMIC_RELAXED);
  for(int i=0;i<RANGE_SLOTS_MAX;i++) __atomic_store_n(&e->ranges[i].cancel,1,__ATOMIC_RELAXED);
  if(e->ext) dts_engine_cancel(e->ext);
  pthread_cond_broadcast(&e->range_wake); pthread_mutex_unlock(&e->range_lock);
}
void dts_engine_destroy(DtsEngine *e) {
  if(e) { close_media(e); free(e->ext_url); free(e->ext_headers);
    pthread_cond_destroy(&e->range_wake); pthread_mutex_destroy(&e->range_lock); free(e); }
}
void dts_engine_external_audio(DtsEngine *e,const char *url,const char *headers,const char *langs,int offset_ms) {
  if(!e) return;
  free(e->ext_url); free(e->ext_headers); e->ext_url=e->ext_headers=NULL;
  if(url && *url) { e->ext_url=strdup(url); e->ext_headers=strdup(headers?headers:""); }
  snprintf(e->ext_langs,sizeof e->ext_langs,"%s",langs?langs:"");
  dts_engine_external_offset(e,offset_ms);
}
static void external_offset_set(DtsEngine *e,int offset_ms);
void dts_engine_external_offset(DtsEngine *e,int offset_ms) {
  if(!e) return;
  quiet_free(e);   /* set directly (a seek): nothing left to wait for */
  external_offset_set(e,offset_ms);
}
static void external_offset_set(DtsEngine *e,int offset_ms) {
  if(offset_ms>DTS_ENGINE_EXTERNAL_OFFSET_MAX_MS) offset_ms=DTS_ENGINE_EXTERNAL_OFFSET_MAX_MS;
  if(offset_ms<-DTS_ENGINE_EXTERNAL_OFFSET_MAX_MS) offset_ms=-DTS_ENGINE_EXTERNAL_OFFSET_MAX_MS;
  e->ext_offset_ns=(int64_t)offset_ms*1000000;
  /* A manual change (not the offset a match just gave) ends the second look. */
  if(e->mt_t1>=0 && llabs(e->ext_offset_ns-e->mt_o1_user_ns)>20000000) e->mt_t1=-1;
}
void dts_engine_external_speed(DtsEngine *e,int ratio_ppm) {
  if(!e) return;
  if(ratio_ppm>60000) ratio_ppm=60000;    /* PAL is +4.3 % */
  if(ratio_ppm<-60000) ratio_ppm=-60000;
  e->ext_ratio=ratio_ppm?1.0+ratio_ppm/1e6:0;
}
static double ext_k(const DtsEngine *e) { return e->ext_ratio>0?e->ext_ratio:1.0; }
/* A known film/TV speed ratio (24/23.976, 25/24, 25/23.976 and back). */
static int known_speed(double k) {
  static const double known[]={24000.0/23976.0,25.0/24.0,25000.0/23976.0};
  for(int i=0;i<3;i++)
    if(fabs(k-known[i])<0.15*(known[i]-1.0) || fabs(k-1.0/known[i])<0.15*(known[i]-1.0)) return 1;
  return 0;
}
int dts_engine_external_active(DtsEngine *e) { return e && e->ext_url && e->ext_headers; }
int dts_engine_external_stream(DtsEngine *e) { return e && e->ext_url && e->ext_headers && e->ext ? e->ext_stream : -1; }
/* Only the subtitle track shown is read (-1: none). An MP4 keeps each track's
 * samples in its own chunks: owner's log 17/11, a 4K Dolby Vision MP4 with 27
 * text tracks, the reader went back and forth up to 180 MiB between them and
 * the picture and the conversion ring started over each time. A track off
 * still advances in the demuxer without reading, so turned on again it
 * starts where the picture is. Only for an MP4 with audio from another
 * source (owner, 17/11). Called from the playback worker. */
void dts_engine_subtitle_only(DtsEngine *e,int stream) {
  if(!e || !ext_mp4(e)) return;
  int off=0;
  for(unsigned i=0;i<e->fmt->nb_streams;i++) {
    AVStream *s=e->fmt->streams[i];
    if(s->codecpar->codec_type!=AVMEDIA_TYPE_SUBTITLE) continue;
    if((int)i==stream) s->discard=AVDISCARD_DEFAULT;
    else { if(s->discard!=AVDISCARD_ALL) off++; s->discard=AVDISCARD_ALL; }
  }
  if(off) { printf("[ext-audio] MP4: %d subtitle track(s) not read, only the one shown (%d)\n",off,stream); fflush(stdout); }
}
void dts_engine_external_audio_only(DtsEngine *e,int on) { if(e) e->ext_audio_only=!!on; }
void dts_engine_external_track(DtsEngine *e,int s) { if(e) e->ext_force=s>=0?s+1:0; }
void dts_engine_external_track_hint(DtsEngine *e,int stream,const char *lang,const char *codec,int channels,const char *title) {
  if(!e) return;
  e->ext_hint_on=stream>=0 || (codec && *codec) || (title && *title);
  e->ext_hint_idx=stream; e->ext_hint_ch=channels;
  snprintf(e->ext_hint_lang,sizeof e->ext_hint_lang,"%s",lang?lang:"");
  snprintf(e->ext_hint_codec,sizeof e->ext_hint_codec,"%s",codec?codec:"");
  snprintf(e->ext_hint_title,sizeof e->ext_hint_title,"%s",title?title:"");
}
/* speed: another speed still uncorrected; ratio_ppm: the speed played now
 * (the app takes one the frame rates gave, so a match finding the same
 * speed reopens nothing). */
void dts_engine_external_mismatch(DtsEngine *e,int *cut_s,int *speed,int *ratio_ppm) {
  int on=e && e->ext;
  *cut_s=on?e->ext_cut_s:0; *speed=on && e->ext_speed && ext_k(e)==1.0;
  if(ratio_ppm) *ratio_ppm=on && e->ext_speed_auto?(int)llrint((ext_k(e)-1.0)*1e6):0;
}
static int configure_external(DtsEngine *e,double start);
int dts_engine_external_replace(DtsEngine *e,const char *url,const char *headers,const char *langs,
                                int offset_ms,int track,int audio_only,double target) {
  if(!e || !url || !*url) return -1;
  /* The new version's settings first: a fresh open after a failure uses them. */
  dts_engine_external_audio(e,url,headers,langs,offset_ms);
  dts_engine_external_track(e,track);
  dts_engine_external_audio_only(e,audio_only);
  if(!e->fmt || !e->ext) return -1;
  match_cancel(e); match_stop(e);
  /* The old version goes with everything that read or converted it; this
   * source's demux, index, read-ahead and video filter stay as they are. */
  pthread_mutex_lock(&e->range_lock);
  DtsEngine *old=e->ext; e->ext=NULL;
  pthread_mutex_unlock(&e->range_lock);
  __atomic_store_n(&old->cancelled,1,__ATOMIC_RELAXED); dts_engine_destroy(old);
  av_packet_free(&e->ext_pkt); av_packet_free(&e->ext_in);
  av_frame_free(&e->decoded); av_frame_free(&e->encoded);
  avcodec_free_context(&e->dec); avcodec_free_context(&e->enc);
  swr_free(&e->swr); av_audio_fifo_free(e->fifo); e->fifo=NULL;
  av_channel_layout_uninit(&e->input_layout);
  e->ext_have=e->ext_eof=e->ext_conv=0; e->ext_stream=-1; e->timeline=0; e->ext_idx=0;
  e->ext_demux_eof=e->ext_dec_eof=e->ext_swr_eof=e->ext_enc_eof=0;
  { int n=0;
    for(int i=0;i<e->info.n_tracks;i++)
      if(!(e->info.tracks[i].kind==DTS_AUDIO && e->info.tracks[i].stream_index>=DTS_ENGINE_EXTERNAL_STREAM))
        e->info.tracks[n++]=e->info.tracks[i];
    e->info.n_tracks=n; }
  printf("[ext-audio] swapping the other version; this source stays open, at %.1fs\n",target);
  fflush(stdout);
  if(configure_external(e,target>0?target:0)<0) return -1;
  return target>0?0:dts_engine_seek(e,0);
}
int64_t dts_engine_external_bytes(DtsEngine *e,int64_t *total) {
  DtsEngine *x=e?e->ext:NULL;
  if(total) *total=x?x->total:-1;
  return x?(int64_t)x->metrics.range_bytes:0;
}
/* Any thread: a seek, track change or other version is pending while the
 * worker waits for the other version's data (owner's log 14/11: the change
 * itself waited behind a silent connection). That read stops; the picture
 * goes on without the other audio until the worker's seek or swap. */
int dts_engine_external_wake(DtsEngine *e) {
  int woke=0;
  if(!e) return 0;
  pthread_mutex_lock(&e->range_lock);
  DtsEngine *x=e->ext;
  if(x && e->ext_stream>=0) {   /* not while it opens (a swap in progress) */
    pthread_mutex_lock(&x->range_lock);
    if(x->xs_waiting) {
      __atomic_store_n(&x->interrupt,1,__ATOMIC_RELAXED);
      __atomic_store_n(&x->source_cancel,1,__ATOMIC_RELAXED);
      pthread_cond_broadcast(&x->range_wake);
      woke=1;
    }
    pthread_mutex_unlock(&x->range_lock);
  }
  pthread_mutex_unlock(&e->range_lock);
  return woke;
}
int dts_engine_external_woken(DtsEngine *e) { return e && e->ext && (e->ext_woken || __atomic_load_n(&e->ext->interrupt,__ATOMIC_RELAXED)); }
int dts_engine_external_failed(DtsEngine *e) { return e && e->ext_failed; }
void dts_engine_forget_final(const char *url,const char *headers) { final_cache_forget(url,headers?headers:""); }
/* A read of the other version came back short: woken for a pending request
 * or given up (ext_stall_guard). 1 = so: no packet of it now, the picture
 * goes on; 0 = an ordinary end or error. */
static int ext_read_stopped(DtsEngine *e) {
  DtsEngine *x=e->ext;
  if(!x || cancelled(e)) return 0;
  if(x->ext_dead) {
    if(!e->ext_failed) {
      e->ext_failed=1;
      printf("[ext-audio] the other version does not respond: the picture goes on, back to this file's own audio\n");
      fflush(stdout);
    }
    return 1;
  }
  if(__atomic_load_n(&x->interrupt,__ATOMIC_RELAXED)) {
    if(!e->ext_woken) { e->ext_woken=1; printf("[ext-audio] wait for the other version stopped for a pending change\n"); fflush(stdout); }
    return 1;
  }
  return 0;
}
/* Before the other version reads again (its seek, a live shift). */
static void ext_unwake(DtsEngine *e) {
  if(!e->ext_woken && !(e->ext && __atomic_load_n(&e->ext->interrupt,__ATOMIC_RELAXED))) return;
  e->ext_woken=0;
  if(e->ext) dts_engine_interrupt(e->ext,0);
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
static int configure_audio(DtsEngine *e,int core,double start) {
  int r;
  AVCodecParameters *ap=e->fmt->streams[e->info.audio_stream]->codecpar;
  /* Pick once for this engine/session, not on the first track change. The
   * native decoder must see the same codec, channel layout and rate from its
   * initial Load onward. Reuse v12 Seek; never call changeAudioCodec. */
  int fixed=e->dv && e->dv_audio;
  /* DTS off and the TV decodes DTS (dts_off 2, tv.model.edidType): on the
   * Dolby Vision path the DTS stream goes to the TV untouched (experimental). */
  int dts_native=ap->codec_id==AV_CODEC_ID_DTS && e->dv && e->dts_off==2 && !fixed;
  e->passthrough=(ap->codec_id!=AV_CODEC_ID_DTS || dts_native) &&
    !(e->dv && nv_dts_truehd_codec(avcodec_get_name(ap->codec_id))) && !fixed;
  if(e->passthrough) {
    e->in=av_packet_alloc(); e->out=av_packet_alloc();
    if(!e->in || !e->out) return fail(e,"Audio queue allocation",0);
    e->info.channels=ap->ch_layout.nb_channels>0?ap->ch_layout.nb_channels:6;
    e->info.sample_rate=ap->sample_rate>0?ap->sample_rate:48000;
    if(dts_native) {
      /* The whole DTS stream as in the file (DTS-HD MA, DTS:X included), the
       * way the TV player sends it: C3 tester's soundbar showed plain DTS
       * with the core alone and DTS:X from the TV player (11/11). */
      if(e->info.channels>8) e->info.channels=8;
      printf("[dts] dv-dts-native: the TV decodes DTS, full stream passed through ch=%d rate=%d profile=%d\n",
        e->info.channels,e->info.sample_rate,ap->profile);
      fflush(stdout);
    }
    snprintf(e->info.audio_codec,sizeof(e->info.audio_codec),"%s",avcodec_get_name(ap->codec_id));
    e->info.duration_ns=e->fmt->duration==AV_NOPTS_VALUE?0:av_rescale_q(e->fmt->duration,AV_TIME_BASE_Q,NS);
    e->info.start_ns=e->fmt->start_time==AV_NOPTS_VALUE?0:av_rescale_q(e->fmt->start_time,AV_TIME_BASE_Q,NS);
    e->target_ns=(int64_t)(start*1000000000.0);
    return start>0?dts_engine_seek(e,start):0;
  }
  const AVCodec *decoder=avcodec_find_decoder(ap->codec_id);
  if(!decoder) return fail(e,"DTS decoder absent",0);
  e->dec=avcodec_alloc_context3(decoder);
  if(!e->dec) return fail(e,"DTS decoder allocation",0);
  avcodec_parameters_to_context(e->dec,ap);
  e->dec->pkt_timebase=e->fmt->streams[e->info.audio_stream]->time_base;
  /* Corrupt HD extensions must reach the worker error path; an explicit
   * core-only reopen decides whether to discard the failed extension. */
  e->dec->err_recognition=AV_EF_EXPLODE;
  if(core && ap->codec_id==AV_CODEC_ID_DTS && av_opt_set_int(e->dec->priv_data,"core_only",1,0)<0) return fail(e,"DTS core-only unavailable",0);
  if((r=avcodec_open2(e->dec,decoder,NULL))<0) return fail(e,"DTS decoder open",r);
  /* AC3 5.1 only when asked and the build has the encoder; else stereo AAC. */
  const AVCodec *encoder=(fixed ? e->dv_audio==2 : g_surround_ac3)?avcodec_find_encoder(AV_CODEC_ID_AC3):NULL;
  if(fixed && e->dv_audio==2 && !encoder) return fail(e,"Fixed DV AC3 encoder absent",0);
  int out_channels=2;
  if(encoder) {
    int in_channels=ap->ch_layout.nb_channels;
    out_channels=(fixed || in_channels==0 || in_channels>2)?6:2;
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
  if(fixed) {
    printf("[dts] dv-fixed-audio mode=%d input=%s output=%s channels=%d rate=%d\n",
      e->dv_audio,avcodec_get_name(ap->codec_id),e->info.audio_codec,e->info.channels,e->info.sample_rate);
    fflush(stdout);
  }
  if(nv_dts_truehd_codec(avcodec_get_name(ap->codec_id))) {
    printf("[dts] audio-convert input=truehd output=%s channels=%d rate=%d\n",
           e->info.audio_codec,e->info.channels,e->info.sample_rate);
    fflush(stdout);
  }
  e->info.duration_ns=e->fmt->duration==AV_NOPTS_VALUE?0:av_rescale_q(e->fmt->duration,AV_TIME_BASE_Q,NS);
  e->info.start_ns=e->fmt->start_time==AV_NOPTS_VALUE?0:av_rescale_q(e->fmt->start_time,AV_TIME_BASE_Q,NS);
  e->target_ns=(int64_t)(start*1000000000.0);
  return start>0?dts_engine_seek(e,start):0;
}
/* External audio: which tracks of the other file the TV takes as they are.
 * The native Load accepts AC-3/E-AC-3 (any layout up to 7.1) and raw stereo
 * 48 kHz AAC-LC, the same streams the converter itself produces. */
static int ext_usable(const AVCodecParameters *p) {
  if(p->codec_type!=AVMEDIA_TYPE_AUDIO) return 0;
  if(p->codec_id==AV_CODEC_ID_AC3 || p->codec_id==AV_CODEC_ID_EAC3)
    return p->ch_layout.nb_channels<=8;
  return p->codec_id==AV_CODEC_ID_AAC && p->ch_layout.nb_channels==2 && p->sample_rate==48000 &&
         (p->profile==AV_PROFILE_AAC_LOW || p->profile==AV_PROFILE_UNKNOWN);
}
static int ext_lang_is(const char *code,const char *pref) {
  if(!code || !*code || !pref || !*pref) return 0;
  return g_lang_match?g_lang_match(code,pref):!strcasecmp(code,pref);
}
/* Any other audio this build can decode is converted like a DTS track of the
 * main file (AC-3 5.1 or AAC stereo, Settings > DTS); DTS itself only while
 * DTS conversion is not off. */
static int ext_convertible(const DtsEngine *e,const AVCodecParameters *p) {
  if(p->codec_type!=AVMEDIA_TYPE_AUDIO || !avcodec_find_decoder(p->codec_id)) return 0;
  if(p->codec_id==AV_CODEC_ID_DTS && e->dts_off) return 0;
  return 1;
}
/* 2 = passes through, 1 = needs conversion, 0 = unusable. */
static int ext_rank(const DtsEngine *e,const AVCodecParameters *p) {
  return ext_usable(p)?2:ext_convertible(e,p)?1:0;
}
/* The audio tracks of a version, for the hold-OK menu: opens only its
 * container header (a few MiB) on a throwaway engine. */
int dts_engine_probe_audio(const char *url,const char *headers,DtsTrack *out,int max) {
  DtsEngine *x=dts_engine_create();
  int n=-1;
  if(!x) return -1;
  x->fx_mib=DTS_ENGINE_EXTERNAL_RING_MIB; x->remember_final=1;
  if(dts_engine_open_demux(x,url,headers)==0) {
    n=0;
    /* Under two minutes: no version of a film or an episode, the debrid
     * service's placeholder clip (owner's log 13/11: a 30 s AAC clip served
     * for a TorBox file not ready yet). */
    if(x->fmt->duration>0 && x->fmt->duration<120LL*AV_TIME_BASE) {
      printf("[ext-audio] tracks: %.0f s file, the debrid placeholder clip\n",x->fmt->duration/1e6);
      fflush(stdout);
      final_cache_forget(url,headers?headers:"");   /* the clip's address: through the add-on next time */
      dts_engine_destroy(x);
      return -3;
    }
    for(unsigned i=0;i<x->fmt->nb_streams && n<max;i++) {
      AVStream *st=x->fmt->streams[i]; AVCodecParameters *p=st->codecpar;
      if(ext_rank(x,p)<=0) continue;
      DtsTrack *t=&out[n++];
      memset(t,0,sizeof *t);
      t->kind=DTS_AUDIO; t->stream_index=(int)i; t->stream_id=st->id;
      t->channels=p->ch_layout.nb_channels;
      snprintf(t->codec,sizeof t->codec,"%s",avcodec_get_name(p->codec_id));
      copy_metadata(t->language,sizeof t->language,st->metadata,"language");
      copy_metadata(t->title,sizeof t->title,st->metadata,"title");
    }
  }
  dts_engine_destroy(x);
  return n;
}
/* In the first preferred language that has one: a track that passes through,
 * else one to convert; without a language match the same over all tracks.
 * -1 when the file has none. */
static int ext_pick(DtsEngine *e,AVFormatContext *fmt) {
  char langs[64], *save=NULL;
  snprintf(langs,sizeof langs,"%s",e->ext_langs);
  for(char *l=strtok_r(langs,",",&save);l;l=strtok_r(NULL,",",&save)) {
    for(int want=2;want>=1;want--)
      for(unsigned i=0;i<fmt->nb_streams;i++) {
        AVDictionaryEntry *t=av_dict_get(fmt->streams[i]->metadata,"language",NULL,0);
        if(ext_rank(e,fmt->streams[i]->codecpar)==want && t && ext_lang_is(t->value,l)) return (int)i;
      }
  }
  for(int want=2;want>=1;want--)
    for(unsigned i=0;i<fmt->nb_streams;i++) if(ext_rank(e,fmt->streams[i]->codecpar)==want) return (int)i;
  return -1;
}
/* The remembered track in this episode's version: the same language (when
 * both say one), then the file's track name, codec, channels and number.
 * Two English tracks (main and commentary) are told apart by the name, or by
 * the number in the same release. -1: nothing like it (by language then). */
static int ext_hint_pick(DtsEngine *e,AVFormatContext *fmt) {
  int best=-1, bs=2;
  for(unsigned i=0;i<fmt->nb_streams;i++) {
    AVStream *st=fmt->streams[i];
    char l[16]="", t[64]="";
    int s=0;
    if(!ext_rank(e,st->codecpar)) continue;
    copy_metadata(l,sizeof l,st->metadata,"language");
    copy_metadata(t,sizeof t,st->metadata,"title");
    if(e->ext_hint_lang[0] && l[0]) {
      const char *h=e->ext_hint_lang;
      /* "en"/"eng", "pl"/"pol": the app's matcher, or the same first letters */
      int same=!strcasecmp(l,h) || ext_lang_is(l,h) || ext_lang_is(h,l) ||
               ((strlen(l)==2)!=(strlen(h)==2) && !strncasecmp(l,h,2));
      if(!same) continue;
      s+=1;
    }
    if(e->ext_hint_title[0] && t[0] && !strcasecmp(t,e->ext_hint_title)) s+=4;
    if(e->ext_hint_codec[0] && !strcasecmp(avcodec_get_name(st->codecpar->codec_id),e->ext_hint_codec)) s+=2;
    if(e->ext_hint_ch>0 && st->codecpar->ch_layout.nb_channels==e->ext_hint_ch) s+=1;
    if((int)i==e->ext_hint_idx) s+=1;
    if(s>bs) { bs=s; best=(int)i; }
  }
  return best;
}
/* Decoder and encoder for a donor track that needs conversion. */
static int ext_configure_conv(DtsEngine *e,AVStream *as) {
  AVCodecParameters *ap=as->codecpar;
  const AVCodec *decoder=avcodec_find_decoder(ap->codec_id);
  int r;
  if(!decoder) return fail(e,"External audio: decoder absent",0);
  e->dec=avcodec_alloc_context3(decoder);
  if(!e->dec) return fail(e,"External audio: decoder allocation",0);
  avcodec_parameters_to_context(e->dec,ap);
  e->dec->pkt_timebase=as->time_base;
  /* The core is enough for a lossy output and costs far less CPU on a TV. */
  if(ap->codec_id==AV_CODEC_ID_DTS) av_opt_set_int(e->dec->priv_data,"core_only",1,0);
  if((r=avcodec_open2(e->dec,decoder,NULL))<0) return fail(e,"External audio: decoder open",r);
  const AVCodec *encoder=g_surround_ac3?avcodec_find_encoder(AV_CODEC_ID_AC3):NULL;
  int out_channels=2;
  if(encoder) { int in=ap->ch_layout.nb_channels; out_channels=(in==0 || in>2)?6:2; }
  else encoder=avcodec_find_encoder(AV_CODEC_ID_AAC);
  if(!encoder) return fail(e,"External audio: encoder absent",0);
  e->enc=avcodec_alloc_context3(encoder);
  if(!e->enc) return fail(e,"External audio: encoder allocation",0);
  e->enc->codec_id=encoder->id;
  av_channel_layout_default(&e->enc->ch_layout,out_channels);
  if(reset_encoder(e)<0) return -1;
  e->fifo=av_audio_fifo_alloc(e->enc->sample_fmt,e->enc->ch_layout.nb_channels,4096);
  e->decoded=av_frame_alloc(); e->encoded=av_frame_alloc(); e->ext_in=av_packet_alloc();
  if(!e->fifo || !e->decoded || !e->encoded || !e->ext_in) return fail(e,"Audio queue allocation",0);
  e->ext_conv=1; e->timeline=0;
  e->ext_demux_eof=e->ext_dec_eof=e->ext_swr_eof=e->ext_enc_eof=0;
  return 0;
}
/* Settings > "Audio only": an MP4 donor's index says where every audio
 * sample is, so only those chunks are fetched. Matroska has no such index
 * (its blocks are found by reading the clusters): the whole file, as before.
 * Too many small chunks (more than 4 per second) would mean as many HTTP
 * requests: the whole file too. */
static void ext_audio_only_start(DtsEngine *x,AVStream *as) {
  const char *name=x->fmt->iformat?x->fmt->iformat->name:"";
  int n=avformat_index_get_entries_count(as), m=0;
  double seconds=x->fmt->duration>0?x->fmt->duration/1e6:0;
  int64_t audio=0;
  if(!strstr(name,"mp4") && !strstr(name,"mov")) {
    printf("[ext-audio] audio only: %s file, reading the whole file\n",name); fflush(stdout); return;
  }
  if(n<=0 || seconds<=0) { printf("[ext-audio] audio only: no index, reading the whole file\n"); fflush(stdout); return; }
  int64_t *pos=malloc((size_t)n*sizeof *pos); int *size=malloc((size_t)n*sizeof *size);
  if(!pos || !size) { free(pos); free(size); return; }
  for(int i=0;i<n;i++) {
    const AVIndexEntry *ie=avformat_index_get_entry(as,i);
    if(!ie || ie->size<=0) continue;
    /* Contiguous samples are one chunk; a gap under 64 KiB is fetched with
     * them, cheaper than one more request. */
    if(m && ie->pos>=pos[m-1]+size[m-1] && ie->pos-(pos[m-1]+size[m-1])<=65536 &&
       ie->pos+ie->size-pos[m-1]<16*1024*1024) size[m-1]=(int)(ie->pos+ie->size-pos[m-1]);
    else { pos[m]=ie->pos; size[m]=ie->size; m++; }
    audio+=ie->size;
  }
  for(int i=1;i<m;i++) if(pos[i]<pos[i-1]) { m=0; break; }   /* chunks out of file order */
  if(m<=0 || m>seconds*4) {
    printf("[ext-audio] audio only: %d chunks in %.0f s, reading the whole file\n",m,seconds); fflush(stdout);
    free(pos); free(size); return;
  }
  fx_close(x);   /* stop the stream that read the header */
  pthread_mutex_lock(&x->range_lock);
  x->sp_pos=pos; x->sp_size=size; x->sp_n=m; x->sp_want=0; x->sp_err=0;
  pthread_mutex_unlock(&x->range_lock);
  printf("[ext-audio] audio only: %d chunks, %.1f MiB of audio in a %.1f MiB file\n",m,audio/1048576.0,
    x->total>0?x->total/1048576.0:0);
  fflush(stdout);
}
static double ext_fps(const AVStream *s) {
  AVRational r=s->avg_frame_rate.num>0?s->avg_frame_rate:s->r_frame_rate;
  return r.num>0 && r.den>0?(double)r.num/r.den:0;
}
static int configure_external(DtsEngine *e,double start) {
  DtsEngine *x=dts_engine_create();
  if(!x) return fail(e,"External audio allocation",0);
  x->fx_mib=g_ext_ring_mib; x->parent=e; x->remember_final=1;
  e->ext_woken=e->ext_failed=0;
  pthread_mutex_lock(&e->range_lock); e->ext=x; pthread_mutex_unlock(&e->range_lock);
  if(cancelled(e)) return fail(e,"Cancelled",0);
  printf("[ext-audio] opening second source ringMiB=%d offsetMs=%lld langs=%s\n",
    x->fx_mib,(long long)(e->ext_offset_ns/1000000),e->ext_langs[0]?e->ext_langs:"-");
  fflush(stdout);
  if(dts_engine_open_demux(x,e->ext_url,e->ext_headers)<0) {
    snprintf(e->error,sizeof e->error,"External audio: %.120s",x->error[0]?x->error:"open failed");
    return -1;
  }
  int k=ext_pick(e,x->fmt);
  if(!e->ext_force && e->ext_hint_on) {
    int h=ext_hint_pick(e,x->fmt);
    printf("[ext-audio] series memory: the track like the one chosen (%s %s %dch \"%.40s\", was %d): %d\n",
           e->ext_hint_lang,e->ext_hint_codec,e->ext_hint_ch,e->ext_hint_title,e->ext_hint_idx,h);
    if(h>=0) k=h;
  }
  if(e->ext_force>0 && (unsigned)(e->ext_force-1)<x->fmt->nb_streams &&
     ext_rank(e,x->fmt->streams[e->ext_force-1]->codecpar)>0) k=e->ext_force-1;
  for(unsigned i=0;i<x->fmt->nb_streams;i++) {
    AVStream *st=x->fmt->streams[i]; AVCodecParameters *p=st->codecpar;
    AVDictionaryEntry *l=av_dict_get(st->metadata,"language",NULL,0);
    printf("[ext-audio] track %u type=%s codec=%s ch=%d rate=%d lang=%s fps=%.3f%s\n",i,
      av_get_media_type_string(p->codec_type)?av_get_media_type_string(p->codec_type):"?",
      avcodec_get_name(p->codec_id),p->ch_layout.nb_channels,p->sample_rate,l?l->value:"",ext_fps(st),
      (int)i==k?" <- chosen":"");
    /* Only the chosen stream leaves the demuxer; the bytes of the others
     * are still read (they are interleaved in the same clusters). */
    if((int)i!=k) st->discard=AVDISCARD_ALL;
  }
  fflush(stdout);
  if(k<0) {
    int audio=0;
    for(unsigned i=0;i<x->fmt->nb_streams;i++) audio+=x->fmt->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO;
    /* No audio at all, far shorter than the picture: the debrid service's
     * placeholder clip for a file it does not have ready (owner's log 07/11:
     * one 1 fps H.264 track). */
    if(!audio && x->fmt->duration<e->fmt->duration/2) {
      final_cache_forget(x->url,x->headers);
      return fail(e,"External audio: this version is not ready on the debrid service yet (placeholder clip)",0);
    }
    return fail(e,audio?"External audio: no audio track this TV can play or convert":
                        "External audio: the other version has no audio track",0);
  }
  if(e->ext_audio_only) ext_audio_only_start(x,x->fmt->streams[k]);
  { /* A different frame rate (25 vs 23.976) means a sped-up version: the
     * audio would drift away; log it, the TV test decides what to refuse. */
    double fa=e->info.fps_den?(double)e->info.fps_num/e->info.fps_den:0, fb=0;
    for(unsigned i=0;i<x->fmt->nb_streams;i++)
      if(x->fmt->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO) { fb=ext_fps(x->fmt->streams[i]); break; }
    /* For the player's warning: another cut (owner's logs 07/11: a version
     * 25.7 s longer, another 100 s shorter) or another speed. */
    e->ext_speed=fa>0 && fb>0 && fabs(fa-fb)/fa>0.001;
    e->ext_fps_ratio=fa>0 && fb>0?fb/fa:0;
    /* No speed asked and a known film/TV ratio that the lengths confirm
     * (owner's log 14/11: a French 25 fps version of a 23.976 picture,
     * 2508.9 s for 2616.0 s): played at that speed from the start, so the
     * match only has the offset left to find, no reopening for the speed. */
    e->ext_speed_auto=0;
    if(e->ext_ratio<=0 && e->ext_speed && known_speed(e->ext_fps_ratio) &&
       x->fmt->duration>0 && e->fmt->duration>0 &&
       fabs((double)x->fmt->duration*e->ext_fps_ratio/(double)e->fmt->duration-1.0)<0.01) {
      e->ext_ratio=e->ext_fps_ratio; e->ext_speed_auto=1;
      printf("[ext-audio] other version at another speed (frame rates %.3f / %.3f, the lengths agree): played at ratio %.6f\n",
             fb,fa,e->ext_ratio);
    }
    /* In this source's time: a sped-up version is not another cut. */
    e->ext_cut_s=(int)llround(((double)x->fmt->duration*ext_k(e)-(double)e->fmt->duration)/1e6);
    /* Lengths half a second or more apart, the same cut otherwise: the gaps
     * differ somewhere, so the sync is checked all through. */
    e->mt_watch=e->mt_same=e->mt_stepped=0;
    if(x->fmt->duration>0 && e->fmt->duration>0) {
      double d=((double)x->fmt->duration*ext_k(e)-(double)e->fmt->duration)/1e6;
      if(fabs(d)>=0.5 && fabs(d)<=10.0) {
        e->mt_watch=1;
        printf("[ext-audio] the lengths differ by %.1f s: the sync is checked all through\n",d);
      }
    }
    printf("[ext-audio] main fps=%.3f duration=%.1fs, other fps=%.3f duration=%.1fs%s\n",fa,
      e->fmt->duration/1e6,fb,x->fmt->duration/1e6,
      fa>0 && fb>0 && fabs(fa-fb)>0.05?" WARNING: frame rates differ, audio will drift":"");
    fflush(stdout);
  }
  /* The same, the other way round: THIS source is the short clip (owner's log
   * 07/11: a 30 s, 7.9 MB MP4 served for a 4.3 GB AVI). */
  if(e->fmt->duration<120LL*AV_TIME_BASE && x->fmt->duration>600LL*AV_TIME_BASE &&
     x->fmt->duration>e->fmt->duration*10)
    return fail(e,"This source is a placeholder clip from the debrid service (not cached yet): pick another source for the picture",0);
  /* A debrid file that is not cached answers with a short warning clip that
   * plays normally: far shorter than the picture, it is not this title. */
  /* Under two minutes for a picture of ten or more: the same placeholder
   * clip, with an audio track (owner's log 13/11: 30 s of AAC, 25 fps, for a
   * TorBox file not ready yet, even one marked cached). */
  /* A short clip's address is not remembered (owner's log 14/11: the three
   * retries of a placeholder went straight back to the clip, never asking
   * the add-on whether the file was ready). */
  if(x->fmt->duration<e->fmt->duration/2) final_cache_forget(x->url,x->headers);
  if(x->fmt->duration>0 && x->fmt->duration<120LL*AV_TIME_BASE && e->fmt->duration>600LL*AV_TIME_BASE)
    return fail(e,"External audio: this version is not ready on the debrid service yet (placeholder clip)",0);
  if(x->fmt->duration<e->fmt->duration/2) {
    snprintf(e->error,sizeof e->error,"External audio: the other version lasts %.0f s, this one %.0f s (not cached?)",
      x->fmt->duration/1e6,e->fmt->duration/1e6);
    return -1;
  }
  e->ext_base_ns=(e->fmt->start_time!=AV_NOPTS_VALUE?av_rescale_q(e->fmt->start_time,AV_TIME_BASE_Q,NS):0)
                 -(x->fmt->start_time!=AV_NOPTS_VALUE?av_rescale_q(x->fmt->start_time,AV_TIME_BASE_Q,NS):0);
  AVStream *as=x->fmt->streams[k]; AVCodecParameters *ap=as->codecpar;
  e->ext_stream=k; e->ext_have=e->ext_eof=0; e->ext_clock_ns=INT64_MIN;
  e->ext_last_end_ns=e->ext_floor_ns=INT64_MIN;
  /* A swap of the other version keeps this source's packets (see
   * dts_engine_external_replace). */
  if(!e->ext_pkt) e->ext_pkt=av_packet_alloc();
  if(!e->in) e->in=av_packet_alloc();
  if(!e->out) e->out=av_packet_alloc();
  if(!e->ext_pkt || !e->in || !e->out) return fail(e,"Audio queue allocation",0);
  e->passthrough=1;   /* the main file: demux only, its audio unused */
  e->info.audio_stream=DTS_ENGINE_EXTERNAL_STREAM;
  /* Following another cut switches the encoder to this file's audio in the
   * scenes the other lacks: the conversion path. */
  int follow=e->fw_on && abs(e->ext_cut_s)>10;
  if(!ext_usable(ap) || ext_k(e)!=1.0 || follow) {
    if(ext_configure_conv(e,as)<0) return -1;
    if(ext_k(e)!=1.0) { printf("[ext-audio] other version's speed corrected: ratio %.6f\n",ext_k(e)); fflush(stdout); }
    e->info.channels=e->enc->ch_layout.nb_channels; e->info.sample_rate=48000;
    snprintf(e->info.audio_codec,sizeof e->info.audio_codec,"%s",avcodec_get_name(e->enc->codec_id));
    printf("[ext-audio] converting %s %d ch -> %s %d ch\n",avcodec_get_name(ap->codec_id),
      ap->ch_layout.nb_channels,e->info.audio_codec,e->info.channels);
    fflush(stdout);
  } else {
    e->info.channels=ap->ch_layout.nb_channels>0?ap->ch_layout.nb_channels:(ap->codec_id==AV_CODEC_ID_AAC?2:6);
    e->info.sample_rate=ap->sample_rate>0?ap->sample_rate:48000;
    snprintf(e->info.audio_codec,sizeof e->info.audio_codec,"%s",avcodec_get_name(ap->codec_id));
  }
  /* Every donor audio track this TV can play or convert, for the audio menu
   * (owner, 07/11: a WEB-DL with a dub and a voice-over picks either). */
  for(unsigned i=0;i<x->fmt->nb_streams && e->info.n_tracks<DTS_TRACK_MAX;i++) {
    AVStream *ds=x->fmt->streams[i]; AVCodecParameters *dp=ds->codecpar;
    if(dp->codec_type!=AVMEDIA_TYPE_AUDIO || ((int)i!=k && ext_rank(e,dp)<=0)) continue;
    DtsTrack *t=&e->info.tracks[e->info.n_tracks++];
    memset(t,0,sizeof *t);
    t->kind=DTS_AUDIO; t->stream_id=ds->id;
    t->stream_index=(int)i==k?DTS_ENGINE_EXTERNAL_STREAM:DTS_ENGINE_EXTERNAL_STREAM+1+(int)i;
    t->channels=dp->ch_layout.nb_channels>0?dp->ch_layout.nb_channels:((int)i==k?e->info.channels:2);
    snprintf(t->codec,sizeof t->codec,"%s",avcodec_get_name(dp->codec_id));
    copy_metadata(t->language,sizeof t->language,ds->metadata,"language");
    copy_metadata(t->title,sizeof t->title,ds->metadata,"title");
  }
  e->info.duration_ns=av_rescale_q(e->fmt->duration,AV_TIME_BASE_Q,NS);
  e->info.start_ns=e->fmt->start_time==AV_NOPTS_VALUE?0:av_rescale_q(e->fmt->start_time,AV_TIME_BASE_Q,NS);
  e->target_ns=(int64_t)(start*1000000000.0);
  printf("[ext-audio] ready codec=%s ch=%d rate=%d stream=%d%s\n",e->info.audio_codec,e->info.channels,
    e->info.sample_rate,k,follow?" (following another cut)":"");
  e->fw_autoarm=follow || e->ext_automatch; e->mt_main_pref=0; e->mt_retry=0;
  fflush(stdout);
  return start>0?dts_engine_seek(e,start):0;
}
int dts_engine_open(DtsEngine *e,const char *url,const char *headers,int audio,int core,double start) {
#ifdef NV_DTS_PICTURE_DIAG
  if(e)video_diag_reset(e);
#endif
  if (!e || !url || !*url) return -1;
  __atomic_store_n(&e->range_live,0,__ATOMIC_RELAXED);
  close_media(e); memset(&e->info,0,sizeof(e->info)); e->error[0]=0;
  memset(&e->metrics,0,sizeof(e->metrics));
#if defined(NV_DTS_DEBUG) || defined(NV_HTTP_BUFFER_DIAG)
  memset(&e->logged_metrics,0,sizeof(e->logged_metrics)); e->logged_at_ns=0;
#endif
  e->pos=0; e->total=-1; e->cache_size=0; e->has_core=0;
  e->range_bytes=e->dv?RANGE_BYTES_DV:RANGE_BYTES; e->range_slots=e->dv?RANGE_SLOTS_DV:RANGE_SLOTS;
  if(g_ahead_mib>0) { e->range_bytes=RANGE_BYTES_DV; e->range_slots=(g_ahead_mib+3)/4; }
#ifdef NV_MP4_BD_TEST
  /* Reserve pairing scratch from this engine's existing payload budget. No
   * extra HTTP lane, second demuxer or video decoder is created. */
  if(e->allow_mp4) {
    if(!e->dv || e->range_slots<=4)return fail(e,"MP4 pairing buffer unavailable",0);
    e->range_slots-=4;
  }
#endif
  e->final_url[0]=0; e->final_cross=0; e->passthrough=0; e->remux=0; e->opening=1;
  e->xs_tries=e->xs_quiet=0; e->sg_first_ms=e->sg_win_ms=e->sg_blocked_ms=e->sg_wait_ms=0;
  e->demux_eof=e->decoder_eof=e->resampler_eof=e->encoder_sent_eof=e->timeline=0;
  /* Test option: one continuous request; the ring takes the same RAM the
   * range slots would (Settings > Buffer size, 64 MiB when automatic). */
  e->conv_stream=__atomic_load_n(&g_conversion_stream,__ATOMIC_RELAXED);
  e->dts_off=__atomic_load_n(&g_dts_off,__ATOMIC_RELAXED);
  if(e->conv_stream) {
    int mib=g_ahead_mib>0?g_ahead_mib:64;
    if(mib<16)mib=16;
    e->http_workers=1; e->fx_cap=(int64_t)mib*1024*1024; e->fx_req=-1;
  } else {
    e->http_workers=__atomic_load_n(&g_conversion_workers,__ATOMIC_RELAXED);
    if(e->http_workers>RANGE_WORKERS)e->http_workers=RANGE_WORKERS;
  }
  if (cancelled(e)) return fail(e,"Cancelled",0);
  if (audio < -1 || !isfinite(start) || start<0 || start>INT64_MAX/1000000000.0)
    return fail(e,"Invalid DTS conversion options",0);
  e->url=strdup(url); e->headers=strdup(headers?headers:"");
  if(e->conv_stream && e->url && e->headers) {
    /* One small probe learns the size and the final (redirected) address
     * before the stream starts, as the native cache does. */
    long got=0; int status=0,cross=0; int64_t total=-1; char fin[8192];
    char *probe=rede_baixar_trecho64_final(e->url,e->headers,0,255,&got,&total,&status,&e->cancelled,fin,&cross);
    int probed=probe!=NULL && total>0;
    free(probe);
    /* A refused or dead source fails now, like the first range would, not
     * after the stream's long retry schedule. */
    if(!probed) {
      printf("[http-buffer] path=conversion mode=stream probe-failed http=%d\n",status);fflush(stdout);
      return fail(e,cancelled(e)?"Cancelled":"Source probe",AVERROR(EIO));
    }
    pthread_mutex_lock(&e->range_lock);
    if(total>0) e->total=total;
    if(fin[0]) { snprintf(e->final_url,sizeof e->final_url,"%s",fin); e->final_cross=cross; }
    pthread_mutex_unlock(&e->range_lock);
    printf("[http-buffer] path=conversion connections=1 mode=stream bufferMiB=%lld probe=%d\n",
      (long long)(e->fx_cap>>20),status);fflush(stdout);
  }
  e->fmt=avformat_alloc_context();
  unsigned char *buffer=av_malloc(32768);
  if (!e->url || !e->headers || !e->fmt || !buffer) { av_free(buffer); return fail(e,"Demux allocation",0); }
  e->io=e->conv_stream?avio_alloc_context(buffer,32768,0,e,read_fluxo,NULL,seek_fluxo)
                     :avio_alloc_context(buffer,32768,0,e,read_range,NULL,seek_range);
  if (!e->io) { av_free(buffer); return fail(e,"IO allocation",0); }
  e->fmt->pb=e->io; e->fmt->flags|=AVFMT_FLAG_CUSTOM_IO;
  e->fmt->interrupt_callback=(AVIOInterruptCB){cancelled,e};
  /* Never give libavformat the signed URL; custom AVIO is its sole transport.
   * No decoder logs are enabled, including malformed metadata containing URLs. */
  av_log_set_level(AV_LOG_QUIET);
  AVDictionary *opts=NULL;
  /* External audio also takes an AVI or MPEG-TS picture source (owner's log
   * 07/11: a 4.3 GB OpenDML AVI the TV plays natively). Only there: the DTS and
   * Dolby Vision conversions keep their MKV/MP4 contract. */
  int ext_open=dts_engine_external_active(e);
  av_dict_set(&opts,"format_whitelist",ext_open?"matroska,webm,mov,avi,mpegts":"matroska,webm,mov",0);
  av_dict_set(&opts,"probesize","4194304",0);
  av_dict_set(&opts,"analyzeduration","5000000",0);
  int r=avformat_open_input(&e->fmt,NULL,NULL,&opts); av_dict_free(&opts);
  /* Not MKV/MP4 (an AVI or TS source, or a placeholder clip instead of the
   * film: owner's log 07/11) says so instead of FFmpeg's "Invalid data". */
  if(r==AVERROR_INVALIDDATA)
    return fail(e,ext_open?"This source is not a video file the player can open (or the debrid service sent a placeholder)"
                          :"This source is not an MKV or MP4 file (or the debrid service sent a placeholder)",0);
  if(r<0) return fail(e,"Container open",r);
  /* AVI stores decode order only (no presentation timestamps on video):
   * libavformat derives them, B-frames included, or every picture would be
   * dropped as untimed. */
  if(e->fmt->iformat && !strcmp(e->fmt->iformat->name,"avi")) e->fmt->flags|=AVFMT_FLAG_GENPTS;
  r=avformat_find_stream_info(e->fmt,NULL);
  e->opening=0;
  if(r<0) return fail(e,"Container metadata",r);
  if(e->fmt->duration==AV_NOPTS_VALUE || e->fmt->duration<=0)
    return fail(e,"Finite VOD duration required",0);
  e->info.audio_stream=e->info.video_stream=-1;
  int external=dts_engine_external_active(e);
  for(unsigned i=0;i<e->fmt->nb_streams;i++) {
    AVStream *s=e->fmt->streams[i]; AVCodecParameters *p=s->codecpar;
    if(av_packet_side_data_get(p->coded_side_data,p->nb_coded_side_data,AV_PKT_DATA_ENCRYPTION_INIT_INFO)
       || p->codec_tag==MKTAG('e','n','c','a') || p->codec_tag==MKTAG('e','n','c','v'))
      return fail(e,"Encrypted media is unsupported",0);
    if(p->codec_type==AVMEDIA_TYPE_VIDEO && e->info.video_stream<0 && !(s->disposition&AV_DISPOSITION_ATTACHED_PIC))
      e->info.video_stream=i;
    /* Dolby Vision path: AC-3/E-AC-3 normally pass through; the fixed-output
     * test still selects by SOURCE metadata before configuring its encoder. */
    if(!external && nv_dts_audio_feeds_off(avcodec_get_name(p->codec_id),e->dv,e->dts_off)
       && e->info.audio_stream<0 && (audio<0 || audio==(int)i)) e->info.audio_stream=i;
    int kind=p->codec_type==AVMEDIA_TYPE_VIDEO?DTS_VIDEO:p->codec_type==AVMEDIA_TYPE_AUDIO?DTS_AUDIO:p->codec_type==AVMEDIA_TYPE_SUBTITLE?DTS_SUBTITLE:0;
    /* External audio: the main file's audio is never read or listed. */
    if(external && kind==DTS_AUDIO) { s->discard=AVDISCARD_ALL; continue; }
    if(kind && e->info.n_tracks<DTS_TRACK_MAX) {
      DtsTrack *t=&e->info.tracks[e->info.n_tracks++]; t->kind=kind; t->stream_index=i; t->stream_id=s->id;
      t->channels=p->ch_layout.nb_channels; t->forced=!!(s->disposition&AV_DISPOSITION_FORCED);
      snprintf(t->codec,sizeof(t->codec),"%s",avcodec_get_name(p->codec_id));
      copy_metadata(t->language,sizeof(t->language),s->metadata,"language");
      copy_metadata(t->title,sizeof(t->title),s->metadata,"title");
    }
  }
  if(e->info.video_stream<0 || (!external && e->info.audio_stream<0)) return fail(e,"Selected DTS track or video absent",0);
#ifdef NV_MP4_BD_TEST
  if(mp4_configure(e)<0)return -1;
#endif
  AVStream *v=e->fmt->streams[e->info.video_stream]; AVCodecParameters *vp=v->codecpar;
  const char *filter=vp->codec_id==AV_CODEC_ID_H264?"h264_mp4toannexb":vp->codec_id==AV_CODEC_ID_HEVC?"hevc_mp4toannexb":NULL;
  if(!filter && vp->codec_id!=AV_CODEC_ID_VP9 && vp->codec_id!=AV_CODEC_ID_AV1)
    return fail(e,external?"The picture of this source is an old codec (XviD/DivX): audio from another version needs H.264 or HEVC"
                          :"Compressed video codec unsupported",0);
  const AVPacketSideData *dv=av_packet_side_data_get(vp->coded_side_data,vp->nb_coded_side_data,AV_PKT_DATA_DOVI_CONF);
#ifdef NV_MP4_BD_TEST
  if(e->mp4) {
    AVCodecParameters *ep=e->fmt->streams[e->mp4->el_stream]->codecpar;
    dv=av_packet_side_data_get(ep->coded_side_data,ep->nb_coded_side_data,AV_PKT_DATA_DOVI_CONF);
  }
#endif
  if(dv && dv->size>=sizeof(AVDOVIDecoderConfigurationRecord)) {
    const AVDOVIDecoderConfigurationRecord *conf=(const AVDOVIDecoderConfigurationRecord *)dv->data;
    e->info.dovi_profile=conf->dv_profile; e->info.dovi_level=conf->dv_level;
    e->info.dovi_rpu_present=conf->rpu_present_flag;
    e->info.dovi_el_present=conf->el_present_flag;
    e->info.dovi_bl_present=conf->bl_present_flag;
#ifdef NV_MP4_BD_TEST
    if(e->mp4)e->info.dovi_bl_present=1; /* verified separate base track */
#endif
    e->info.dovi_bl_compatibility_id=conf->dv_bl_signal_compatibility_id;
#ifdef NV_MP4_BD_TEST
    /* The opt-in single-track recovery proved PQ/BT.2020 from the actual SPS
     * and validated P8 RPU before this output-only compatibility correction.
     * Preserve the input record and every compressed picture/RPU byte. */
    if(e->mp4_p8_compat_verified)e->info.dovi_bl_compatibility_id=1;
#endif
    int unsupported=(conf->dv_profile == 7 && !e->allow_p7) ||
        !nv_dv7_profile(conf->dv_profile,conf->el_present_flag,
                       e->info.dovi_bl_present,conf->rpu_present_flag) || vp->codec_id!=AV_CODEC_ID_HEVC;
    int compat=conf->dv_bl_signal_compatibility_id;
    if (unsupported && external && vp->codec_id==AV_CODEC_ID_HEVC && e->info.dovi_bl_present &&
        (compat==1 || compat==2 || compat==4 || compat==6)) {
      /* External audio needs this path, the Dolby Vision layer cannot take it
       * (P7 with the P7 conversion off): play the HDR10/SDR/HLG base layer,
       * enhancement layer and RPU dropped from every picture. */
      printf("[ext-audio] Dolby Vision profile %d not converted here: base layer only (compatibility %d)\n",
        conf->dv_profile,compat);
      fflush(stdout);
      e->dv_strip=1; e->info.dovi_profile=e->info.dovi_level=0;
      e->info.dovi_rpu_present=e->info.dovi_el_present=e->info.dovi_bl_present=0;
      e->info.dovi_bl_compatibility_id=0;
      dv=NULL;
    } else if (unsupported)
      return fail(e,"Dolby Vision layer/profile unsupported",0);
    if (dv && conf->dv_profile == 7) {
      e->dv7=1;
      e->info.dovi_profile=8; e->info.dovi_el_present=0;
      e->info.dovi_bl_compatibility_id=1;
      printf("[dv7] experimental P7 -> P8.1: RPU converted, enhancement layer discarded\n");
      printf("[dv7] RPU mapping=%s; FEL picture discarded; session snapshot\n",
             e->preserve_p7_mapping ? "preserved" : "standard");fflush(stdout);
    }
    /* mp4toannexb preserves HEVC RPU NAL units; no video re-encode occurs. */
  }
  e->info.width=vp->width; e->info.height=vp->height;
  snprintf(e->info.video_codec,sizeof(e->info.video_codec),"%s",avcodec_get_name(vp->codec_id));
  e->info.fps_num=v->avg_frame_rate.num; e->info.fps_den=v->avg_frame_rate.den;
  e->info.color_primaries=vp->color_primaries; e->info.color_transfer=vp->color_trc; e->info.color_matrix=vp->color_space;
  snprintf(e->info.hdr,sizeof(e->info.hdr),"%s",e->info.dovi_profile?"DolbyVision":vp->color_trc==AVCOL_TRC_SMPTE2084?"PQ":vp->color_trc==AVCOL_TRC_ARIB_STD_B67?"HLG":"SDR");
  if(vp->codec_id==AV_CODEC_ID_HEVC) {
    /* An hvcC this code reads is converted by hevc_annexb_packet; anything
     * else keeps FFmpeg's hevc_mp4toannexb exactly as before. */
    int own=nv_hevc_annexb_open(&e->hevc_annexb,vp->extradata,vp->extradata_size>0?(size_t)vp->extradata_size:0);
    if(own<0) return fail(e,"Video filter allocation",AVERROR(ENOMEM));
    if(own) filter=NULL;
#ifdef NV_DTS_PICTURE_DIAG
    memset(e->annexb_modes,0,sizeof e->annexb_modes); e->annexb_logged=0;
    printf("[hevc-annexb] converter=%s lengthSize=%d hvccSetBytes=%zu\n",own?"own":"ffmpeg",
      e->hevc_annexb.length_size,e->hevc_annexb.sets_size);
    fflush(stdout);
#endif
  }
  if(filter) {
    const AVBitStreamFilter *video_filter=av_bsf_get_by_name(filter);
    if(!video_filter) return fail(e,"Video filter absent",0);
    r=av_bsf_alloc(video_filter,&e->bsf);
    if(r<0) return fail(e,"Video filter allocation",r);
    avcodec_parameters_copy(e->bsf->par_in,vp); e->bsf->time_base_in=v->time_base;
    if((r=av_bsf_init(e->bsf))<0) return fail(e,"Video filter open",r);
  }
  return external?configure_external(e,start):configure_audio(e,core,start);
}
int dts_engine_audio(DtsEngine *e,int stream,int core,double target) {
  if(!e || !e->fmt || e->remux || e->ext || cancelled(e) || stream<0 ||
     stream>=(int)e->fmt->nb_streams || !isfinite(target) || target<0 ||
     target>INT64_MAX/1000000000.0) return -1;
  AVCodecParameters *ap=e->fmt->streams[stream]->codecpar;
  if(ap->codec_type!=AVMEDIA_TYPE_AUDIO ||
     !nv_dts_audio_feeds_off(avcodec_get_name(ap->codec_id),e->dv,e->dts_off)) return -1;
  // Free the previous audio chain before allocating its replacement. The
  // demuxer, video filter, DV conversion state and bounded HTTP queue survive.
  av_packet_free(&e->in); av_packet_free(&e->out);
  av_frame_free(&e->decoded); av_frame_free(&e->encoded);
  avcodec_free_context(&e->dec); avcodec_free_context(&e->enc);
  swr_free(&e->swr); av_audio_fifo_free(e->fifo); e->fifo=NULL;
  av_channel_layout_uninit(&e->input_layout);
  e->info.audio_stream=stream;e->has_core=0;e->error[0]=0;
  e->demux_eof=e->decoder_eof=e->resampler_eof=e->encoder_sent_eof=e->timeline=0;
  // Always seek, including position zero, to reset libavformat's packet queue.
  if(configure_audio(e,core,0)<0) return -1;
  return dts_engine_seek(e,target);
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
  e->final_seeded=0;
  final_seed(e);
  e->fx_req=-1;
  e->io=avio_alloc_context(buffer,65536,0,e,read_fluxo,NULL,seek_fluxo);
  if (!e->io) { av_free(buffer); return fail(e,"IO allocation",0); }
  e->fmt->pb=e->io; e->fmt->flags|=AVFMT_FLAG_CUSTOM_IO;
  e->fmt->interrupt_callback=(AVIOInterruptCB){cancelled,e};
  AVDictionary *opts=NULL;
  /* The other version may also be an AVI or a TS (small releases with a dub
   * or a voice-over; owner, 07/11). */
  av_dict_set(&opts,"format_whitelist","matroska,webm,mov,avi,mpegts",0);
  av_dict_set(&opts,"probesize","1048576",0);
  int r=avformat_open_input(&e->fmt,NULL,NULL,&opts); av_dict_free(&opts);
  if(r<0) return fail(e,"Container open",r);
  /* TS declares its tracks without their parameters (channels, rate) and no
   * duration: a short probe of its packets fills both. */
  if(!strcmp(e->fmt->iformat->name,"mpegts")) {
    e->fmt->max_analyze_duration=3000000;
    if(avformat_find_stream_info(e->fmt,NULL)<0) return fail(e,"Container metadata",0);
  }
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
    /* Another version at another speed: its audio is stretched by the ratio
     * (only the ratio of the two rates matters; x100 keeps it exact). */
    if(e->ext_conv && ext_k(e)!=1.0)
      r=swr_alloc_set_opts2(&e->swr,&e->enc->ch_layout,e->enc->sample_fmt,4800000,&f->ch_layout,f->format,
                            (int)llrint(f->sample_rate*100.0/ext_k(e)),0,NULL);
    else
    r=swr_alloc_set_opts2(&e->swr,&e->enc->ch_layout,e->enc->sample_fmt,48000,&f->ch_layout,f->format,f->sample_rate,0,NULL);
    if(r<0 || (r=swr_init(e->swr))<0) return fail(e,"Channel resampler open",r);
  }
  if(!e->swr) return 0;
  if(f && e->timeline && f->best_effort_timestamp!=AV_NOPTS_VALUE) {
    int64_t incoming=av_rescale_q(f->best_effort_timestamp,e->dec->pkt_timebase,e->enc->time_base);
    if(e->ext_conv && ext_k(e)!=1.0) incoming=llrint(incoming*ext_k(e));   /* stretched timeline */
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
       * At most four holes / two seconds of silence per 30 s of media progress:
       * one hole may be up to 2 s (owner's log 07/11: a 1.06 s hole where the
       * two discs of an extended edition were joined ended the DV session).
       * swr_inject_silence takes INPUT samples; its buffered delay is included
       * in the output capacity below (also at 44.1/96 kHz). */
      int64_t gap=incoming-expected;
      const char *reason=NULL;
      if(gap<0) reason="backward";
      else if(gap>96000) reason="too-large";
      else {
        if(e->pad_count && expected-e->pad_anchor>=30LL*48000)
          e->pad_count=e->pad_samples=0;
        if(e->pad_count>=4 || gap>96000-e->pad_samples) reason="budget";
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
  if(e->ext_conv && ext_k(e)!=1.0) capacity=(int)(capacity*ext_k(e))+64;
  uint8_t **data=NULL;
  r=av_samples_alloc_array_and_samples(&data,NULL,e->enc->ch_layout.nb_channels,capacity,e->enc->sample_fmt,0);
  if(r<0) return fail(e,"Resample buffer",r);
  int count=swr_convert(e->swr,data,capacity,f?(const uint8_t **)f->extended_data:NULL,f?f->nb_samples:0);
  if(count>=0 && f && !e->timeline) {
    int64_t pts=f->best_effort_timestamp;
    if(pts==AV_NOPTS_VALUE) { av_freep(&data[0]); av_freep(&data); return fail(e,"DTS sample timestamp absent",0); }
    e->sample_pts=av_rescale_q(pts,e->dec->pkt_timebase,e->enc->time_base); e->timeline=1;
    if(e->ext_conv && ext_k(e)!=1.0) e->sample_pts=llrint(e->sample_pts*ext_k(e));
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
/* Video packet in e->in -> Annex B in place (new buffer, same props and side
 * data). Same output and same rejections as hevc_mp4toannexb, except that an
 * IRAP packet's own VPS/SPS/PPS are never followed by older hvcC copies. */
static int hevc_annexb_packet(DtsEngine *e) {
  AVPacket *p=e->in; size_t size=0; int mode=0;
  if(nv_hevc_annexb_packet(&e->hevc_annexb,p->data,p->size>0?(size_t)p->size:0,NULL,0,&size,&mode)<0)
    return AVERROR_INVALIDDATA;
  if(size>(size_t)(INT_MAX-AV_INPUT_BUFFER_PADDING_SIZE)) return AVERROR_INVALIDDATA;
  AVBufferRef *b=av_buffer_alloc(size+AV_INPUT_BUFFER_PADDING_SIZE);
  if(!b) return AVERROR(ENOMEM);
  if(nv_hevc_annexb_packet(&e->hevc_annexb,p->data,p->size>0?(size_t)p->size:0,b->data,size,&size,NULL)<0) {
    av_buffer_unref(&b); return AVERROR_INVALIDDATA;
  }
  memset(b->data+size,0,AV_INPUT_BUFFER_PADDING_SIZE);
  av_buffer_unref(&p->buf); p->buf=b; p->data=b->data; p->size=(int)size;
#ifdef NV_DTS_PICTURE_DIAG
  unsigned count=++e->annexb_modes[mode&3];
  if((mode==NV_HEVC_ANNEXB_INBAND || mode==NV_HEVC_ANNEXB_MIXED) && (e->annexb_logged<8 || !(count&255))) {
    static const char *const names[4]={"no-irap","container","inband","mixed"};
    AVStream *v=e->fmt->streams[e->info.video_stream];
    e->annexb_logged++;
    printf("[hevc-annexb] mode=%s ptsNs=%lld count=%u container=%u inband=%u mixed=%u\n",names[mode&3],
      (long long)ns(p->pts,v->time_base),count,e->annexb_modes[NV_HEVC_ANNEXB_CONTAINER],
      e->annexb_modes[NV_HEVC_ANNEXB_INBAND],e->annexb_modes[NV_HEVC_ANNEXB_MIXED]);
    fflush(stdout);
  }
#endif
  return 0;
}
static int frame_out(DtsEngine *e,DtsFrame *f,int kind,AVRational tb) {
  f->kind=kind; f->stream_index=kind==DTS_SUBTITLE?e->out->stream_index:kind==DTS_AUDIO?e->info.audio_stream:e->info.video_stream;
  f->data=e->out->data; f->size=e->out->size;
  f->pts_ns=ns(e->out->pts,tb); f->dts_ns=ns(e->out->dts,tb); f->duration_ns=ns(e->out->duration,tb);
  return 1;
}
/* External audio is released this far ahead of the newest video time, so
 * the two streams reach the native pipeline interleaved as in one file. */
#define EXT_LEAD_NS INT64_C(500000000)
/* Keep one packet of the other file pending, already shifted by the offset
 * and trimmed to the seek target. 0 ok (pending or end), -1 error. */
#include "dts_match.inc"
#include <ctype.h>
static int ext_donor_seek(DtsEngine *e,double t,int *how);
static int ext_reset_chain(DtsEngine *e);
static void follow_streams(DtsEngine *e,int *mains,int *donor);
static void follow_start(DtsEngine *e,double score);
/* "MATCH AUTOMATICALLY" (owner, 07/11, on click only): DTS_MATCH_SECONDS of
 * this file's audio (a track that is read anyway, interleaved with the
 * picture: no extra download) and of the other version's, decoded to band
 * envelopes while playing; dts_match.inc finds the offset. When it is not
 * near, a reader of its own fetches the other version around the offsets
 * worth trying (struct MatchJob). */
typedef struct {
  AVCodecContext *dec; SwrContext *swr; AVFrame *fr;
  DtsMatchEnv ev; int64_t t0_ns; int cap;
  float *env[DTS_MATCH_BANDS];
} MatchSide;
/* s[2]: a second track of this file captured with the first when the two
 * files share no language (alt_stream, -1 none); the better one is kept. */
struct MatchCap { int main_stream, donor_stream, alt_stream, alt_same, early; MatchSide s[3]; };
struct MatchJob {
  pthread_t th; pthread_mutex_t lock; int started, done, cancel;
  DtsEngine *holder;                       /* the capture in progress, to cancel its reads */
  char *url, *headers; int stream;
  float *menv[DTS_MATCH_BANDS]; int mn; double mt0;   /* this file's capture (a copy) */
  double center, base, k0, kf, main_dur, donor_dur; int cut_s; int64_t donor_total;
  int found, how, heavy; double off, ratio, score;    /* result (raw offset, s) */
  int ncand; double cand[DTS_MATCH_CAND], cand_sc[DTS_MATCH_CAND];   /* best places, raw */
};
/* The best places over several searches, 0.5 s apart, best first. */
static void match_cand_add(int *n,double *off,double *sc,double o,double v) {
  for(int i=0;i<*n;i++) if(fabs(off[i]-o)<0.5) { if(v>sc[i]) { sc[i]=v; off[i]=o; } goto sort; }
  if(*n<DTS_MATCH_CAND) { off[*n]=o; sc[*n]=v; (*n)++; }
  else if(v>sc[*n-1]) { off[*n-1]=o; sc[*n-1]=v; }
sort:
  for(int i=1;i<*n;i++) for(int k=i;k>0 && sc[k]>sc[k-1];k--) {
    double t=sc[k]; sc[k]=sc[k-1]; sc[k-1]=t; t=off[k]; off[k]=off[k-1]; off[k-1]=t;
  }
}
/* Not confident: is one of its candidates confirmed on the two halves? */
static int match_try_verify(const DtsMatchSig *sm,const DtsMatchSig *sd,double k,DtsMatchResult *r) {
  for(int i=0;i<r->ncand;i++)
    /* Not on a near-zero peak (owner's log 07/11: a 0.05 one was "confirmed"). */
    if(r->cand_score[i]>=0.10 && dts_match_verify(sm,sd,k,r->cand_off[i])) {
      printf("[ext-audio] match: %.3f s (score %.2f) confirmed on both halves of the capture\n",r->cand_off[i],r->cand_score[i]);
      fflush(stdout);
      r->offset_s=r->cand_off[i]; r->score=r->cand_score[i]; r->confident=1;
      return 1;
    }
  return 0;
}
static void match_respiro(void) { usleep(1000); }
static void match_side_free(MatchSide *m) {
  avcodec_free_context(&m->dec); swr_free(&m->swr); av_frame_free(&m->fr);
  for(int b=0;b<DTS_MATCH_BANDS;b++) av_freep(&m->env[b]);
}
static int match_side_open(MatchSide *m,AVStream *st,int cap) {
  const AVCodec *c=avcodec_find_decoder(st->codecpar->codec_id);
  memset(m,0,sizeof *m);
  m->t0_ns=INT64_MIN; m->cap=cap;
  for(int b=0;b<DTS_MATCH_BANDS;b++) if(!(m->env[b]=av_malloc(sizeof(float)*(size_t)cap))) return -1;
  if(!c || !(m->dec=avcodec_alloc_context3(c)) || !(m->fr=av_frame_alloc())) return -1;
  avcodec_parameters_to_context(m->dec,st->codecpar);
  m->dec->pkt_timebase=st->time_base;
  if(st->codecpar->codec_id==AV_CODEC_ID_DTS) av_opt_set_int(m->dec->priv_data,"core_only",1,0);
  return avcodec_open2(m->dec,c,NULL)<0?-1:0;
}
static int match_side_full(const MatchSide *m) { return m->ev.max>0 && m->ev.n>=m->ev.max; }
static void match_side_sig(const MatchSide *m,DtsMatchSig *s) {
  for(int b=0;b<DTS_MATCH_BANDS;b++) s->env[b]=m->ev.env[b];
  s->n=m->ev.n; s->t0=m->t0_ns!=INT64_MIN?m->t0_ns/1e9:0;
}
/* One decoded frame of a side into its envelopes (the frame is unref'd).
 * -1 broken. */
static int match_side_frame(MatchSide *m,AVFrame *fr,AVRational tb) {
  if(!m->swr) {
    AVChannelLayout mono=AV_CHANNEL_LAYOUT_MONO;
    if(swr_alloc_set_opts2(&m->swr,&mono,AV_SAMPLE_FMT_FLT,fr->sample_rate,&fr->ch_layout,fr->format,fr->sample_rate,0,NULL)<0 ||
       swr_init(m->swr)<0) { swr_free(&m->swr); av_frame_unref(fr); return -1; }
    dts_match_env_init(&m->ev,fr->sample_rate,m->env,m->cap);
  }
  if(m->t0_ns==INT64_MIN) {
    int64_t p=fr->best_effort_timestamp!=AV_NOPTS_VALUE?fr->best_effort_timestamp:fr->pts;
    if(p==AV_NOPTS_VALUE) { av_frame_unref(fr); return 0; }
    m->t0_ns=ns(p,tb);
  }
  float buf[8192]; uint8_t *out=(uint8_t *)buf;
  int left=fr->nb_samples; const uint8_t **in=(const uint8_t **)fr->extended_data;
  while(left>0 || in) {
    int got=swr_convert(m->swr,&out,8192,in,in?left:0);
    in=NULL; left=0;
    if(got<=0) break;
    dts_match_env_push(&m->ev,buf,got);
  }
  av_frame_unref(fr);
  return 0;
}
/* One packet of a side into its envelopes. 1 when full, 0 more, -1 broken. */
static int match_side_feed(MatchSide *m,AVPacket *pkt,AVRational tb) {
  if(match_side_full(m)) return 1;
  if(!m->dec || avcodec_send_packet(m->dec,pkt)<0) return 0;
  while(avcodec_receive_frame(m->dec,m->fr)==0)
    if(match_side_frame(m,m->fr,tb)<0) return -1;
  return match_side_full(m);
}
static void match_disarm(DtsEngine *e) {
  struct MatchCap *mc=e->mt;
  if(!mc) return;
  int fm=-1, fd=-1;
  follow_streams(e,&fm,&fd);   /* tracks the follow keeps reading stay on */
  if(e->fmt && mc->main_stream>=0 && (unsigned)mc->main_stream<e->fmt->nb_streams && mc->main_stream!=fm)
    e->fmt->streams[mc->main_stream]->discard=AVDISCARD_ALL;
  /* The other version's track read only for the comparison goes back off. */
  if(e->ext && e->ext->fmt && mc->donor_stream!=e->ext_stream && mc->donor_stream>=0 && mc->donor_stream!=fd &&
     (unsigned)mc->donor_stream<e->ext->fmt->nb_streams)
    e->ext->fmt->streams[mc->donor_stream]->discard=AVDISCARD_ALL;
  if(e->fmt && mc->alt_stream>=0 && (unsigned)mc->alt_stream<e->fmt->nb_streams && mc->alt_stream!=fm &&
     mc->alt_stream!=mc->main_stream)
    e->fmt->streams[mc->alt_stream]->discard=AVDISCARD_ALL;
  match_side_free(&mc->s[0]); match_side_free(&mc->s[1]); match_side_free(&mc->s[2]);
  av_free(mc); e->mt=NULL; e->mt_check=0;
}
static void match_job_free(struct MatchJob *j) {
  if(!j) return;
  free(j->url); free(j->headers);
  for(int b=0;b<DTS_MATCH_BANDS;b++) free(j->menv[b]);
  pthread_mutex_destroy(&j->lock); free(j);
}
static void match_job_stop(DtsEngine *e) {
  struct MatchJob *j=e->mj;
  if(!j) return;
  pthread_mutex_lock(&j->lock);
  j->cancel=1;
  if(j->holder) { __atomic_store_n(&j->holder->cancelled,1,__ATOMIC_RELAXED); if(j->holder->ext) dts_engine_cancel(j->holder->ext); }
  pthread_mutex_unlock(&j->lock);
  if(j->started) pthread_join(j->th,NULL);
  match_job_free(j); e->mj=NULL;
}
/* Session closed or another version: nothing of the match survives. */
static void follow_free(DtsEngine *e);
static void match_stop(DtsEngine *e) {
  match_disarm(e); match_job_stop(e); e->mt_t1=-1; e->mt_later=0;
  quiet_free(e);
  follow_free(e); e->fw_autoarm=0; e->fw_small=0;
}
/* A fresh reader of the other version, `seconds` of its chosen track from
 * `from` (its own timeline) into `out`. 0 ok. */
static int match_capture(struct MatchJob *j,double from,double seconds,MatchSide *out) {
  DtsEngine *h=dts_engine_create(), *c=dts_engine_create();
  int r=-1;
  if(!h || !c) { dts_engine_destroy(h); dts_engine_destroy(c); return -1; }
  c->fx_mib=g_ext_ring_mib; c->parent=h; h->ext=c;
  pthread_mutex_lock(&j->lock);
  int cancel=j->cancel; if(!cancel) j->holder=h;
  pthread_mutex_unlock(&j->lock);
  if(!cancel && dts_engine_open_demux(c,j->url,j->headers)==0 && (unsigned)j->stream<c->fmt->nb_streams) {
    for(unsigned i=0;i<c->fmt->nb_streams;i++) if((int)i!=j->stream) c->fmt->streams[i]->discard=AVDISCARD_ALL;
    h->ext_stream=j->stream;
    AVStream *st=c->fmt->streams[j->stream];
    int how=0;
    if(ext_donor_seek(h,from,&how)>=0 && match_side_open(out,st,(int)(seconds*DTS_MATCH_HZ)+DTS_MATCH_HZ)==0) {
      AVPacket *pkt=av_packet_alloc();
      int64_t stop=(int64_t)((from+seconds+60.0)*1e9);
      while(pkt && !__atomic_load_n(&j->cancel,__ATOMIC_RELAXED) && av_read_frame(c->fmt,pkt)>=0) {
        int full=0;
        if(pkt->stream_index==j->stream) {
          full=match_side_feed(out,pkt,st->time_base)!=0;
          if(pkt->pts!=AV_NOPTS_VALUE && ns(pkt->pts,st->time_base)>stop) full=1;
        }
        av_packet_unref(pkt);
        if(full) break;
      }
      av_packet_free(&pkt);
      if(out->ev.n>=5*DTS_MATCH_HZ && out->t0_ns!=INT64_MIN) r=0;
    }
  }
  pthread_mutex_lock(&j->lock); j->holder=NULL; pthread_mutex_unlock(&j->lock);
  dts_engine_destroy(h);   /* and its reader c */
  return r;
}
static void *match_job_run(void *arg) {
  struct MatchJob *j=arg;
  /* Below the interface and the playback threads: the menu keeps its frames
   * (owner's log 07/11: the +-5 min search made it stutter). */
#ifdef SYS_gettid
  setpriority(PRIO_PROCESS,(id_t)syscall(SYS_gettid),10);
#endif
  dts_match_respiro=match_respiro;
  DtsMatchSig sm={{j->menv[0],j->menv[1],j->menv[2],j->menv[3]},j->mn,j->mt0};
  const double tm=j->mt0+j->mn/2.0/DTS_MATCH_HZ;   /* centre of this file's window */
  const double bps=j->donor_total>0 && j->donor_dur>0?j->donor_total/j->donor_dur:0;
  double ks[2]={j->k0,j->kf}; int nk=j->kf>0 && fabs(j->kf-j->k0)>0.0005?2:1;
  struct { double c, r, cap_mb; int how; } st[5]; int n=0;
  /* The lengths suggest where the extra minutes are: all at the start
   * (offset -cut), spread evenly (proportional), or at the end (no change,
   * already searched). */
  /* Over 10 minutes apart (an extended against a theatrical cut, owner's log
   * 07/11): the extra scenes are spread through the film, so only the
   * proportional place makes sense, searched wider; +-5 min is skipped. */
  int far=abs(j->cut_s)>600;
  if(abs(j->cut_s)>10) {
    double h1=-j->cut_s+j->base, h2=-j->cut_s*(j->main_dur>0?tm/j->main_dur:0)+j->base;
    if(!far) { st[n].c=h1; st[n].r=10; st[n].cap_mb=1e9; st[n++].how=2; }
    if(far || (fabs(h2-h1)>15 && fabs(h2-j->center)>15)) { st[n].c=h2; st[n].r=far?60:10; st[n].cap_mb=300; st[n++].how=2; }
    /* Another cut far apart: one direction only (a longer picture has the
     * other version's scene earlier, a shorter one later), around the
     * proportional place, wider. */
    if(far) {
      double c=h2, r=150;
      if(j->cut_s<0 && c-r<j->base) c=j->base+r;
      if(j->cut_s>0 && c+r>j->base) c=j->base-r;
      st[n].c=c; st[n].r=r; st[n].cap_mb=300; st[n++].how=4;
    }
  }
  st[n].c=j->center; st[n].r=60; st[n].cap_mb=300; st[n++].how=3;
  if(!far) { st[n].c=j->center; st[n].r=300; st[n].cap_mb=300; st[n++].how=4; }
  for(int i=0;i<n && !__atomic_load_n(&j->cancel,__ATOMIC_RELAXED) && !j->found;i++) {
    if(st[i].how==2 && fabs(st[i].c-j->center)<10) continue;   /* the first search covered it */
    double lo=1e18, hi=-1e18;
    const double pad=DTS_MATCH_WIN_S/2.0+5.0;   /* the window, and the two halves of the check */
    for(int q=0;q<nk;q++) {
      double a=(tm-pad-(st[i].c+st[i].r))/ks[q], b=(tm+pad-(st[i].c-st[i].r))/ks[q];
      if(a<lo) lo=a;
      if(b>hi) hi=b;
    }
    lo-=2.0; hi+=2.0;
    if(lo<0) lo=0;
    if(hi<=lo+5.0) continue;
    double mb=bps*(hi-lo)/1048576.0;
    if(bps>0 && mb>st[i].cap_mb) {
      printf("[ext-audio] match: +-%.0f s skipped, about %.0f MB of the other version to read\n",st[i].r,mb);
      fflush(stdout); j->heavy=1; continue;
    }
    printf("[ext-audio] match: %s, reading %.0f s of the other version (about %.0f MB)\n",
      st[i].how==2?"trying the offset its length suggests":st[i].r<100?"searching +-60 s":"searching +-5 min",hi-lo,mb);
    fflush(stdout);
    MatchSide side;
    memset(&side,0,sizeof side);   /* freed below even when the capture failed early */
    if(match_capture(j,lo,hi-lo,&side)==0) {
      DtsMatchSig sd; match_side_sig(&side,&sd);
      for(int q=0;q<nk;q++) {
        DtsMatchResult r;
        int got=dts_match_search(&sm,&sd,ks[q],st[i].c,st[i].r,&r);
        if(got && !r.confident) match_try_verify(&sm,&sd,ks[q],&r);
        if(got && r.confident && (!j->found || r.score>j->score)) {
          j->found=1; j->how=st[i].how; j->off=r.offset_s; j->ratio=ks[q]; j->score=r.score;
        }
        if(got && ks[q]==j->k0) for(int c=0;c<r.ncand;c++) match_cand_add(&j->ncand,j->cand,j->cand_sc,r.cand_off[c],r.cand_score[c]);
        printf("[ext-audio] match: ratio %.5f around %.1f s: offset %.3f s score %.2f margin %.2f%s\n",ks[q],st[i].c-j->base,
          r.offset_s-j->base,r.score,r.margin,r.confident?" (trusted)":"");
        fflush(stdout);
      }
    }
    match_side_free(&side);
  }
  pthread_mutex_lock(&j->lock); j->done=1; pthread_mutex_unlock(&j->lock);
  return NULL;
}
static void match_fail(DtsEngine *e,int heavy);
static void match_report(DtsEngine *e,double off_raw,double ratio,double score,int how) {
  int64_t user=llrint((off_raw*1e9-e->ext_base_ns)/1e6);
  /* Beyond the sync's reach: never clipped and applied (owner's log 07/11:
   * +13.6 min became +10 min). */
  if(user>DTS_ENGINE_EXTERNAL_OFFSET_MAX_MS || user<-DTS_ENGINE_EXTERNAL_OFFSET_MAX_MS) {
    printf("[ext-audio] match: %lld ms is beyond the sync's reach, not applied\n",(long long)user); fflush(stdout);
    match_fail(e,0); return;
  }
  e->mt_state=2; e->mt_offset_ms=(int)user; e->mt_score=(int)lround(score*100.0);
  e->mt_ratio_ppm=ratio>0?(int)llrint((ratio-1.0)*1e6):0; e->mt_how=how;
  /* Another cut, with "Follow through the whole film" on: from this find on. */
  if(how!=5 && e->fw_on && abs(e->ext_cut_s)>10) follow_start(e,score);
  /* Versions whose gaps differ: the small-shift follow, by itself. */
  else if(how!=5 && e->mt_watch && !e->fw && abs(e->ext_cut_s)<=10) { e->fw_small=1; follow_start(e,score); if(!e->fw) e->fw_small=0; }
}
static void match_fail(DtsEngine *e,int heavy) {
  e->mt_state=3; e->mt_how=0; e->mt_heavy=heavy;
  /* The lengths' hint only within the sync's reach: an extended cut 50 min
   * longer is no hint (owner's log 07/11 showed it clipped to +10 min). */
  e->mt_hint_ms=abs(e->ext_cut_s)>10 && abs(e->ext_cut_s)<=600?-e->ext_cut_s*1000:0;
}
static void match_set_cands(DtsEngine *e,int n,const double *off,const double *sc) {
  e->mt_ncand=0;
  for(int i=0;i<n && e->mt_ncand<3;i++) {
    int64_t u=llrint((off[i]*1e9-e->ext_base_ns)/1e6);
    if(u>DTS_ENGINE_EXTERNAL_OFFSET_MAX_MS || u<-DTS_ENGINE_EXTERNAL_OFFSET_MAX_MS) continue;
    e->mt_cand_ms[e->mt_ncand]=(int)u; e->mt_cand_sc[e->mt_ncand++]=(int)lround(sc[i]*100.0);
  }
}
/* A find of a later check: reported like any other, marked for the app
 * (no toast, a quiet moment for the change, not the series memory). */
static void match_report_later(DtsEngine *e,double off_raw,double score) {
  match_report(e,off_raw,0,score,1);
  if(e->mt_state==2) { e->mt_later=1; e->mt_o1_user_ns=(int64_t)e->mt_offset_ms*1000000; }
}
/* Checking all through: the find of this check against the offset playing
 * now. A move of 40 ms or more (the app's AUDIOEXT_AUTO_MIN_MS) is applied. */
static void match_watch_step(DtsEngine *e,double t2,const DtsMatchResult *r) {
  double step=r->offset_s-(e->ext_offset_ns+e->ext_base_ns)/1e9;
  e->mt_t1=t2; e->mt_o1=r->offset_s;
  if(fabs(step)>=0.040) {
    e->mt_same=0; e->mt_stepped=1;
    printf("[ext-audio] match: check at %.0f s: the offset moved by %+d ms to %d ms\n",t2,(int)lround(step*1000.0),
           (int)llround((r->offset_s*1e9-e->ext_base_ns)/1e6));
    fflush(stdout);
    match_report_later(e,r->offset_s,r->score);
    if(e->mt_state==2) e->mt_o1_user_ns=(int64_t)e->mt_offset_ms*1000000;
    return;
  }
  e->mt_same++;
  printf("[ext-audio] match: check at %.0f s: offset unchanged (%+d ms), next check in %d min\n",t2,
         (int)lround(step*1000.0),e->mt_same>=2?6:e->mt_stepped?2:3);
  fflush(stdout);
}
/* A find at the same speed is looked at again 5 minutes later: a drift
 * between the two means another speed (owner's log 07/11: 23.970 vs 23.976
 * fps, or a PAL version). With the sync checked all through (mt_watch) it
 * is looked at again every few minutes to the end. */
static void match_second_look(DtsEngine *e,const DtsMatchSig *sm,const DtsMatchSig *sd) {
  DtsMatchResult r;
  double t2=sm->t0+sm->n/2.0/DTS_MATCH_HZ, center=(e->ext_offset_ns+e->ext_base_ns)/1e9;
  int got=dts_match_search(sm,sd,1.0,center,3.0,&r);
  double span=t2-e->mt_t1;
  if(!got || !r.confident || fabs(span)<120.0) {
    printf("[ext-audio] match: second look %s\n",got && r.confident?"too close to the first":"not trusted");
    fflush(stdout);
    /* Checking all through: tried again at the next turn. */
    if(!e->mt_watch) e->mt_t1=-1;
    else if(fabs(span)>=120.0) e->mt_t1=t2;
    return;
  }
  double drift=(r.offset_s-e->mt_o1)/span;
  printf("[ext-audio] match: second look %.0f s later: offset %.3f s (first %.3f s), drift %.1f ppm\n",
    span,r.offset_s-e->ext_base_ns/1e9,e->mt_o1-e->ext_base_ns/1e9,drift*1e6);
  fflush(stdout);
  double k=1.0/(1.0-drift);
  /* A speed change is only believed when the frame rates differ, or the
   * measure lands on a known film/TV ratio (24/23.976, 25/24, 25/23.976 and
   * back). Same frame rate and an odd ratio: the offset moved instead (a gap
   * longer in one release; owner's log 08/11, Chicago Fire: 0.5 s read as
   * 1645 ppm, the other version reopened at that speed and back again). */
  int std=known_speed(k), same_fps=e->ext_fps_ratio>0 && fabs(e->ext_fps_ratio-1.0)<1e-4;
  /* The same frame rate with the follow on: whatever is left is a gap, even
   * when it lands near a known ratio (the follow keeps the sync meanwhile). */
  if(e->mt_watch && (fabs(drift)<0.0002 || (same_fps && (!std || e->fw_small)))) { match_watch_step(e,t2,&r); return; }
  e->mt_t1=-1;
  if(fabs(drift)<0.0002) return;   /* the same speed: 0.7 s an hour at most */
  if(same_fps && !std) {
    printf("[ext-audio] match: second look: the same frame rate, so not a speed difference: offset moved to %.3f s\n",
           r.offset_s-e->ext_base_ns/1e9);
    /* A version whose gaps differ: checked all through from here (owner's
     * log 16/11: this find was dropped and the offset stayed 0.85 s off). */
    e->mt_watch=1; e->mt_same=0; e->mt_stepped=1;
    printf("[ext-audio] match: the offset moved, so the sync is checked all through from now on\n");
    fflush(stdout);
    match_report_later(e,r.offset_s,r.score);
    e->mt_t1=t2; e->mt_o1=r.offset_s;
    return;
  }
  /* The frame rates name the exact ratio when they agree with the measure. */
  if(e->ext_fps_ratio>0 && fabs(e->ext_fps_ratio-1.0)>1e-5 && fabs((e->ext_fps_ratio-k)/(k-1.0))<0.3) k=e->ext_fps_ratio;
  /* this file = other * k + c, through the second find */
  double c=t2-(t2-r.offset_s)*k;
  match_report(e,c,k,r.score,5);
}
static void match_finish(DtsEngine *e) {
  struct MatchCap *mc=e->mt;
  DtsMatchSig sm, sd;
  match_side_sig(&mc->s[0],&sm); match_side_sig(&mc->s[1],&sd);
  if(e->mt_check) { match_second_look(e,&sm,&sd); match_disarm(e); return; }
  double base=e->ext_base_ns/1e9, center=e->ext_offset_ns/1e9+base, k0=ext_k(e);
  if(mc->alt_stream>=0) {   /* two tracks of this file: keep the one that compares better */
    DtsMatchSig sa; DtsMatchResult r0, ra;
    match_side_sig(&mc->s[2],&sa);
    int g0=dts_match_search(&sm,&sd,k0,center,10.0,&r0), ga=sa.n>0 && dts_match_search(&sa,&sd,k0,center,10.0,&ra);
    double s0=g0?r0.score+(r0.confident?1.0:0.0):-1.0, sa_=ga?ra.score+(ra.confident?1.0:0.0):-1.0;
    printf("[ext-audio] match: track %d scores %.2f, track %d scores %.2f\n",mc->main_stream,g0?r0.score:0.0,mc->alt_stream,ga?ra.score:0.0);
    if(sa_>s0) {
      int fm=-1, fd=-1, old=mc->main_stream;
      follow_streams(e,&fm,&fd);
      if(old!=fm) e->fmt->streams[old]->discard=AVDISCARD_ALL;
      sm=sa; mc->main_stream=mc->alt_stream;
      printf("[ext-audio] match: comparing with this file's track %d from now on\n",mc->main_stream);
    }
    fflush(stdout);
    e->mt_main_pref=mc->main_stream+1;
  }
  double ks[2]={k0,e->ext_fps_ratio}; int nk=e->ext_fps_ratio>0 && fabs(e->ext_fps_ratio-k0)>0.0005?2:1;
  DtsMatchResult best={0}; double bk=k0; int found=0;
  e->mt_ncand=0;
  for(int q=0;q<nk;q++) {
    DtsMatchResult r;
    int got=dts_match_search(&sm,&sd,ks[q],center,10.0,&r);
    if(got && !r.confident) match_try_verify(&sm,&sd,ks[q],&r);
    if(got && ks[q]==k0) match_set_cands(e,r.ncand,r.cand_off,r.cand_score);
    printf("[ext-audio] match: ratio %.5f: %s offset %d ms (was %d) score %.2f margin %.2f\n",ks[q],
      got && r.confident?"found":"not trusted",got?(int)lround(r.offset_s*1000.0-base*1000.0):0,
      (int)(e->ext_offset_ns/1000000),got?r.score:0.0,got?r.margin:0.0);
    if(got && r.confident && (!found || r.score>best.score)) { best=r; bk=ks[q]; found=1; }
  }
  fflush(stdout);
  if(found) {
    match_report(e,best.offset_s,bk==1.0?0:bk,best.score,1);
    if(bk==1.0) { e->mt_t1=sm.t0+sm.n/2.0/DTS_MATCH_HZ; e->mt_o1=best.offset_s;
                  e->mt_o1_user_ns=(int64_t)e->mt_offset_ms*1000000; }
    match_disarm(e); return;
  }
  /* Not near: a reader of its own tries further. */
  struct MatchJob *j=e->ext_url && e->ext_stream>=0 && e->ext?calloc(1,sizeof *j):NULL;
  if(j) {
    pthread_mutex_init(&j->lock,NULL);
    j->url=strdup(e->ext_url); j->headers=strdup(e->ext_headers?e->ext_headers:"");
    j->stream=mc->donor_stream>=0?mc->donor_stream:e->ext_stream; j->mn=sm.n; j->mt0=sm.t0;
    /* The passive candidates go on with the wider search's. */
    for(int c=0;c<e->mt_ncand;c++)
      match_cand_add(&j->ncand,j->cand,j->cand_sc,(e->mt_cand_ms[c]*1e6+e->ext_base_ns)/1e9,e->mt_cand_sc[c]/100.0);
    int ok=j->url && j->headers;
    for(int b=0;b<DTS_MATCH_BANDS;b++) if(sm.env[b] && ok) {
      if(!(j->menv[b]=malloc(sizeof(float)*(size_t)sm.n))) ok=0;
      else memcpy(j->menv[b],sm.env[b],sizeof(float)*(size_t)sm.n);
    }
    j->center=center; j->base=base; j->k0=k0; j->kf=e->ext_fps_ratio; j->cut_s=e->ext_cut_s;
    j->main_dur=e->fmt->duration>0?e->fmt->duration/1e6:0;
    j->donor_dur=e->ext->fmt && e->ext->fmt->duration>0?e->ext->fmt->duration/1e6:0;
    j->donor_total=e->ext->total;
    if(ok && pthread_create(&j->th,NULL,match_job_run,j)==0) { j->started=1; e->mj=j; }
    else match_job_free(j);
  }
  if(!e->mj) match_fail(e,0);
  match_disarm(e);
}
/* A first look at 25 s of the 40 (owner 16/11: after a new episode or the
 * intro skip the sync came late). A 12 s window in the middle of what is
 * captured reaches +-6 s around the current offset; taken only when very
 * clear (the window is shorter, a lucky peak likelier), else the capture goes
 * on to 40 s as before. Only for versions at most 10 s apart (another cut
 * keeps the 40 s: its follow takes its bar from this score, and a dub's
 * 12 s can score far above its 40 s), and not for a second look, two tracks
 * of this file to choose from, or two speeds to try. */
#define MATCH_EARLY_S 25
static int match_early(DtsEngine *e) {
  struct MatchCap *mc=e->mt;
  DtsMatchSig sm, sd; DtsMatchResult r;
  double base=e->ext_base_ns/1e9, center=e->ext_offset_ns/1e9+base, k0=ext_k(e);
  if(e->ext_fps_ratio>0 && fabs(e->ext_fps_ratio-k0)>0.0005) return 0;
  match_side_sig(&mc->s[0],&sm); match_side_sig(&mc->s[1],&sd);
  if(!dts_match_search_at(&sm,&sd,k0,center,6.0,12.0,0.5,&r)) return 0;
  int ok=r.score>=0.55 && r.margin>=0.35;
  printf("[ext-audio] match: early look at %d s: %s offset %d ms (was %d) score %.2f margin %.2f\n",MATCH_EARLY_S,
         ok?"found":"not sure yet,",(int)lround(r.offset_s*1000.0-base*1000.0),(int)(e->ext_offset_ns/1000000),r.score,r.margin);
  fflush(stdout);
  if(!ok) return 0;
  e->mt_ncand=0;
  match_report(e,r.offset_s,k0==1.0?0:k0,r.score,1);
  if(k0==1.0) { e->mt_t1=sm.t0+sm.n/2.0/DTS_MATCH_HZ; e->mt_o1=r.offset_s;
                e->mt_o1_user_ns=(int64_t)e->mt_offset_ms*1000000; }
  match_disarm(e);
  return 1;
}
static void match_feed(DtsEngine *e,int side,AVPacket *pkt,AVRational tb) {
  struct MatchCap *mc=e->mt;
  if(!mc || match_side_full(&mc->s[side])) return;
  if(match_side_feed(&mc->s[side],pkt,tb)<0) { mc->s[side].ev.max=mc->s[side].ev.n=1; }
  if(!mc->early && !e->mt_check && (mc->alt_stream<0 || mc->alt_same) && abs(e->ext_cut_s)<=10 &&
     mc->s[0].ev.n>=MATCH_EARLY_S*DTS_MATCH_HZ && mc->s[1].ev.n>=MATCH_EARLY_S*DTS_MATCH_HZ &&
     !match_side_full(&mc->s[0]) && !match_side_full(&mc->s[1])) {
    mc->early=1;
    if(match_early(e)) return;
  }
  if(match_side_full(&mc->s[0]) && match_side_full(&mc->s[1]) && (mc->alt_stream<0 || match_side_full(&mc->s[2])))
    match_finish(e);
}
/* An interrupted capture (seek, live offset) is not used; a search already
 * reading on its own goes on (this file's capture is kept with it). */
static void match_cancel(DtsEngine *e) {
  if(!e->mt) return;
  int check=e->mt_check;
  match_disarm(e);
  if(check) return;   /* the second look is armed again later */
  /* An automatic capture cut short by a jump (owner's log 11/11, S9E16: the
   * automatic intro skip right at the start) is no failure to report: it
   * starts again from where playback went, up to 3 times. A click still
   * reports its interruption. */
  if(e->mt_auto) {
    e->mt_auto=0; e->mt_state=0;
    if(e->mt_retry<3) { e->mt_retry++; e->fw_autoarm=1; }
    printf("[ext-audio] match: interrupted by a jump (automatic)%s\n",e->fw_autoarm?": again from there":"");
    fflush(stdout);
    return;
  }
  e->mt_state=3; e->mt_offset_ms=e->mt_score=0; e->mt_how=e->mt_hint_ms=e->mt_heavy=0;
  printf("[ext-audio] match: interrupted\n"); fflush(stdout);
}
/* Tracks worth comparing: decodable audio, never a commentary. Rank: the
 * lighter to decode the better (lossy, then DTS core, then the rest). */
static int match_rank(AVStream *st) {
  AVCodecParameters *p=st->codecpar;
  char title[64]="";
  if(p->codec_type!=AVMEDIA_TYPE_AUDIO || !avcodec_find_decoder(p->codec_id)) return 0;
  copy_metadata(title,sizeof title,st->metadata,"title");
  for(char *c=title;*c;c++) *c=(char)tolower((unsigned char)*c);
  if((st->disposition&AV_DISPOSITION_COMMENT) || strstr(title,"comment") || strstr(title,"komentarz")) return 0;
  return p->codec_id==AV_CODEC_ID_AC3 || p->codec_id==AV_CODEC_ID_EAC3 || p->codec_id==AV_CODEC_ID_AAC ||
         p->codec_id==AV_CODEC_ID_MP3 || p->codec_id==AV_CODEC_ID_OPUS || p->codec_id==AV_CODEC_ID_VORBIS?3:
         p->codec_id==AV_CODEC_ID_DTS?2:1;
}
/* The best track of `f` in language `lang` ("" any), -1 none. */
static int match_track_lang(AVFormatContext *f,const char *lang) {
  int best=-1, rank=0;
  for(unsigned i=0;i<f->nb_streams;i++) {
    char l[16]="";
    int r=match_rank(f->streams[i]);
    if(!r) continue;
    copy_metadata(l,sizeof l,f->streams[i]->metadata,"language");
    if(lang[0] && !ext_lang_is(l,lang) && !ext_lang_is(lang,l) && strcasecmp(l,lang)) continue;
    if(r>rank) { rank=r; best=(int)i; }
  }
  return best;
}
/* WHAT TO COMPARE (owner's log 07/11: this file's English against the other
 * version's Polish voice-over barely correlated, 7-16 %): the same language
 * in both files when there is one, whatever track plays. The other
 * version's language first (this file may have it), then this file's
 * default language in the other version, then any language both have.
 * `alt` 0: the other version's playing track only (its other tracks are
 * not read: an MP4 read by its audio chunks alone). */
static void match_pair(DtsEngine *e,int alt,int *mains,int *donor,int *shared) {
  AVFormatContext *mf=e->fmt, *df=e->ext->fmt;
  char ld[16]="", lm[16]="";
  int m=-1, d=e->ext_stream, def=-1;
  copy_metadata(ld,sizeof ld,df->streams[e->ext_stream]->metadata,"language");
  if(ld[0]) m=match_track_lang(mf,ld);
  for(unsigned i=0;i<mf->nb_streams && def<0;i++)
    if(match_rank(mf->streams[i]) && (mf->streams[i]->disposition&AV_DISPOSITION_DEFAULT)) def=(int)i;
  if(def<0) def=match_track_lang(mf,"");
  if(m<0 && alt && def>=0) {
    copy_metadata(lm,sizeof lm,mf->streams[def]->metadata,"language");
    int k=lm[0]?match_track_lang(df,lm):-1;
    /* The lightest track of that language (AC-3 before TrueHD: owner's log
     * 07/11 decoded the TrueHD, twice the CPU). */
    if(k>=0) { m=match_track_lang(mf,lm); if(m<0) m=def; d=k; }
  }
  for(unsigned i=0;i<mf->nb_streams && m<0 && alt;i++) {
    char l[16]="";
    if(!match_rank(mf->streams[i])) continue;
    copy_metadata(l,sizeof l,mf->streams[i]->metadata,"language");
    int k=l[0]?match_track_lang(df,l):-1;
    if(k>=0) { m=(int)i; d=k; }
  }
  if(shared) *shared=m>=0;
  /* A track of this file in the same language that already compared better
   * (match_alt_same) is the one compared from then on. */
  if(m>=0) {
    int p=e->mt_main_pref-1;
    char lp[16]="", lm2[16]="";
    if(p>=0 && p!=m && (unsigned)p<mf->nb_streams && match_rank(mf->streams[p])) {
      copy_metadata(lp,sizeof lp,mf->streams[p]->metadata,"language");
      copy_metadata(lm2,sizeof lm2,mf->streams[m]->metadata,"language");
      if(!strcasecmp(lp,lm2) || ext_lang_is(lp,lm2)) m=p;
    }
  }
  if(m<0) {   /* no shared language: the lightest track of the default one */
    char l[16]="";
    if(def>=0) copy_metadata(l,sizeof l,mf->streams[def]->metadata,"language");
    m=l[0]?match_track_lang(mf,l):-1;
    if(m<0) m=def;
    /* ...unless another track of this file already compared better. */
    int p=e->mt_main_pref-1;
    if(p>=0 && (unsigned)p<mf->nb_streams && match_rank(mf->streams[p])) m=p;
  }
  *mains=m; *donor=d;
}
static int lang_is_en(const char *l) {
  return l && l[0] && (!strcasecmp(l,"en") || !strcasecmp(l,"eng") || ext_lang_is(l,"en") || ext_lang_is(l,"eng"));
}
/* NO SHARED LANGUAGE (owner's log 08/11: The Godfather compared its Russian
 * dub with a Polish voice-over; a voice-over usually sits on the original,
 * often English, track): a second track of this file in another language
 * is captured too, English first, and the better one is kept. -1 none. */
static int match_alt(DtsEngine *e,int m) {
  AVFormatContext *mf=e->fmt;
  char lm[16]="";
  int best=-1, br=0;
  if(m<0) return -1;
  copy_metadata(lm,sizeof lm,mf->streams[m]->metadata,"language");
  for(unsigned i=0;i<mf->nb_streams;i++) {
    char l[16]="";
    int r=match_rank(mf->streams[i]);
    if(!r || (int)i==m) continue;
    copy_metadata(l,sizeof l,mf->streams[i]->metadata,"language");
    if(!l[0] || (lm[0] && (!strcasecmp(l,lm) || ext_lang_is(l,lm)))) continue;   /* another language only */
    r+=lang_is_en(l)?10:0;
    if(r>br) { br=r; best=(int)i; }
  }
  return best;
}
/* THE SAME LANGUAGE, ANOTHER TRACK (owner's log 21/11, Farscape 4K: its
 * English "Stereo" AC-3, the lightest, scored 0.07 against the Blu-ray's
 * English, as against a Polish voice-over, while the audio played in sync):
 * a second track of this file in the language compared, another codec or
 * channel count first, is captured too and the better one is kept. -1 none. */
static int match_alt_same(DtsEngine *e,int m) {
  AVFormatContext *mf=e->fmt;
  char lm[16]="";
  int best=-1, br=0;
  if(m<0) return -1;
  const AVCodecParameters *pm=mf->streams[m]->codecpar;
  copy_metadata(lm,sizeof lm,mf->streams[m]->metadata,"language");
  for(unsigned i=0;i<mf->nb_streams;i++) {
    char l[16]="";
    int r=match_rank(mf->streams[i]);
    if(!r || (int)i==m) continue;
    copy_metadata(l,sizeof l,mf->streams[i]->metadata,"language");
    if(lm[0] ? (!l[0] || (strcasecmp(l,lm) && !ext_lang_is(l,lm))) : l[0]!=0) continue;
    const AVCodecParameters *pi=mf->streams[i]->codecpar;
    r+=pi->codec_id!=pm->codec_id?10:0;
    r+=pi->ch_layout.nb_channels!=pm->ch_layout.nb_channels?5:0;
    if(r>br) { br=r; best=(int)i; }
  }
  return best;
}
static int match_arm(DtsEngine *e,int check) {
  if(!e || !e->ext || !e->ext->fmt || !e->fmt || e->ext_stream<0) return -1;
  match_disarm(e);
  int best=-1, donor=e->ext_stream, shared=0;
  match_pair(e,e->ext->sp_n<=0,&best,&donor,&shared);
  if(best<0) return -1;
  struct MatchCap *mc=av_mallocz(sizeof *mc);
  if(!mc) return -1;
  mc->main_stream=best; mc->donor_stream=donor; mc->alt_stream=-1; e->mt=mc;
  if(match_side_open(&mc->s[0],e->fmt->streams[best],DTS_MATCH_MAX)<0 ||
     match_side_open(&mc->s[1],e->ext->fmt->streams[donor],DTS_MATCH_MAX)<0) { match_disarm(e); return -1; }
  e->fmt->streams[best]->discard=AVDISCARD_DEFAULT;
  if(shared && !check && !e->mt_main_pref) {
    int a=match_alt_same(e,best);
    if(a>=0 && match_side_open(&mc->s[2],e->fmt->streams[a],DTS_MATCH_MAX)==0) {
      char la[16]="";
      mc->alt_stream=a; mc->alt_same=1; e->fmt->streams[a]->discard=AVDISCARD_DEFAULT;
      copy_metadata(la,sizeof la,e->fmt->streams[a]->metadata,"language");
      printf("[ext-audio] match: also trying this file's track %d (%s %s), the same language\n",a,
             avcodec_get_name(e->fmt->streams[a]->codecpar->codec_id),la[0]?la:"-");
    }
  }
  if(!shared && !check && !e->mt_main_pref) {
    int a=match_alt(e,best), same=0;
    /* No other language in this file (owner's log 21/11: a Polish AVI with
     * no language tag against a 4K file with three English tracks): another
     * track of the same one, as with a shared language. */
    if(a<0) { a=match_alt_same(e,best); same=a>=0; }
    if(a>=0 && match_side_open(&mc->s[2],e->fmt->streams[a],DTS_MATCH_MAX)==0) {
      char la[16]="";
      mc->alt_stream=a; mc->alt_same=same; e->fmt->streams[a]->discard=AVDISCARD_DEFAULT;
      copy_metadata(la,sizeof la,e->fmt->streams[a]->metadata,"language");
      printf("[ext-audio] match: no shared language, also trying this file's track %d (%s %s)%s\n",a,
             avcodec_get_name(e->fmt->streams[a]->codecpar->codec_id),la,same?", the same language":"");
    }
  }
  if(donor!=e->ext_stream) e->ext->fmt->streams[donor]->discard=AVDISCARD_DEFAULT;
  e->mt_check=check;
  { char lm[16]="", ld[16]="";
    copy_metadata(lm,sizeof lm,e->fmt->streams[best]->metadata,"language");
    copy_metadata(ld,sizeof ld,e->ext->fmt->streams[donor]->metadata,"language");
    printf("[ext-audio] match: %scapturing %d s of this file's track %d (%s %s) and the other version's track %d (%s %s%s)\n",
      check?"second look, ":"",DTS_MATCH_SECONDS,best,avcodec_get_name(e->fmt->streams[best]->codecpar->codec_id),lm[0]?lm:"-",
      donor,avcodec_get_name(e->ext->fmt->streams[donor]->codecpar->codec_id),ld[0]?ld:"-",
      donor!=e->ext_stream?", compared only":""); }
  fflush(stdout);
  return 0;
}
int dts_engine_external_match_arm(DtsEngine *e) {
  if(!e) return -1;
  match_job_stop(e); e->mt_t1=-1; e->mt_auto=0;   /* a click; the automatic callers set it after */
  e->mt_heavy=e->mt_hint_ms=e->mt_how=e->mt_ncand=0;
  if(match_arm(e,0)<0) { match_fail(e,0); return -1; }
  e->mt_state=1; e->mt_started_ms=clock_ns(CLOCK_MONOTONIC)/1000000u;
  return 0;
}
/* The second look starts on its own once playback is 5 minutes away from
 * the first find (called with this file's picture time). */
static void match_second_look_due(DtsEngine *e,int64_t t_ns) {
  if(e->mt_t1<0 || e->mt || e->mj || t_ns==INT64_MIN) return;
  /* Checking all through: 3 min, 2 once the offset has moved (its moves come
   * at each break), 6 once it held twice. */
  double gap=e->mt_watch?(e->mt_same>=2?360.0:e->mt_stepped?120.0:180.0):300.0;
  if(fabs(t_ns/1e9-e->mt_t1)<gap) return;
  if(match_arm(e,1)<0) e->mt_t1=-1;
}
int dts_engine_external_match_poll(DtsEngine *e,DtsMatchInfo *o) {
  memset(o,0,sizeof *o);
  if(!e) return -1;
  if(e->mj) {
    struct MatchJob *j=e->mj; int done;
    pthread_mutex_lock(&j->lock); done=j->done; pthread_mutex_unlock(&j->lock);
    if(done) {
      if(j->found) {
        match_report(e,j->off,j->ratio==1.0?0:j->ratio,j->score,j->how);
        if(j->ratio==1.0) { e->mt_t1=j->mt0+j->mn/2.0/DTS_MATCH_HZ; e->mt_o1=j->off;
                            e->mt_o1_user_ns=(int64_t)e->mt_offset_ms*1000000; }
      } else match_fail(e,j->heavy);
      match_set_cands(e,j->ncand,j->cand,j->cand_sc);
      printf("[ext-audio] match: wider search %s\n",j->found?"found it":"found nothing trusted");
      for(int c=0;c<e->mt_ncand;c++) printf("[ext-audio] match: candidate %d: %d ms (score %d)\n",c+1,e->mt_cand_ms[c],e->mt_cand_sc[c]);
      fflush(stdout);
      pthread_join(j->th,NULL); j->started=0;
      match_job_free(j); e->mj=NULL;
    } else return 0;
  }
  if(e->mt_state==1 && e->mt && !e->mt_check && clock_ns(CLOCK_MONOTONIC)/1000000u-e->mt_started_ms>90000u) match_cancel(e);  /* paused or stuck */
  if(e->mt_state==2 || e->mt_state==3) {
    o->automatic=e->mt_auto; if(e->mt_state==3 || !e->mt) e->mt_auto=0;
    o->ncand=e->mt_ncand;
    for(int c=0;c<e->mt_ncand;c++) { o->cand_ms[c]=e->mt_cand_ms[c]; o->cand_score[c]=e->mt_cand_sc[c]; }
  }
  if(e->mt_state==2) {
    o->offset_ms=e->mt_offset_ms; o->score=e->mt_score; o->ratio_ppm=e->mt_ratio_ppm; o->how=e->mt_how;
    o->later=e->mt_later; e->mt_later=0;
    e->mt_state=0; return 1;
  }
  if(e->mt_state==3) {
    e->mt_later=0;
    o->score=e->mt_score; o->hint_ms=e->mt_hint_ms; o->too_heavy=e->mt_heavy;
    e->mt_state=0; return -1;
  }
  return 0;
}
/* FOLLOW ANOTHER CUT (Settings > Audio from another version > Follow
 * through the whole film; owner, 07/11). After a trusted find, both audios
 * keep being compared every 2 s over the last 8 s. When the other version
 * stops matching (a scene it does not have: an extended picture with the
 * theatrical audio), this file's own audio plays (FILL) while the other one
 * waits where it stopped; when the latest 10 s of this file show up in the
 * other version again (read ahead up to 2.5 min), it resumes from there with
 * the new offset. Needs the conversion path: the encoder switches source
 * without a reload. */
#define FW_MAIN_S 60
#define FW_LOOK_S 40
#define FW_LOOK_MAX_S 150
enum { FW_OFF=0, FW_SYNC=1, FW_FILL=2, FW_GAVEUP=3 };
struct Follow {
  int mstream, dstream;
  MatchSide m, d;          /* this file (rolling 60 s); the other version (rolling, or the lookahead in FILL) */
  double thr;              /* "in sync" score, from the find that started it */
  double base;             /* that find's score: low when the two languages differ */
  int bad, good_since_seek, checked;
  int need;                /* bad checks (2 s apart) before a scene counts as missing */
  int strict;              /* false alarms so far (the other version came back at the same offset) */
  int other;               /* the compared tracks are in two languages */
  int shift_n; double shift_s;   /* checks in a row that found the same small shift (raw offset, s) */
  int64_t fill_off_ns;     /* FILL: the offset it began with */
  double t_good;           /* this file's time of the last check in sync (s) */
  double d_pause;          /* FILL: the other version's time its content stopped matching (raw, s) */
  double fill_since;       /* FILL: this file's time it began (s) */
  int look_s;
  SwrContext *pcm;         /* this file's audio to the encoder's format */
};
static void follow_streams(DtsEngine *e,int *mains,int *donor) {
  *mains=e->fw?e->fw->mstream:-1; *donor=e->fw?e->fw->dstream:-1;
}
static void follow_event(DtsEngine *e,int kind,int ms) {
  e->fw_event=kind; e->fw_event_ms=ms;
  printf("[ext-audio] follow: %s\n",kind==1?"a scene the other version lacks: this file's own audio":
         kind==2?"shared scenes again: the other version resumes":
         kind==4?"a small shift (a shorter or longer gap in one version): sync adjusted":"lost it: use Match automatically");
  fflush(stdout);
}
static void follow_side_reset(MatchSide *m) {
  if(m->dec) avcodec_flush_buffers(m->dec);
  m->ev.n=0; m->ev.accn=0; for(int b=0;b<DTS_MATCH_BANDS;b++) m->ev.acc[b]=0;
  m->t0_ns=INT64_MIN;
}
/* Keep the last `keep` blocks of a side (a rolling window). */
static void follow_roll(MatchSide *m,int keep) {
  if(m->ev.n<=keep) return;
  int shift=m->ev.n-keep;
  for(int b=0;b<DTS_MATCH_BANDS;b++) if(m->ev.env[b]) memmove(m->ev.env[b],m->ev.env[b]+shift,sizeof(float)*(size_t)keep);
  m->ev.n=keep;
  if(m->t0_ns!=INT64_MIN) m->t0_ns+=(int64_t)shift*(1000000000/DTS_MATCH_HZ);
}
static void follow_free(DtsEngine *e) {
  struct Follow *f=e->fw;
  if(!f) return;
  match_side_free(&f->m); match_side_free(&f->d); swr_free(&f->pcm);
  free(f); e->fw=NULL; e->fw_state=FW_OFF;
}
static void follow_start(DtsEngine *e,double score) {
  if((!e->ext_conv && !e->fw_small) || !e->ext || !e->ext->fmt) { printf("[ext-audio] follow: needs the conversion path, off\n"); fflush(stdout); return; }
  struct Follow *f=e->fw;
  if(!f) {
    int ms=-1, ds=-1;
    match_pair(e,e->ext->sp_n<=0,&ms,&ds,NULL);
    if(ms<0 || ds<0 || !(f=calloc(1,sizeof *f))) return;
    f->mstream=ms; f->dstream=ds;
    if(match_side_open(&f->m,e->fmt->streams[ms],FW_MAIN_S*DTS_MATCH_HZ)<0 ||
       match_side_open(&f->d,e->ext->fmt->streams[ds],(FW_LOOK_MAX_S+FW_MAIN_S)*DTS_MATCH_HZ)<0) {
      match_side_free(&f->m); match_side_free(&f->d); free(f); return;
    }
    e->fmt->streams[ms]->discard=AVDISCARD_DEFAULT;
    e->ext->fmt->streams[ds]->discard=AVDISCARD_DEFAULT;
    e->fw=f;
  }
  f->thr=0.5*score; if(f->thr<0.12) f->thr=0.12;
  if(f->strict) { f->thr*=0.8; if(f->thr<0.06) f->thr=0.06; }
  f->base=score;
  /* Owner's log 08/11 (The Godfather, Russian against a Polish voice-over,
   * find 0.19): in another language the dialogue alone drops the score under
   * the bar, so two languages (or a weak find) wait ~10 s (and a longer
   * check) before calling a scene missing; every false alarm doubles that. */
  { char lm[16]="", ld[16]="";
    copy_metadata(lm,sizeof lm,e->fmt->streams[f->mstream]->metadata,"language");
    copy_metadata(ld,sizeof ld,e->ext->fmt->streams[f->dstream]->metadata,"language");
    int other=lm[0] && ld[0] && strcasecmp(lm,ld) && !ext_lang_is(lm,ld) && !ext_lang_is(ld,lm);
    f->other=other;
    f->need=(other || score<0.20?5:2)<<(f->strict>2?2:f->strict); }
  f->bad=0; f->good_since_seek=1; f->t_good=-1;
  if(e->fw_state!=FW_FILL) e->fw_state=FW_SYNC;
  printf("[ext-audio] follow: on%s (in sync from score %.2f, this file's track %d, the other's %d)\n",
         e->fw_small?", small shifts only":"",f->thr,f->mstream,f->dstream);
  fflush(stdout);
}
/* This file's decoded frame into the encoder's queue, in FILL: on the
 * output timeline (holes filled with silence, overlaps cut). */
static void follow_pcm_out(DtsEngine *e,AVFrame *fr,AVRational tb) {
  struct Follow *f=e->fw;
  int64_t p=fr->best_effort_timestamp!=AV_NOPTS_VALUE?fr->best_effort_timestamp:fr->pts;
  if(p==AV_NOPTS_VALUE || !e->enc || !e->fifo) return;
  if(!f->pcm && (swr_alloc_set_opts2(&f->pcm,&e->enc->ch_layout,e->enc->sample_fmt,48000,&fr->ch_layout,fr->format,fr->sample_rate,0,NULL)<0 ||
                 swr_init(f->pcm)<0)) { swr_free(&f->pcm); return; }
  int64_t t=ns(p,tb);
  int64_t end=ns(e->sample_pts+av_audio_fifo_size(e->fifo),e->enc->time_base)+e->ext_offset_ns+e->ext_base_ns;
  int64_t dur=(int64_t)fr->nb_samples*1000000000LL/(fr->sample_rate>0?fr->sample_rate:48000);
  if(t+dur<=end) return;   /* already given out */
  int ch=e->enc->ch_layout.nb_channels, bps=av_get_bytes_per_sample(e->enc->sample_fmt), planar=av_sample_fmt_is_planar(e->enc->sample_fmt);
  if(t>end+20000000LL) {   /* a hole: silence up to this frame */
    int gap=(int)((t-end)*48/1000000);
    if(gap>48000*2) gap=48000*2;
    uint8_t **z=NULL;
    if(av_samples_alloc_array_and_samples(&z,NULL,ch,gap,e->enc->sample_fmt,0)>=0) {
      av_samples_set_silence(z,0,gap,ch,e->enc->sample_fmt);
      av_audio_fifo_write(e->fifo,(void **)z,gap);
      av_freep(&z[0]); av_freep(&z);
    }
    end=ns(e->sample_pts+av_audio_fifo_size(e->fifo),e->enc->time_base)+e->ext_offset_ns+e->ext_base_ns;
  }
  int cap=(int)av_rescale_rnd(fr->nb_samples,48000,fr->sample_rate>0?fr->sample_rate:48000,AV_ROUND_UP)+256;
  uint8_t **data=NULL;
  if(av_samples_alloc_array_and_samples(&data,NULL,ch,cap,e->enc->sample_fmt,0)<0) return;
  int got=swr_convert(f->pcm,data,cap,(const uint8_t **)fr->extended_data,fr->nb_samples);
  int skip=t<end?(int)((end-t)*48/1000000):0;
  if(got>skip) {
    uint8_t *ptr[8];
    for(int c=0;c<(planar?ch:1) && c<8;c++) ptr[c]=data[c]+(size_t)skip*bps*(planar?1:ch);
    av_audio_fifo_write(e->fifo,(void **)ptr,got-skip);
  }
  av_freep(&data[0]); av_freep(&data);
}
static void follow_enter_fill(DtsEngine *e,double t_good) {
  struct Follow *f=e->fw;
  int64_t out=e->ext_last_end_ns!=INT64_MIN?e->ext_last_end_ns:e->ext_clock_ns;
  if(out==INT64_MIN || !e->ext_conv) return;
  double k=ext_k(e), off=(e->ext_offset_ns+e->ext_base_ns)/1e9;
  f->d_pause=(t_good-off)/k;
  if(f->d_pause<0) f->d_pause=0;
  /* The other version waits where it stopped matching: read ahead from there. */
  int how=0;
  if(ext_donor_seek(e,f->d_pause>2?f->d_pause-2:0,&how)<0 || ext_reset_chain(e)<0) return;
  e->timeline=1;
  e->sample_pts=av_rescale_q(out-e->ext_offset_ns-e->ext_base_ns,NS,e->enc->time_base);
  e->ext_floor_ns=out;
  follow_side_reset(&f->d);
  f->look_s=FW_LOOK_S; f->fill_since=out/1e9; f->bad=0; f->fill_off_ns=e->ext_offset_ns;
  e->fw_state=FW_FILL;
  printf("[ext-audio] follow: own audio from %.1fs, the other version waits at %.1fs\n",out/1e9,f->d_pause); fflush(stdout);
  follow_event(e,1,(int)(e->ext_offset_ns/1000000));
}
static void follow_resume(DtsEngine *e,int64_t offset_ns) {
  struct Follow *f=e->fw;
  int64_t from=e->ext_last_end_ns!=INT64_MIN?e->ext_last_end_ns:e->ext_clock_ns;
  if(offset_ns>(int64_t)DTS_ENGINE_EXTERNAL_OFFSET_MAX_MS*1000000) offset_ns=(int64_t)DTS_ENGINE_EXTERNAL_OFFSET_MAX_MS*1000000;
  if(offset_ns<-(int64_t)DTS_ENGINE_EXTERNAL_OFFSET_MAX_MS*1000000) offset_ns=-(int64_t)DTS_ENGINE_EXTERNAL_OFFSET_MAX_MS*1000000;
  e->ext_offset_ns=offset_ns;
  if(from==INT64_MIN) from=e->target_ns;
  double t=(from-e->ext_offset_ns-e->ext_base_ns)/1e9/ext_k(e)-1.0;
  int how=0;
  if(t<0) t=0;
  if(ext_donor_seek(e,t,&how)<0 || ext_reset_chain(e)<0) return;
  e->ext_floor_ns=from;
  if(f) {
    follow_side_reset(&f->d); f->bad=0; f->good_since_seek=1;
    /* Back at the offset it left after a short while: nothing was missing
     * (dialogue in another language). Stricter from now on. */
    if(e->fw_state==FW_FILL && llabs(offset_ns-f->fill_off_ns)<80000000LL && from/1e9-f->fill_since<120.0 && f->strict<3) {
      f->strict++;
      f->need*=2; f->thr*=0.8; if(f->thr<0.06) f->thr=0.06;
      printf("[ext-audio] follow: false alarm (same offset again): now waits for %d checks, bar %.2f\n",f->need,f->thr);
    }
  }
  e->fw_state=FW_SYNC;
  printf("[ext-audio] follow: the other version resumes at %.1fs, offset %lld ms\n",from/1e9,(long long)(offset_ns/1000000)); fflush(stdout);
  follow_event(e,2,(int)(offset_ns/1000000));
}
/* Every 3 s of this file: still in sync? (SYNC) / back in shared scenes? (FILL) */
/* A small shift the follow found: at once, or at a pause in small mode.
 * The same aim asked again while waiting goes on waiting. */
static int follow_shift(DtsEngine *e,int ms) {
  if(e->fw_small && e->qs && abs(e->qs_offset_ms-ms)<40) return 1;   /* already on its way: no new event */
  /* The checks' first find moves with the follow: a shift it made is no
   * drift (owner's log 16/11: a +420 ms shift between two checks read as
   * 1083 ppm, close to 24/23.976, and the other version was reopened at that
   * speed and back again: two reloads). Moved before the shift, so the
   * manual-change test in external_offset_set sees it as ours. */
  int from=e->qs?e->qs_offset_ms:(int)(e->ext_offset_ns/1000000);
  if(e->mt_t1>=0) { e->mt_o1+=(ms-from)/1000.0; e->mt_o1_user_ns+=(int64_t)(ms-from)*1000000; }
  if(e->fw_small) return dts_engine_external_shift_quiet(e,ms);
  return dts_engine_external_shift(e,ms);
}
static void follow_check(DtsEngine *e) {
  struct Follow *f=e->fw;
  DtsMatchSig sm, sd;
  match_side_sig(&f->m,&sm); match_side_sig(&f->d,&sd);
  if(sm.n<11*DTS_MATCH_HZ || f->m.t0_ns==INT64_MIN || f->d.t0_ns==INT64_MIN) return;
  /* In sync: the last 8 s (a quick "lost"); back: the last 10 s (surer). */
  double win=8.0, pos=(sm.n-4.0*DTS_MATCH_HZ)/sm.n, k=ext_k(e), off=(e->ext_offset_ns+e->ext_base_ns)/1e9;
  double tend=sm.t0+(double)sm.n/DTS_MATCH_HZ;
  DtsMatchResult r;
  if(e->fw_state==FW_SYNC) {
    if(!dts_match_search_at(&sm,&sd,k,off,1.0,win,pos,&r)) return;   /* not covered (yet) or silent */
    double dd=r.offset_s-off;
    int here=fabs(dd)<0.25;
    /* Small mode (versions 0.5-10 s apart) moves only for a real gap: at
     * least 200 ms and a clear score (owner's log 17/11, Stargate SG-1: a
     * weak match, 0.23, set the bar at 0.12, and noise moved the sync
     * +102 ms; the real gaps seen were 330-600 ms at 0.48-0.76). */
    double near=e->fw_small?0.20:0.10, need=e->fw_small?fmax(f->thr,0.35):f->thr;
    if(r.score>=f->thr && fabs(dd)<near) { f->bad=0; f->good_since_seek=1; f->t_good=tend-8.0; f->shift_n=0; return; }
    /* Owner's log 08/11 (Chicago Fire, two episode releases): the sync was
     * right, then 0.6 s off a few minutes on (a gap a little longer in one
     * of them) until matched again by hand. A clear peak at another offset
     * up to ~1 s away, the same in two checks in a row: moved there live. */
    if(r.score>=need && r.margin>=0.10 && fabs(dd)<=0.95) {
      if(f->shift_n && fabs(r.offset_s-f->shift_s)<0.04) f->shift_n++;
      else { f->shift_n=1; f->shift_s=r.offset_s; }
      f->bad=0;
      if(f->shift_n>=2) {
        int ms=(int)lround((r.offset_s*1e9-e->ext_base_ns)/1e6);
        f->shift_n=0;
        printf("[ext-audio] follow: shifted %+d ms (score %.2f): offset %d ms\n",(int)lround(dd*1000.0),r.score,ms);
        if(follow_shift(e,ms)==0) { f->good_since_seek=1; f->t_good=-1; follow_event(e,4,ms); }
      }
      return;
    }
    f->shift_n=0;
    if(r.score>=f->thr && here) { f->bad=0; f->good_since_seek=1; f->t_good=tend-8.0; return; }
    /* Weak, but the peak is still at this offset: in sync (quiet scene, or
     * dialogue in another language over the same music and effects). */
    if(fabs(r.offset_s-off)<0.08 && r.score>=0.04 && r.margin>=(f->other?0.025:0.05)) { f->bad=0; f->t_good=tend-8.0; return; }
    if(r.score>=f->thr*0.7) return;   /* unclear: neither */
    if(++f->bad<f->need) return;
    /* Before switching: the whole bad stretch at once (longer = surer). Still
     * peaking at this offset = not a missing scene. */
    { double cw=2.0*f->bad+4.0; if(cw>16.0) cw=16.0;
      DtsMatchResult c;
      if(sm.n>=(cw+3.0)*DTS_MATCH_HZ) {
        int got=dts_match_search_at(&sm,&sd,k,off,1.5,cw,(sm.n-(cw/2.0+1.0)*DTS_MATCH_HZ)/sm.n,&c);
        /* Two languages: a weak peak right here is enough (the effects and
         * music line up, the speech does not). */
        if(got && fabs(c.offset_s-off)<0.08 &&
           (f->other?c.score>=0.05 && c.margin>=0.02:c.score>=fmax(0.05,f->thr*0.4) && c.margin>=0.04)) {
          printf("[ext-audio] follow: still in sync over %.0f s (score %.2f): not a missing scene\n",cw,c.score); fflush(stdout);
          f->bad=0; f->t_good=tend-8.0; return;
        }
        /* ...or clearly at another offset close by (a gap a little longer
         * or shorter in one version): moved there instead of switching. */
        if(got && fabs(c.offset_s-off)>=near && fabs(c.offset_s-off)<=1.4 && c.score>=(e->fw_small?need:f->thr*0.8) && c.margin>=0.08) {
          int ms=(int)lround((c.offset_s*1e9-e->ext_base_ns)/1e6);
          printf("[ext-audio] follow: shifted %+d ms over %.0f s (score %.2f): offset %d ms\n",
                 (int)lround((c.offset_s-off)*1000.0),cw,c.score,ms);
          f->bad=0; f->shift_n=0;
          if(follow_shift(e,ms)==0) { f->good_since_seek=1; f->t_good=-1; follow_event(e,4,ms); }
          return;
        }
        if(!got && f->bad<f->need*2) return;   /* not covered: look again */
      }
    }
    /* Small shifts only: no own audio; the checks minutes apart still run. */
    if(e->fw_small) { f->bad=0; return; }
    printf("[ext-audio] follow: no longer in sync (score %.2f, needs %.2f)\n",r.score,f->thr); fflush(stdout);
    if(!f->good_since_seek || f->t_good<0) {
      /* Lost right after a jump: the place is unknown, search it all. */
      follow_enter_fill(e,tend-8.0);
      if(!e->mt && !e->mj) { dts_engine_external_match_arm(e); e->mt_auto=1; }
      return;
    }
    follow_enter_fill(e,f->t_good);
    return;
  }
  if(e->fw_state!=FW_FILL || sd.n<15*DTS_MATCH_HZ) return;
  win=10.0; pos=(sm.n-5.0*DTS_MATCH_HZ)/sm.n;
  double tmid=tend-5.0, dlo=sd.t0, dhi=sd.t0+(double)sd.n/DTS_MATCH_HZ;
  double omin=tmid-(dhi-7.0)*k, omax=tmid-(dlo+7.0)*k;
  if(omax>omin && dts_match_search_at(&sm,&sd,k,(omin+omax)/2,(omax-omin)/2,win,pos,&r) &&
     r.score>=f->thr && r.margin>=0.08) {
    follow_resume(e,llrint(r.offset_s*1e9)-e->ext_base_ns);
    return;
  }
  if(sd.n>=(f->look_s-2)*DTS_MATCH_HZ && f->look_s<FW_LOOK_MAX_S) f->look_s+=20;   /* not there yet: read further */
  if(tend-f->fill_since>FW_LOOK_MAX_S+60.0) {
    e->fw_state=FW_GAVEUP;
    follow_event(e,3,(int)(e->ext_offset_ns/1000000));
  }
}
/* This file's track the follow compares (from the demux loop). */
static void follow_feed_main(DtsEngine *e,AVPacket *pkt,AVRational tb) {
  struct Follow *f=e->fw;
  MatchSide *m=&f->m;
  if(m->ev.max && m->ev.n>=m->ev.max-3*DTS_MATCH_HZ) { follow_roll(m,m->ev.max*2/3); f->checked=0; }
  if(!m->dec || avcodec_send_packet(m->dec,pkt)<0) return;
  while(avcodec_receive_frame(m->dec,m->fr)==0) {
    if(e->fw_state==FW_FILL || e->fw_state==FW_GAVEUP) follow_pcm_out(e,m->fr,tb);
    match_side_frame(m,m->fr,tb);
  }
  if(m->ev.n-f->checked>=2*DTS_MATCH_HZ) { f->checked=m->ev.n; follow_check(e); }
}
/* The other version's compared track. Rolling in sync; in FILL the lookahead
 * from where it waits. */
static void follow_feed_donor(DtsEngine *e,AVPacket *pkt,AVRational tb) {
  MatchSide *d=&e->fw->d;
  if(e->fw_state==FW_SYNC && d->ev.max && d->ev.n>=FW_MAIN_S*DTS_MATCH_HZ) follow_roll(d,FW_MAIN_S*DTS_MATCH_HZ*2/3);
  match_side_feed(d,pkt,tb);
}
/* FILL: the other version is read ahead for the comparison only. */
static void follow_read_ahead(DtsEngine *e) {
  struct Follow *f=e->fw;
  for(int i=0;i<16 && !e->ext_demux_eof;i++) {
    double have=f->d.t0_ns!=INT64_MIN?f->d.t0_ns/1e9+(double)f->d.ev.n/DTS_MATCH_HZ:0;
    if(f->d.t0_ns!=INT64_MIN && have>=f->d_pause+f->look_s) return;
    if(match_side_full(&f->d)) return;
    if(e->ext_woken || e->ext_failed) return;
    int r=av_read_frame(e->ext->fmt,e->ext_in);
    if(r<0 && ext_read_stopped(e)) return;
    if(r==AVERROR_EOF) { e->ext_demux_eof=1; return; }
    if(r<0) return;
    if(e->ext_in->stream_index==f->dstream)
      follow_feed_donor(e,e->ext_in,e->ext->fmt->streams[f->dstream]->time_base);
    av_packet_unref(e->ext_in);
  }
}
/* FILL (and after giving up): the encoder's packets from this file's audio. */
static int follow_fill_conv(DtsEngine *e) {
  while(!e->ext_have) {
    int r;
    if(cancelled(e)) return fail(e,"Cancelled",0);
    r=avcodec_receive_packet(e->enc,e->ext_pkt);
    if(r==0) {
      int64_t pts=ns(e->ext_pkt->pts,e->enc->time_base)+e->ext_offset_ns+e->ext_base_ns;
      int64_t dur=e->ext_pkt->duration>0?ns(e->ext_pkt->duration,e->enc->time_base):0;
      if(pts<0 || pts+dur<=e->target_ns || pts<e->ext_floor_ns) { av_packet_unref(e->ext_pkt); continue; }
      e->ext_pts=pts; e->ext_have=1; break;
    }
    if(r!=AVERROR(EAGAIN)) return fail(e,"External audio encode",r);
    r=encode_fifo(e,0);
    if(r<0) return -1;
    if(r>0) continue;
    if(e->fw_state==FW_FILL) follow_read_ahead(e);
    break;   /* more of this file's audio comes with its demux */
  }
  return 0;
}
void dts_engine_external_follow(DtsEngine *e,int on) { if(e) e->fw_on=!!on; }
void dts_engine_external_automatch(DtsEngine *e,int on) { if(e) e->ext_automatch=!!on; }
int dts_engine_external_follow_poll(DtsEngine *e,int *state,int *offset_ms) {
  int k=e?e->fw_event:0;
  *state=e?e->fw_state:0; *offset_ms=e?e->fw_event_ms:0;
  if(e) e->fw_event=0;
  return k;
}
/* Conversion: the next encoded packet of the donor's audio, timed like
 * ext_fill's. Mirrors next_frame's decode loop with the donor's packets. */
static int ext_fill_conv(DtsEngine *e) {
  if(e->fw_state==FW_FILL || e->fw_state==FW_GAVEUP) return follow_fill_conv(e);
  if(e->ext_woken || e->ext_failed) return 0;
  while(!e->ext_have && !e->ext_eof) {
    int r;
    if(cancelled(e)) return fail(e,"Cancelled",0);
    r=avcodec_receive_packet(e->enc,e->ext_pkt);
    if(r==0) {
      int64_t pts=ns(e->ext_pkt->pts,e->enc->time_base)+e->ext_offset_ns+e->ext_base_ns;
      int64_t dur=e->ext_pkt->duration>0?ns(e->ext_pkt->duration,e->enc->time_base):0;
      if(pts<0 || pts+dur<=e->target_ns || pts<e->ext_floor_ns) { av_packet_unref(e->ext_pkt); continue; }
      e->ext_pts=pts; e->ext_have=1; break;
    }
    if(r==AVERROR_EOF) { e->ext_eof=1; break; }
    if(r!=AVERROR(EAGAIN)) return fail(e,"External audio encode",r);
    r=encode_fifo(e,e->ext_swr_eof);
    if(r<0) return -1;
    if(r>0) continue;
    if(!e->ext_dec_eof) {
      r=avcodec_receive_frame(e->dec,e->decoded);
      if(r==0) { r=resample(e,e->decoded); av_frame_unref(e->decoded); if(r<0) return -1; continue; }
      if(r==AVERROR_EOF) e->ext_dec_eof=1;
      else if(r!=AVERROR(EAGAIN)) return fail(e,"External audio decode",r);
    }
    if(e->ext_dec_eof && !e->ext_swr_eof) {
      r=resample(e,NULL); if(r<0) return -1; if(r==0) e->ext_swr_eof=1; continue;
    }
    if(e->ext_swr_eof && !e->ext_enc_eof) {
      if((r=avcodec_send_frame(e->enc,NULL))<0) return fail(e,"External audio encoder drain",r);
      e->ext_enc_eof=1; continue;
    }
    if(e->ext_demux_eof) { e->ext_eof=1; break; }
    r=av_read_frame(e->ext->fmt,e->ext_in);
    if(r<0 && ext_read_stopped(e)) return 0;
    if(r==AVERROR_EOF) {
      e->ext_demux_eof=1;
      if((r=avcodec_send_packet(e->dec,NULL))<0) return fail(e,"External audio decoder drain",r);
      continue;
    }
    if(r<0) return fail(e,cancelled(e)?"Cancelled":"External audio read",r);
    if(e->mt && e->ext_in->stream_index==e->mt->donor_stream)
      match_feed(e,1,e->ext_in,e->ext->fmt->streams[e->ext_in->stream_index]->time_base);
    if(e->fw && e->ext_in->stream_index==e->fw->dstream)
      follow_feed_donor(e,e->ext_in,e->ext->fmt->streams[e->fw->dstream]->time_base);
    if(e->ext_in->stream_index==e->ext_stream) {
      r=avcodec_send_packet(e->dec,e->ext_in);
      /* A damaged packet costs a few ms of sound, not the whole playback. */
      if(r<0 && r!=AVERROR(EAGAIN) && r!=AVERROR_INVALIDDATA) { av_packet_unref(e->ext_in); return fail(e,"External audio decode input",r); }
    }
    av_packet_unref(e->ext_in);
  }
  return 0;
}
static int ext_fill(DtsEngine *e) {
  if(e->ext_conv) return ext_fill_conv(e);
  if(e->ext_woken || e->ext_failed) return 0;
  while(!e->ext_have && !e->ext_eof) {
    if(cancelled(e)) return fail(e,"Cancelled",0);
    int r=av_read_frame(e->ext->fmt,e->ext_pkt);
    if(r<0 && ext_read_stopped(e)) return 0;
    if(r==AVERROR_EOF) { e->ext_eof=1; break; }
    if(r<0) {
      if(cancelled(e)) return fail(e,"Cancelled",0);
      return fail(e,"External audio read",r);
    }
    AVPacket *k=e->ext_pkt;
    /* "Match automatically" may compare another track of this version (the
     * language this file has, owner's log 07/11). */
    if(e->mt && k->stream_index==e->mt->donor_stream)
      match_feed(e,1,k,e->ext->fmt->streams[k->stream_index]->time_base);
    /* The small-shift follow compares here too (no conversion path). */
    if(e->fw && k->stream_index==e->fw->dstream)
      follow_feed_donor(e,k,e->ext->fmt->streams[k->stream_index]->time_base);
    if(k->stream_index!=e->ext_stream || k->pts==AV_NOPTS_VALUE) { av_packet_unref(k); continue; }
    AVRational tb=e->ext->fmt->streams[e->ext_stream]->time_base;
    int64_t pts=ns(k->pts,tb)+e->ext_offset_ns+e->ext_base_ns, dur=k->duration>0?ns(k->duration,tb):0;
    if(pts<0 || pts+dur<=e->target_ns || pts<e->ext_floor_ns) { av_packet_unref(k); continue; }
    e->ext_pts=pts; e->ext_have=1;
  }
  return 0;
}
static int ext_emit(DtsEngine *e,DtsFrame *f) {
  AVRational tb=e->ext_conv?e->enc->time_base:e->ext->fmt->streams[e->ext_stream]->time_base;
  av_packet_unref(e->out);
  av_packet_move_ref(e->out,e->ext_pkt); e->ext_have=0;
  f->kind=DTS_AUDIO; f->stream_index=DTS_ENGINE_EXTERNAL_STREAM;
  f->data=e->out->data; f->size=e->out->size;
  f->pts_ns=f->dts_ns=e->ext_pts;
  f->duration_ns=e->out->duration>0?ns(e->out->duration,tb):0;
  e->ext_last_end_ns=f->pts_ns+f->duration_ns;
  if(e->qs) quiet_poll(e);
  return 1;
}
/* 1 = an external packet is due (emitted into f), 0 = not yet, -1 error. */
static int ext_due(DtsEngine *e,DtsFrame *f) {
  if(ext_fill(e)<0) return -1;
  if(!e->ext_have || e->ext_clock_ns==INT64_MIN || e->ext_pts>e->ext_clock_ns+EXT_LEAD_NS) return 0;
  return ext_emit(e,f);
}
/* Main file ended: the rest of the external audio, up to the main duration. */
static int ext_drain(DtsEngine *e,DtsFrame *f) {
  if(ext_fill(e)<0) return -1;
  if(!e->ext_have || e->ext_pts>e->info.start_ns+e->info.duration_ns) return 0;
  return ext_emit(e,f);
}
static int ext_video_out(DtsEngine *e,DtsFrame *f,AVRational tb) {
  int r=frame_out(e,f,DTS_VIDEO,tb);
  int64_t v=e->out->pts!=AV_NOPTS_VALUE?e->out->pts:e->out->dts;
  if(e->ext && v!=AV_NOPTS_VALUE) { int64_t t=ns(v,tb); if(t>e->ext_clock_ns) e->ext_clock_ns=t; }
  return r;
}
/* A Matroska Cluster at or after `pos`: its offset and its timestamp
 * (Timecode, in the video stream's time base). Read 1 MiB at a time, up to
 * 64 MiB (a 4K remux can put tens of MiB between clusters). 0 when found. */
static int mkv_cluster_in(const uint8_t *b,int n,int64_t at,int64_t *cpos,int64_t *ctc) {
  for(int i=0;i+16<n;i++) {
    if(b[i]!=0x1F || b[i+1]!=0x43 || b[i+2]!=0xB6 || b[i+3]!=0x75) continue;
    int j=i+4, l=1;
    while(l<=8 && !(b[j]&(0x80>>(l-1)))) l++;
    if(l>8) continue;
    j+=l;
    if(b[j]==0xBF && j+6<n) j+=6;                 /* CRC-32 child first */
    if(j+2>=n || b[j]!=0xE7 || !(b[j+1]&0x80)) continue;   /* Timecode */
    int vl=b[j+1]&0x7F; j+=2;
    if(vl<1 || vl>8 || j+vl>n) continue;
    int64_t tc=0;
    for(int k=0;k<vl;k++) tc=(tc<<8)|b[j+k];
    *cpos=at+i; *ctc=tc;
    return 0;
  }
  return -1;
}
static int mkv_cluster_at(AVFormatContext *xf,int64_t pos,int64_t *cpos,int64_t *ctc) {
  enum { CHUNK=1024*1024, KEEP=32 };
  uint8_t *b=av_malloc(CHUNK+KEEP);
  int ok=-1, have=0;
  int64_t base=pos, got=0;   /* base: file offset of b[0] */
  if(!b) return -1;
  if(avio_seek(xf->pb,pos,SEEK_SET)>=0)
    while(ok<0 && got<64LL*CHUNK) {
      int n=avio_read(xf->pb,b+have,CHUNK);
      if(n<=0) break;
      got+=n;
      int len=have+n, keep=len<KEEP?len:KEEP;
      ok=mkv_cluster_in(b,len,base,cpos,ctc);
      memmove(b,b+len-keep,(size_t)keep);
      base+=len-keep; have=keep;
    }
  av_free(b);
  return ok;
}
/* A file with no index at all: find the Clusters around `t` (proportional jumps
 * corrected by their timestamps) and hand them to the demuxer as index entries
 * of the video stream, with one near the end so the target is never the last
 * entry (that would make Matroska parse forward cluster by cluster). */
static int ext_index_by_clusters(DtsEngine *e,AVFormatContext *xf,int vs,double t) {
  AVStream *vst=xf->streams[vs];
  int64_t total=e->ext->total;
  double len=xf->duration>0?xf->duration/1e6:0, tb=av_q2d(vst->time_base);
  int64_t pe, te, pc, tc, pos;
  if(total<=8*1024*1024 || len<=0 || tb<=0) return -1;
  if(mkv_cluster_at(xf,total-8*1024*1024,&pe,&te)<0) return -1;
  pos=(int64_t)((double)total*(t/len))-4*1024*1024;
  for(int tries=0;tries<6;tries++) {
    if(pos<0) pos=0;
    if(pos>total-4*1024*1024) pos=total-4*1024*1024;
    if(mkv_cluster_at(xf,pos,&pc,&tc)<0) { pos-=4*1024*1024; continue; }
    double at=tc*tb;
    if(at<=t && t-at<30.0) {
      av_add_index_entry(vst,pc,tc,0,0,AVINDEX_KEYFRAME);
      if(te*tb>t) av_add_index_entry(vst,pe,te,0,0,AVINDEX_KEYFRAME);
      return 0;
    }
    pos+=(int64_t)((double)total*((t-at)/len))-(at>t?4*1024*1024:0);
  }
  return -1;
}
/* The donor's container seek to `t` seconds of its own timeline. Matroska keeps
 * its index (Cues) on the video track; with only the audio track enabled, a
 * plain seek (-1) picked that audio stream, found no index and fell back to
 * reading the file from the start (owner's logs 07/11: 24-162 s of black at
 * 960-6187 s). The video track first; a file with no index gets one from the
 * Clusters found by position; the plain seek is the last resort. `how`:
 * 1 index, 2 clusters found by position, 3 plain. */
static void follow_side_reset(MatchSide *m);
static int ext_donor_seek(DtsEngine *e,double t,int *how) {
  /* The follow's copy of the other version is placed by sample count from
   * its first frame: after any jump of the other file it starts over
   * (owner's log 08/11, Chicago Fire: every live offset change from a new
   * find read as a missing scene ~13 s later). */
  if(e->fw) follow_side_reset(&e->fw->d);
  AVFormatContext *xf=e->ext->fmt;
  int64_t ts=(int64_t)(t*1000000.0);
  int vs=-1;
  for(unsigned i=0;i<xf->nb_streams;i++)
    if(xf->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO &&
       !(xf->streams[i]->disposition&AV_DISPOSITION_ATTACHED_PIC)) { vs=(int)i; break; }
  /* Once per version: does it have an index? A seek of the audio stream to 0
   * makes Matroska read its Cues (onto the video stream) and costs a packet. */
  if(vs>=0 && !e->ext_idx) {
    avformat_seek_file(xf,e->ext_stream,INT64_MIN,0,0,AVSEEK_FLAG_BACKWARD);
    e->ext_idx=avformat_index_get_entries_count(xf->streams[vs])>0?1:2;
    printf("[ext-audio] other version index: %s\n",e->ext_idx==1?"yes":"none (clusters found by position)");
    fflush(stdout);
  }
  if(vs>=0) {
    int64_t v=av_rescale_q(ts,AV_TIME_BASE_Q,xf->streams[vs]->time_base);
    if(e->ext_idx==1 || (t>30.0 && strstr(xf->iformat->name,"matroska") && ext_index_by_clusters(e,xf,vs,t)==0)) {
      if(avformat_seek_file(xf,vs,INT64_MIN,v,v,AVSEEK_FLAG_BACKWARD)>=0) { *how=e->ext_idx==1?1:2; return 0; }
    }
  }
  *how=3;
  return avformat_seek_file(xf,-1,INT64_MIN,ts,ts,AVSEEK_FLAG_BACKWARD);
}
/* The donor's packet queue and conversion chain, after a donor seek. */
static int ext_reset_chain(DtsEngine *e) {
  av_packet_unref(e->ext_pkt); e->ext_have=e->ext_eof=0;
  if(e->ext_conv) {
    avcodec_flush_buffers(e->dec);
    if(reset_encoder(e)<0) return -1;
    swr_free(&e->swr); av_channel_layout_uninit(&e->input_layout);
    av_audio_fifo_reset(e->fifo); av_packet_unref(e->ext_in);
    e->timeline=0; e->ext_demux_eof=e->ext_dec_eof=e->ext_swr_eof=e->ext_enc_eof=0;
  }
  return 0;
}
static int ext_seek(DtsEngine *e,double seconds) {
  double t=(seconds-(e->ext_offset_ns+e->ext_base_ns)/1e9)/ext_k(e);
  if(t<0) t=0;
  uint64_t t0=clock_ns(CLOCK_MONOTONIC);
  int64_t b0=(int64_t)e->ext->metrics.range_bytes;
  match_cancel(e);
  ext_unwake(e);
  quiet_reset(e->qs);
  if(e->ext_failed) return ext_reset_chain(e);   /* given up: the app goes back to its own audio */
  int how=0, r=ext_donor_seek(e,t,&how);
  printf("[ext-audio] seek to %.1fs: other version %llu ms via %s, %lld KiB read\n",t,
    (unsigned long long)((clock_ns(CLOCK_MONOTONIC)-t0)/1000000u),how==1?"index":how==2?"clusters found by position":"scan",
    (long long)(((int64_t)e->ext->metrics.range_bytes-b0)/1024));
  fflush(stdout);
  /* Woken by a newer request (it seeks again) or given up: no packets of it
   * for now, the picture still moves. */
  if(r<0 && !ext_read_stopped(e)) return fail(e,"External audio seek",r);
  e->ext_clock_ns=INT64_MIN; e->ext_last_end_ns=e->ext_floor_ns=INT64_MIN;
  if(e->fw) {   /* a jump: compared again from scratch, in sync until shown otherwise */
    follow_side_reset(&e->fw->m); follow_side_reset(&e->fw->d);
    e->fw->bad=0; e->fw->good_since_seek=0; e->fw->checked=0; e->fw->t_good=-1;
    if(e->fw_state!=FW_OFF) e->fw_state=FW_SYNC;
  }
  return ext_reset_chain(e);
}
/* A QUIET MOMENT FOR AN AUTOMATIC SYNC CHANGE (owner 16/11): the live
 * change repeats (offset growing) or skips (shrinking) that much of the
 * other version's sound at the moment it is made. It is made in steps of at
 * most QUIET_STEP_MS, each one at the end of a pause at least the step long
 * (plus a margin), so the repeated part is the pause itself (owner, 16/11: a
 * 0.6 s change made in speech repeated words; pauses of 0.64 s were too rare
 * to wait for). The other version's audio is decoded as it is given out
 * (energy per 10 ms). "Quiet" is 11 dB under the median of the last 10 s, or
 * within 4 dB of its floor (its 10th percentile) when the music under the
 * dialogue keeps the pauses above the first. No pause for QUIET_WAIT_MS: the
 * step halves, down to QUIET_STEP_MIN_MS, made then even out of a pause. */
#define QUIET_WAIT_MS 25000u
#define QUIET_STEP_MS 300
#define QUIET_STEP_MIN_MS 100
#define QUIET_RING 1000
struct Quiet {
  AVCodecContext *dec; SwrContext *swr; AVFrame *fr;
  int blk, accn, rn, rp, since_med, run, step;
  double acc; float med, floor;
  uint64_t since;   /* this step's wait started (ms) */
  float ring[QUIET_RING], tmp[QUIET_RING];
};
static int quiet_cmp(const void *a,const void *b) { float x=*(const float *)a, y=*(const float *)b; return x<y?-1:x>y; }
static void quiet_free(DtsEngine *e) {
  struct Quiet *q=e->qs;
  if(!q) return;
  avcodec_free_context(&q->dec); swr_free(&q->swr); av_frame_free(&q->fr);
  free(q); e->qs=NULL;
}
/* Its history starts over (a jump of either file). */
static void quiet_reset(struct Quiet *q) {
  if(!q) return;
  if(q->dec) avcodec_flush_buffers(q->dec);
  q->rn=q->rp=q->since_med=q->run=q->accn=0; q->acc=0; q->med=q->floor=0;
}
static int quiet_open(DtsEngine *e) {
  AVCodecParameters *par=avcodec_parameters_alloc();
  struct Quiet *q=calloc(1,sizeof *q);
  const AVCodec *c=NULL;
  int ok=par && q;
  if(ok) {
    if(e->ext_conv) ok=e->enc && avcodec_parameters_from_context(par,e->enc)>=0;
    else ok=e->ext && e->ext->fmt && e->ext_stream>=0 && avcodec_parameters_copy(par,e->ext->fmt->streams[e->ext_stream]->codecpar)>=0;
  }
  if(ok) ok=(c=avcodec_find_decoder(par->codec_id))!=NULL && (q->dec=avcodec_alloc_context3(c))!=NULL && (q->fr=av_frame_alloc())!=NULL;
  if(ok) ok=avcodec_parameters_to_context(q->dec,par)>=0;
  if(ok && par->codec_id==AV_CODEC_ID_DTS) av_opt_set_int(q->dec->priv_data,"core_only",1,0);
  if(ok) ok=avcodec_open2(q->dec,c,NULL)>=0;
  avcodec_parameters_free(&par);
  if(!ok) { if(q) { avcodec_free_context(&q->dec); av_frame_free(&q->fr); free(q); } return -1; }
  quiet_free(e); e->qs=q;
  return 0;
}
/* A 10 ms energy: quiet or not, against the last 10 s. */
static int quiet_is(const struct Quiet *q,float v) {
  if(v<1e-8f) return 1;
  if(v<=q->med*0.08f) return 1;
  return q->floor<q->med*0.2f && v<=q->floor*2.5f;
}
static void quiet_block(struct Quiet *q,float v) {
  q->ring[q->rp]=v; q->rp=(q->rp+1)%QUIET_RING; if(q->rn<QUIET_RING) q->rn++;
  if(++q->since_med>=100 && q->rn>=300) {   /* median and floor once a second */
    memcpy(q->tmp,q->ring,sizeof(float)*(size_t)q->rn);
    qsort(q->tmp,(size_t)q->rn,sizeof(float),quiet_cmp);
    q->med=q->tmp[q->rn/2]; q->floor=q->tmp[q->rn/10]; q->since_med=0;
  }
  if(q->rn>=300 && quiet_is(q,v)) q->run++; else q->run=0;
}
static void quiet_feed(struct Quiet *q,AVPacket *pkt) {
  if(!q || !q->dec || !pkt || !pkt->data || avcodec_send_packet(q->dec,pkt)<0) return;
  while(avcodec_receive_frame(q->dec,q->fr)==0) {
    AVFrame *fr=q->fr;
    if(!q->swr) {
      AVChannelLayout mono=AV_CHANNEL_LAYOUT_MONO;
      if(swr_alloc_set_opts2(&q->swr,&mono,AV_SAMPLE_FMT_FLT,fr->sample_rate,&fr->ch_layout,fr->format,fr->sample_rate,0,NULL)<0 ||
         swr_init(q->swr)<0) { swr_free(&q->swr); av_frame_unref(fr); return; }
      q->blk=fr->sample_rate/100; if(q->blk<1) q->blk=1;
    }
    float buf[4096]; uint8_t *out=(uint8_t *)buf;
    const uint8_t **in=(const uint8_t **)fr->extended_data; int left=fr->nb_samples;
    for(;;) {
      int got=swr_convert(q->swr,&out,4096,in,left);
      in=NULL; left=0;
      if(got<=0) break;
      for(int i=0;i<got;i++) {
        q->acc+=(double)buf[i]*buf[i];
        if(++q->accn==q->blk) { quiet_block(q,(float)(q->acc/q->blk)); q->acc=0; q->accn=0; }
      }
    }
    av_frame_unref(fr);
  }
}
/* The pause a step needs: the repeated (or skipped) part, plus 80 ms. */
static int quiet_need(int step) { int n=step+80; return n<200?200:n; }
static int external_shift_now(DtsEngine *e,int offset_ms);
/* After each packet of the other version given out: is this the moment for
 * the next step? */
static void quiet_poll(DtsEngine *e) {
  struct Quiet *q=e->qs;
  if(!q) return;
  quiet_feed(q,e->out);
  int cur=(int)(e->ext_offset_ns/1000000), left=e->qs_offset_ms-cur;
  int step=abs(left)<q->step?abs(left):q->step;
  uint64_t now=fx_now_ms(), waited=now-q->since;
  int ok=q->rn>=300 && q->run*10>=quiet_need(step);
  if(!ok && waited>=QUIET_WAIT_MS) {
    if(q->step>QUIET_STEP_MIN_MS) {   /* no such pause: a smaller step */
      q->step=q->step/2<QUIET_STEP_MIN_MS?QUIET_STEP_MIN_MS:q->step/2;
      q->since=now;
      printf("[ext-audio] sync change: no pause of %d ms in %u s, steps of %d ms now\n",
             quiet_need(step),QUIET_WAIT_MS/1000u,q->step);
      fflush(stdout);
      return;
    }
  } else if(!ok) return;
  int to=cur+(left>0?step:-step), run=q->run*10;
  if(ok) printf("[ext-audio] sync change %d -> %d ms (aim %d ms) at a quiet moment (%d ms of quiet, waited %.1f s)\n",
                cur,to,e->qs_offset_ms,run,waited/1000.0);
  else printf("[ext-audio] sync change %d -> %d ms (aim %d ms): no pause in %u s, made now\n",
              cur,to,e->qs_offset_ms,QUIET_WAIT_MS/1000u);
  fflush(stdout);
  int done=to==e->qs_offset_ms;
  if(done) quiet_free(e);
  else {   /* the rest at the next pause: a new one, same history */
    q->run=0; q->since=now;
    if(q->dec) avcodec_flush_buffers(q->dec);
  }
  /* The other file could not move by itself: the aim is kept for the next
   * seek (the app shows it already). */
  /* A step short of the aim is no manual change: the checks go on. */
  double t1=e->mt_t1;
  int r=external_shift_now(e,to);
  e->mt_t1=t1;
  if(r<0) {
    int aim=e->qs?e->qs_offset_ms:to;
    quiet_free(e); external_offset_set(e,aim); e->mt_t1=t1;
    printf("[ext-audio] sync change: the other version could not move now, kept for the next seek\n");
    fflush(stdout);
  }
}
/* Live sync change (CH+/CH-, owner 07/11: every change reloaded the TV
 * player): only the other file moves. It restarts a second before where its
 * audio must continue, and what lands before the end of the audio already
 * given out is dropped, so the timestamps stay continuous and the content
 * jumps by the change. Heard once the audio queued in the TV has played. */
int dts_engine_external_shift(DtsEngine *e,int offset_ms) {
  if(!e) return -1;
  /* The change waiting for quiet moments, asked again: it goes on. Another
   * value (a press of CH+/CH-) is made at once. */
  if(e->qs) { if(offset_ms==e->qs_offset_ms) return 0; quiet_free(e); }
  return external_shift_now(e,offset_ms);
}
int dts_engine_external_shift_quiet(DtsEngine *e,int offset_ms) {
  if(!e || !e->ext || !e->ext->fmt) return -1;
  if(e->fw_state==FW_FILL || e->fw_state==FW_GAVEUP || e->ext_failed) return dts_engine_external_shift(e,offset_ms);
  if(e->qs) { if(offset_ms==e->qs_offset_ms) return 0; e->qs_offset_ms=offset_ms; return 0; }   /* a new aim, same wait */
  int cur=(int)(e->ext_offset_ns/1000000);
  if(offset_ms==cur) return 0;
  if(quiet_open(e)<0) return external_shift_now(e,offset_ms);
  e->qs->step=QUIET_STEP_MS; e->qs->since=fx_now_ms();
  e->qs_offset_ms=offset_ms; e->qs_since_ms=e->qs->since;
  printf("[ext-audio] sync change %d -> %d ms: in steps of up to %d ms, each at a pause\n",cur,offset_ms,QUIET_STEP_MS);
  fflush(stdout);
  return 0;
}
static int external_shift_now(DtsEngine *e,int offset_ms) {
  if(!e || !e->ext || !e->ext->fmt) return -1;
  /* Own audio playing (a scene the other lacks): a new offset resumes it. */
  if(e->fw_state==FW_FILL || e->fw_state==FW_GAVEUP) {
    follow_resume(e,(int64_t)offset_ms*1000000); match_cancel(e); return 0;
  }
  int64_t old=e->ext_offset_ns;
  external_offset_set(e,offset_ms);   /* a change made in steps keeps waiting for the rest */
  if(e->ext_offset_ns==old) return 0;
  match_cancel(e);
  int64_t from=e->ext_last_end_ns!=INT64_MIN?e->ext_last_end_ns:
               e->ext_clock_ns!=INT64_MIN?e->ext_clock_ns:e->target_ns;
  double t=(from-e->ext_offset_ns-e->ext_base_ns)/1e9/ext_k(e)-1.0;
  if(t<0) t=0;
  int how=0;
  uint64_t t0=clock_ns(CLOCK_MONOTONIC);
  ext_unwake(e);
  if(e->ext_failed) return 0;
  if(ext_donor_seek(e,t,&how)<0 && !ext_read_stopped(e)) return -1;
  if(ext_reset_chain(e)<0) return -1;
  e->ext_floor_ns=from;
  printf("[ext-audio] live offset %d ms from %.3fs (no reload): %llu ms\n",offset_ms,from/1e9,
    (unsigned long long)((clock_ns(CLOCK_MONOTONIC)-t0)/1000000u));
  fflush(stdout);
  return 0;
}
/* Passthrough: demux only. Video still crosses the Annex-B filter; the selected
 * audio packets leave as they are in the file, trimmed to the seek target. */
static int next_frame_passthrough(DtsEngine *e,DtsFrame *f) {
  av_packet_unref(e->out);
  for(;;) {
    int r;
    if(cancelled(e)) return fail(e,"Cancelled",0);
    if(e->ext && (r=ext_due(e,f))!=0) return r;
    if(e->bsf) {
      r=av_bsf_receive_packet(e->bsf,e->out);
      if(r==0) return ext_video_out(e,f,e->bsf->time_base_out);
      if(r==AVERROR_EOF) return e->ext?ext_drain(e,f):0;
      if(r!=AVERROR(EAGAIN)) return fail(e,"Compressed video output",r);
    }
    if(e->demux_eof) return e->ext?ext_drain(e,f):0;
    r=read_packet(e);
    if(r==AVERROR_EOF) {
      e->demux_eof=1;
      if(e->bsf && (r=av_bsf_send_packet(e->bsf,NULL))<0) return fail(e,"Video filter drain",r);
      continue;
    }
    if(r<0) return packet_read_error(e,r);
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
#ifdef NV_DTS_PICTURE_DIAG
      video_demux_diag(e);
#endif
      if(e->mt_t1>=0) match_second_look_due(e,ns(e->in->pts!=AV_NOPTS_VALUE?e->in->pts:e->in->dts,tb));
      /* Follow on, another cut: the first match starts by itself. */
      if(e->fw_autoarm && !e->mt && !e->mj) { e->fw_autoarm=0; dts_engine_external_match_arm(e); e->mt_auto=1; }
      if(!e->bsf) {
        if(e->hevc_annexb.sets && (r=hevc_annexb_packet(e))<0) { av_packet_unref(e->in); return fail(e,"Compressed video output",r); }
        av_packet_move_ref(e->out,e->in); return ext_video_out(e,f,tb);
      }
      r=av_bsf_send_packet(e->bsf,e->in);
      av_packet_unref(e->in);
      if(r<0) return fail(e,"Compressed video input",r);
      continue;
    }
    if(e->mt && e->in->stream_index==e->mt->main_stream) match_feed(e,0,e->in,tb);
    else if(e->mt && e->in->stream_index==e->mt->alt_stream) match_feed(e,2,e->in,tb);
    if(e->fw && e->in->stream_index==e->fw->mstream) follow_feed_main(e,e->in,tb);
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
    r=read_packet(e);
    if(r==AVERROR_EOF) {
      e->demux_eof=1;
      if((r=avcodec_send_packet(e->dec,NULL))<0) return fail(e,"DTS decoder drain",r);
      if(e->bsf && (r=av_bsf_send_packet(e->bsf,NULL))<0) return fail(e,"Video filter drain",r);
      continue;
    }
    if(r<0) return packet_read_error(e,r);
    if(av_packet_get_side_data(e->in,AV_PKT_DATA_ENCRYPTION_INFO,NULL)) { av_packet_unref(e->in); return fail(e,"Encrypted packet unsupported",0); }
    if(e->fmt->streams[e->in->stream_index]->codecpar->codec_type==AVMEDIA_TYPE_SUBTITLE) {
      AVRational tb=e->fmt->streams[e->in->stream_index]->time_base;
      av_packet_move_ref(e->out,e->in);
      return frame_out(e,f,DTS_SUBTITLE,tb);
    }
    if(e->in->stream_index==e->info.audio_stream) {
      if(e->dec->codec_id==AV_CODEC_ID_DTS && e->in->size>=4) {
        const uint8_t *b=e->in->data;
        uint32_t sync=((uint32_t)b[0]<<24)|((uint32_t)b[1]<<16)|((uint32_t)b[2]<<8)|b[3];
        if(sync==UINT32_C(0x7ffe8001) || sync==UINT32_C(0xfe7f0180)
            || sync==UINT32_C(0x1fffe800) || sync==UINT32_C(0xff1f00e8)) e->has_core=1;
      }
      r=avcodec_send_packet(e->dec,e->in);
      if(r<0) { av_packet_unref(e->in); return fail(e,"DTS decode input",r); }
    }
    else if(e->in->stream_index==e->info.video_stream) {
#ifdef NV_DTS_PICTURE_DIAG
      video_demux_diag(e);
#endif
      if(!e->bsf) {
        AVRational tb=e->fmt->streams[e->info.video_stream]->time_base;
        if(e->hevc_annexb.sets && (r=hevc_annexb_packet(e))<0) { av_packet_unref(e->in); return fail(e,"Compressed video output",r); }
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
/* Annex-B HEVC picture without its Dolby Vision parts: RPU (type 62), EL
 * (type 63) and every NAL of a nonzero layer. Returns the bytes written. */
static size_t dv_strip_packet(const uint8_t *p,size_t n,uint8_t *out) {
  size_t pre, at=nv_dv7_start(p,n,0,&pre), used=0;
  while(pre && at<n) {
    size_t begin=at+pre, nextpre, end=nv_dv7_start(p,n,begin,&nextpre);
    if(end-begin>=2) {
      const uint8_t *nal=p+begin;
      int type=(nal[0]>>1)&63, layer=((nal[0]&1)<<5)|(nal[1]>>3);
      if(type!=62 && type!=63 && !layer) {
        memcpy(out+used,"\0\0\0\1",4); used+=4;
        memcpy(out+used,nal,end-begin); used+=end-begin;
      }
    }
    at=end; pre=nextpre;
  }
  return used;
}
int dts_engine_next(DtsEngine *e,DtsFrame *f) {
  int result;
  for (;;) {
    result=next_frame(e,f);
    if(result==1 && e->dv_strip && f->kind==DTS_VIDEO) {
      if(f->size>INT_MAX-8192) return fail(e,"Video packet too large",0);
      av_fast_malloc(&e->dv7_buffer,&e->dv7_capacity,(size_t)f->size+8192);
      if(!e->dv7_buffer) return fail(e,"Base layer buffer",AVERROR(ENOMEM));
      size_t size=dv_strip_packet(f->data,f->size,e->dv7_buffer);
      if(!size) continue;   /* an EL-only packet */
      f->data=e->dv7_buffer; f->size=size;
    }
    if(result==1 && (f->kind==DTS_AUDIO || f->kind==DTS_VIDEO))
      __atomic_store_n(&e->range_live,1,__ATOMIC_RELAXED);
    if(result!=1 || !e->dv7 || f->kind!=DTS_VIDEO) break;
    size_t size=0;
    if(f->size>INT_MAX-8192) return fail(e,"DV7 packet too large",0);
    av_fast_malloc(&e->dv7_buffer,&e->dv7_capacity,(size_t)f->size+8192);
    if(!e->dv7_buffer) return fail(e,"DV7 conversion buffer",AVERROR(ENOMEM));
    uint32_t mapping_report=0;
    if(nv_dv7_packet_report(f->data,(size_t)f->size,e->dv7_buffer,e->dv7_capacity,&size,
                          e->preserve_p7_mapping,e->mapping_reported?NULL:&mapping_report)<0)
      return fail(e,"DV7 RPU conversion failed",0);
    if(mapping_report) {
      e->mapping_reported=1;
      printf("[dv7] first RPU verified: mode=%s P7-to-P8=%d mapping-retained=%d residual-disabled=%d differs-from-standard=%d; base picture unchanged\n",
        e->preserve_p7_mapping?"preserved":"standard",!!(mapping_report&1),!!(mapping_report&2),
        !!(mapping_report&4),!!(mapping_report&8));fflush(stdout);
    }
    /* An EL-only packet is discarded, not mistaken for end of stream. */
    if(!size) continue;
    f->data=e->dv7_buffer; f->size=(int)size;
    break;
  }
#ifdef NV_DTS_PICTURE_DIAG
  if(result==1 && f->kind==DTS_VIDEO && e->picture_count<120) {
    e->picture_count++;
    nv_video_timing_log(&e->output_timing,"engine",e->picture_epoch,f->pts_ns,f->dts_ns,f->duration_ns,
      nv_video_picture_sample(e->picture_count));
    if(nv_video_picture_sample(e->picture_count))
      nv_video_picture_log("engine",e->picture_epoch,e->picture_count,f,e->info.video_codec,-1,-1);
  }
#endif
#if defined(NV_DTS_DEBUG) || defined(NV_HTTP_BUFFER_DIAG)
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
      http_session_diag(e,"sample",0);
      e->logged_metrics=*m; e->logged_at_ns=now;
    }
  }
#endif
  return result;
}
int dts_engine_seek(DtsEngine *e,double seconds) {
  if(!e || !e->fmt || !isfinite(seconds) || seconds<0 || seconds>INT64_MAX/1000000000.0) return -1;
  if(cancelled(e)) return fail(e,"Cancelled",0);
#ifdef NV_MP4_BD_TEST
  if(e->mp4)mp4_pair_reset(e);
#endif
#ifdef NV_NATIVE_SEEK_TEST
  /* A live DV seek/audio change is not a new startup: keep the bounded
   * awaited-range timeout/retry policy while fetching its first keyframe. */
  if(!e->dv || e->remux)__atomic_store_n(&e->range_live,0,__ATOMIC_RELAXED);
#else
  __atomic_store_n(&e->range_live,0,__ATOMIC_RELAXED);
#endif
  int64_t timestamp=(int64_t)(seconds*1000000.0);
  int r;
#ifdef NV_MP4_BD_TEST
  if(e->mp4)r=mp4_seek_anchor(e,timestamp);
  else
#endif
  { uint64_t t0=clock_ns(CLOCK_MONOTONIC);
    r=avformat_seek_file(e->fmt,-1,INT64_MIN,timestamp,timestamp,AVSEEK_FLAG_BACKWARD);
    if(e->ext) {
      printf("[ext-audio] seek to %.1fs: this source %llu ms\n",seconds,
        (unsigned long long)((clock_ns(CLOCK_MONOTONIC)-t0)/1000000u));
      fflush(stdout);
    } }
  if(r<0) return fail(e,"Container seek",r);
#ifdef NV_DTS_PICTURE_DIAG
  video_diag_reset(e);
#endif
  if(e->passthrough) {
    if(e->bsf) av_bsf_flush(e->bsf);
    av_packet_unref(e->in); av_packet_unref(e->out);
    e->demux_eof=0; e->target_ns=(int64_t)(seconds*1000000000.0);
    return e->ext?ext_seek(e,seconds):0;
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
