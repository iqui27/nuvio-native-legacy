#ifndef NV_DTS_ENGINE_H
#define NV_DTS_ENGINE_H
#include "dts_media.h"
typedef struct DtsEngine DtsEngine;
/* Numeric worker-only transport diagnostics, reset on open. No URL or headers
 * are retained here. Short reads exclude the final range at end of content.
 * Counters describe consumed ranges; range_wait_ns sums their HTTP durations,
 * which overlap across four persistent lanes and can exceed wall time. */
typedef struct {
  uint64_t range_requests, range_bytes, range_wait_ns, range_max_wait_ns;
  uint64_t range_short_reads;
  uint64_t range_blocked_ns; /* Time the demux worker actually waits for ranges. */
} DtsEngineMetrics;
void dts_engine_metrics(DtsEngine *, DtsEngineMetrics *out);
/* Output for the next open: 1 = AC3 5.1 640 kbps, 0 = stereo AAC (default). */
void dts_engine_set_ac3(int on);
DtsEngine *dts_engine_create(void);
int dts_engine_available(void);
/* Dolby Vision path, set before open: AC-3/E-AC-3 tracks are selectable and
 * pass through undecoded; HTTP ranges are larger and reuse the final address. */
void dts_engine_dv(DtsEngine *, int enabled);
/* Open and seek return 0 on success, negative on error. */
int dts_engine_open(DtsEngine *, const char *url, const char *headers,
                    int audio_stream, int core_only, double start_seconds);
const DtsMediaInfo *dts_engine_info(DtsEngine *);
/* Worker-only: true after a selected packet has an actual embedded core sync.
 * Resets on open; remains true across seeks on that same source/track. */
int dts_engine_has_core(DtsEngine *);
/* Borrowed AVCodecParameters for the subtitle worker. No FFmpeg types cross
 * this header; valid until reopen/destroy. NULL for a non-subtitle stream. */
const void *dts_engine_subtitle_parameters(DtsEngine *, int stream_index);
/* 1 frame, 0 EOF, -1 error. Frame data lives until the next call or seek. */
int dts_engine_next(DtsEngine *, DtsFrame *);
int dts_engine_seek(DtsEngine *, double seconds);
/* Only cancel is safe concurrently with open/next/seek. Join before destroy. */
void dts_engine_cancel(DtsEngine *);
void dts_engine_destroy(DtsEngine *);
const char *dts_engine_error(DtsEngine *);
#endif
