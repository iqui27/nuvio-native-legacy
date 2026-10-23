#ifndef NV_DTS_ENGINE_H
#define NV_DTS_ENGINE_H
#include "dts_media.h"
typedef struct DtsEngine DtsEngine;
/* Recoverable by a fresh native Load/decoder timeline, not a decode failure. */
#define DTS_ENGINE_TIMESTAMP_DISCONTINUITY "DTS timestamp discontinuity requires reload"
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
/* Samsung remux readers: rings alive now and the most alive at once since the
 * last reset (count and bytes), for the per-playback memory line of the .tpk.
 * reset_peak sets the peak back to what is alive now. */
typedef struct { int alive, peak; long long bytes, peak_bytes; } DtsEngineRings;
void dts_engine_rings(DtsEngineRings *out, int reset_peak);
/* Output for the next open: 1 = AC3 5.1 640 kbps, 0 = stereo AAC (default). */
void dts_engine_set_ac3(int on);
/* Samsung remux only: ring of the single-connection reader, in MiB (8..32,
 * default 32), for rings allocated after the call. webOS never calls it. */
void dts_engine_set_ring_mib(int mib);
/* Range read-ahead of dts_engine_open for engines opened after the call, in
 * MiB (Settings > Buffer size): 0 = default (32 MiB), else 4 MiB ranges. */
void dts_engine_set_ahead_mib(int mib);
/* Measured payload for native HTTP and conversion read-ahead. Native capacity
 * is the allocated ring; conversion capacity includes its active range plus
 * the queued range budget. Conversion snapshots must be taken on the engine's
 * owning worker; UI callers use dts_playback_status instead. */
typedef struct {
  int64_t ready_bytes, capacity_bytes, position, total;
  int64_t contiguous_bytes; /* Ready bytes up to the first missing range. */
} DtsBufferInfo;
void dts_engine_buffer_info(DtsEngine *, DtsBufferInfo *);
DtsEngine *dts_engine_source_create(const char *url, const char *headers, int mib);
/* pending_fd: replacement listener, or -1 for a non-preemptible read. */
int dts_engine_source_read(DtsEngine *, int64_t offset, unsigned char *data, int n, int pending_fd);
int64_t dts_engine_source_total(DtsEngine *);
DtsEngine *dts_engine_create(void);
int dts_engine_available(void);
/* Dolby Vision path, set before open: AC-3/E-AC-3 tracks are selectable and
 * pass through undecoded; HTTP ranges are larger and reuse the final address. */
void dts_engine_dv(DtsEngine *, int enabled);
/* Explicit opt-in before open: P7 conversion and webOS recovery. Default off. */
void dts_engine_allow_p7(DtsEngine *, int enabled);
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
/* Samsung remux (dts_remux.c): open only the container over the ranged reader
 * (4 MiB ranges, final address reused). 0 on success. dts_engine_avformat()
 * then returns the AVFormatContext (as void *: no FFmpeg type crosses this
 * header), valid until the next open or destroy. Cancel stays permanent. */
int dts_engine_open_demux(DtsEngine *, const char *url, const char *headers);
void *dts_engine_avformat(DtsEngine *);
int64_t dts_engine_size(DtsEngine *);
/* Remux: on = 1 makes the read in progress (and the next ones) fail until it
 * is set back to 0; unlike cancel it is not permanent. Any thread. */
void dts_engine_interrupt(DtsEngine *, int on);
#endif
