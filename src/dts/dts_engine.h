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
/* Donor-only policy, snapshotted at engine creation (including header probes).
 * Independent of Batch B dts_engine_audio_route: native/DV routing stays there. */
void dts_engine_set_external_dts_off(int on);
void dts_engine_set_normalize(int on);
/* Samsung remux only: ring of the single-connection reader, in MiB (8..32,
 * default 32), for rings allocated after the call. webOS never calls it. */
void dts_engine_set_ring_mib(int mib);
/* Range read-ahead of dts_engine_open for engines opened after the call, in
 * MiB (Settings > Buffer size): 0 = default (32 MiB), else 4 MiB ranges. */
void dts_engine_set_ahead_mib(int mib);
/* HTTP connections (1..4) for sources opened after the call. Conversion:
 * 1 = one continuous request (Standard), 2..4 = range lanes (default 4).
 * Native cache (dts_engine_source_create): 1 = sequential (default). */
void dts_engine_set_http_workers(int n);
void dts_engine_set_source_workers(int n);
/* Measured payload for native HTTP and conversion read-ahead. Native capacity
 * is the allocated ring; conversion capacity includes its active range plus
 * the queued range budget. Conversion snapshots must be taken on the engine's
 * owning worker; UI callers use dts_playback_status instead. */
typedef struct {
  int64_t ready_bytes, capacity_bytes, position, total;
  int64_t contiguous_bytes; /* Ready bytes up to the first missing range. */
  int http_workers; /* Pinned session download limit, not live TCP connections. */
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
/* Device-local opt-in; only the webOS experimental build implements this. */
void dts_engine_mp4_test(DtsEngine *, int enabled);
/* Snapshot before open; P7 RPU only, no extra video or HTTP buffers. */
void dts_engine_p7_mapping(DtsEngine *, int enabled);
/* Snapshot before open (dts_audio_codec.h): TrueHD conversion allowed, fixed
 * DV audio (0 pass through, 1 AAC stereo, 2 AC-3 5.1), DTS row Off. */
void dts_engine_audio_route(DtsEngine *, int truehd, int fixed, int dts_off);
/* Opt-in (#605): strip SMPTE ST 2094-40 (HDR10+) SEI from HEVC packets of
 * the next open (raw BUFFERSTREAM feed), so TVs without HDR10+ decode it as
 * plain HDR10. Static mastering display / content light SEI is kept. */
void dts_engine_set_hdr10p_strip(int on);
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
/* Reconfigure only the audio track of the open container: demux, index,
 * video filter and the bounded HTTP cache survive; the decoder/resampler/
 * encoder chain is rebuilt. Same contract as open. */
int dts_engine_audio(DtsEngine *, int audio_stream, int core_only, double seconds);
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
/* EXTERNAL AUDIO (audio of another version of the title over this picture).
 * Set BEFORE dts_engine_open; the configuration survives reopen (a failed
 * native audio switch reloads and it still applies). */
#define DTS_ENGINE_EXTERNAL_STREAM 4096   /* reported audio_stream with external audio on */
#define DTS_ENGINE_EXTERNAL_RING_MIB 8    /* the donor file's ring */
#define DTS_ENGINE_EXTERNAL_OFFSET_MAX_MS 3600000   /* +-1 h */
void dts_engine_external_audio(DtsEngine *, const char *url, const char *headers,
                               const char *langs_comma, int offset_ms);
void dts_engine_external_offset(DtsEngine *, int offset_ms);   /* live, between packets */
int dts_engine_external_active(DtsEngine *);
/* Settings: fetch only the donor's audio chunks (MP4 index), the whole file
 * otherwise. Set before open or through dts_playback_next_external_audio_only. */
void dts_engine_external_audio_only(DtsEngine *, int on);
/* Donor demux stream index chosen in the audio menu (-1: by language again).
 * Applies at the next open / external_replace. */
void dts_engine_external_track(DtsEngine *, int stream);
/* Other version minus this one, seconds, and another-speed flag (the player's
 * "the versions do not match" warning). Set by configure_external. */
void dts_engine_external_mismatch(DtsEngine *, int *cut_s, int *speed);
/* Live audio shift (CH+/CH-), milliseconds. Returns the applied offset or
 * INT_MIN when external audio is not on. */
int dts_engine_external_shift(DtsEngine *, int offset_ms);
/* The audio tracks of another file, for the hold-OK menu (audioext.c):
 * opens only the container header. -1 on failure, else the count. */
int dts_engine_probe_audio(const char *url, const char *headers, DtsTrack *out, int max);
/* Interrupts the probe in flight (it returns -1). Any thread. */
int dts_engine_probe_audio_cancelled(const char *url, const char *headers, DtsTrack *out, int max, const int *cancel);
/* Result of dts_engine_external_match_poll: the measured offset between the
 * two versions, how it was found (0 none, 1 exact, 2 drift/speed), the
 * candidates for the sync editor and the user's hint. */
typedef struct {
  int offset_ms;      /* other version's time = this one's + offset (to apply) */
  int ratio_ppm;      /* speed difference, parts per million (0: same) */
  int score;          /* match strength 0..100 */
  int how;            /* 0 none, 1 offset, 2 offset+speed */
  int hint_ms;        /* the offset in use when the match ran */
  int too_heavy;      /* search window too big for the signal captured */
  int automatic;      /* captured by "Match automatically", not the menu */
  int ncand, cand_ms[3], cand_score[3];
} DtsMatchInfo;
/* "Match automatically" (settings): capture starts by itself at start or a
 * jump; menu: match_arm runs it once. arm: 0 started, -1 cannot; poll:
 * 0 running, 1 done (out), -1 failed/cancelled. */
int dts_engine_external_match_arm(DtsEngine *);
int dts_engine_external_match_poll(DtsEngine *, DtsMatchInfo *out);
/* The donor's playback speed / this one's (e.g. 24000/25000 PAL): its audio
 * is stretched back. 0 clears. */
void dts_engine_external_speed(DtsEngine *, int ratio_ppm);
/* Fetch ahead of a coming jump target (intro skip): ~10 s of the file at
 * `seconds` on its own request, served to a seek landing there. -1 = cannot
 * (no index near it, not MKV/MP4); tidy releases it once read past. */
int dts_engine_prefetch(DtsEngine *, double seconds);
void dts_engine_prefetch_tidy(DtsEngine *);
/* Drop the current connection and reopen from where the ring is (a slow
 * route stays slow until reconnect). Native-cache and conversion-stream
 * readers only. */
void dts_engine_reconnect(DtsEngine *);
/* Follow another cut: scenes the donor lacks keep this file's own audio
 * (converted), the rest plays the donor's. on=1 arms it. */
void dts_engine_external_follow(DtsEngine *, int on);
/* Series memory: the track chosen in the last episode's version (audioext.c). */
void dts_engine_external_track_hint(DtsEngine *, int stream, const char *lang,
                                    const char *codec, int channels, const char *title);
/* "Match automatically" (Settings): capture by itself when external audio
 * starts. */
void dts_engine_external_automatch(DtsEngine *, int on);
/* Follow state for the app: state (0 off, 1 donor, 2 own audio), the offset
 * used last. -1 not following. */
int dts_engine_external_follow_poll(DtsEngine *, int *state, int *offset_ms);
/* Swap ONLY the other version (audioext menu re-pick), keeping this source's
 * demux/read-ahead/video state. -1 = a fresh open is needed. */
int dts_engine_external_replace(DtsEngine *, const char *url, const char *headers,
                                const char *langs, int offset_ms, int track,
                                int audio_only, double target_seconds);
/* Bytes the donor's reader fetched (the progress line while it downloads). */
int64_t dts_engine_external_bytes(DtsEngine *, int64_t *total);
/* Language match for the picker: NULL = exact strcasecmp. The app installs
 * its alias table ("en"/"eng"...). */
void dts_engine_set_lang_match(int (*match)(const char *code, const char *pref));
/* Change the audio track live (this file's own index): reopen the same
 * container at `seconds` with stream `stream`. core_only as at open. */
int dts_engine_audio(DtsEngine *, int stream, int core_only, double seconds);
#endif
