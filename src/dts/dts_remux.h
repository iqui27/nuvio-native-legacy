/* DTS / TrueHD -> Dolby Digital remux for the Samsung .tpk
 * (docs/features/dts/README.md). Video packets are copied untouched; ONE
 * DTS/TrueHD/MLP track is decoded and re-encoded as AC3 (5.1 640 kb/s, or 2.0
 * 192 kb/s for stereo sources) or, with the "Stereo" setting, AAC 2.0. Output
 * goes to a writer callback: MPEG-TS segments for HLS, or one progressive
 * MPEG-TS / Matroska / fragmented MP4 stream.
 *
 * No FFmpeg type crosses this header. In builds without FFmpeg every call fails
 * cleanly (dts_remux_available() == 0). */
#ifndef NV_DTS_REMUX_H
#define NV_DTS_REMUX_H
#include <stddef.h>
#include <stdint.h>

typedef struct DtsRemux DtsRemux;        /* probed source: track, index, segments */
typedef struct DtsRemuxGen DtsRemuxGen;  /* one generator: own reader, codecs, muxer */

typedef struct {
  double duration_s;
  int width, height;
  char video_codec[16], audio_codec[16];
  char hdr[48];                /* PQ, HLG, SDR or "not in the container" (container flags) */
  int audio_ordinal, stream_index; /* k-th audio track of the file (0-based), FFmpeg index */
  int in_channels, in_rate;    /* from the container header */
  int out_channels, out_bitrate;
  char out_codec[8];           /* "ac3" or "aac" */
  char decoder_path[40];       /* "DTS core only", "TrueHD 5.1 presentation", ... */
  int n_keyframes, n_segments;
  int index_complete;          /* the keyframe index reaches the end (HLS needs it): no
                                * Matroska Cues, or a fragmented MP4 without one, = 0 */
  double max_segment_s;
  int64_t size_bytes;          /* -1 while unknown */
  int mp4;                     /* source container is MP4/MOV */
  double fps;                  /* container frame rate, 0 = unknown (HLS master playlist) */
  int dv_profile;              /* Dolby Vision configuration in the container: profile, -1 = none */
  int dv_el, dv_compat;        /* enhancement layer present; base-layer compatibility id */
} DtsRemuxInfo;

typedef struct {
  int64_t bytes_out;
  long video_pkts, audio_pkts;
  double first_pts_s, last_pts_s; /* source time of the first/last video packet out */
  double wall_s, cpu_s, audio_cpu_s, range_wait_s;
  int fresh;                      /* 1 = this run started with a seek */
} DtsRemuxStats;

/* Writes n bytes; returns n, or < 0 to stop the run (client gone). */
typedef int (*DtsRemuxWrite)(void *opaque, const unsigned char *data, int n);

int dts_remux_available(void);
/* Shared packet budget across generators: 192 MiB, 128 MiB on older TVs. */
void dts_remux_set_queue_budget_mib(int mib);
/* Device offset, snapshotted by each new source. Positive delays audio only. */
void dts_remux_set_audio_offset_ms(int ms);

/* Opens the source and reads its keyframe index (one seek: Matroska parses
 * Cues lazily). audio_ordinal: which audio track to convert, counted among the
 * file's audio tracks in container order from 0 (FFmpeg does not expose the
 * Matroska TrackNumber; its streams follow the TrackEntry order), or -1 for the
 * first DTS/TrueHD/MLP track. stereo_aac: 1 = AAC 2.0 (Settings
 * "Stereo (AAC)"), 0 = AC3. seg_target_s: HLS segments group keyframes to at
 * least this length. *cancel (may be NULL) aborts the network waits. NULL on
 * failure with the reason in err (no URL in it). */
DtsRemux *dts_remux_open(const char *url, const char *headers, int audio_ordinal, int stereo_aac,
                         double seg_target_s, volatile int *cancel, char *err, size_t errn);
/* The same in two steps, so another thread can stop a probe that waits on the
 * network: dts_remux_new() returns an empty source that dts_remux_cancel()
 * already accepts (*cancel, may be NULL, is also read as in dts_remux_open); dts_remux_probe() fills it (0) or fails (-1, reason in err,
 * "Cancelled" after a cancel). Either way the caller closes it. */
DtsRemux *dts_remux_new(volatile int *cancel);
int dts_remux_probe(DtsRemux *, const char *url, const char *headers, int audio_ordinal, int stereo_aac,
                    double seg_target_s, char *err, size_t errn);
const DtsRemuxInfo *dts_remux_info(const DtsRemux *);
double dts_remux_segment_start(const DtsRemux *, int seg);    /* seconds; segment 0 = 0 */
double dts_remux_segment_duration(const DtsRemux *, int seg);
int dts_remux_segment_at(const DtsRemux *, double seconds);
/* Source time of the index keyframe at or before `seconds` (progressive runs
 * start there); `seconds` itself when there is no index. */
double dts_remux_keyframe_before(const DtsRemux *, double seconds);
/* Aborts every network wait of this source and its generators (async-safe). */
void dts_remux_cancel(DtsRemux *);
/* Frees the source; every generator must be freed first. */
void dts_remux_close(DtsRemux *);

/* A generator. The first one reuses the reader opened by dts_remux_open. */
DtsRemuxGen *dts_remux_gen_new(DtsRemux *, char *err, size_t errn);
/* Segment this generator continues with WITHOUT seeking, or -1. */
int dts_remux_gen_next(const DtsRemuxGen *);
/* seg >= 0: HLS segment `seg` as MPEG-TS (continues when seg == next, else
 * seeks). seg < 0: progressive stream in fmt ("mpegts", "matroska" or "mp4")
 * from the keyframe at or before start_s to the end. *stop (may be NULL) ends
 * the run between packets. 0 = done, -1 = error (err), -2 = stopped. */
int dts_remux_gen_run(DtsRemuxGen *, int seg, double start_s, const char *fmt,
                      DtsRemuxWrite w, void *opaque, volatile int *stop,
                      DtsRemuxStats *st, char *err, size_t errn);
void dts_remux_gen_free(DtsRemuxGen *);
/* Breaks the run of this generator out of a network wait (the player gave up
 * on it). Any thread; the run ends with -2 and the next run seeks. */
void dts_remux_gen_interrupt(DtsRemuxGen *);

/* Source time (seconds) a progressive run started at: the keyframe it used. */
double dts_remux_gen_start_s(const DtsRemuxGen *);
#endif
