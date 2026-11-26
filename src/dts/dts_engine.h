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
/* Validated 1..4, snapshotted per engine/source. Defaults: conversion 4,
 * ordinary native HTTP cache 1 (the unchanged sequential streaming reader). */
void dts_engine_set_http_workers(int workers);
void dts_engine_set_source_workers(int workers);
/* Samsung remux only: ring of the single-connection reader, in MiB (8..32,
 * default 32), for rings allocated after the call. webOS never calls it. */
void dts_engine_set_ring_mib(int mib);
/* Range read-ahead of dts_engine_open for engines opened after the call, in
 * MiB (Settings > Buffer size): 0 = default (32 MiB), else 4 MiB ranges. */
void dts_engine_set_ahead_mib(int mib);
/* Test option: conversion opened after this reads over one continuous HTTP
 * request (ring = buffer size) instead of range requests. */
void dts_engine_set_conversion_stream(int on);
/* Settings > DTS "Off": no DTS track is selected or converted. */
void dts_engine_set_dts_off(int on);
int dts_engine_dts_off(void);
/* Measured payload for native HTTP and conversion read-ahead. Native capacity
 * is the allocated ring; conversion capacity includes its active range plus
 * the queued range budget. Conversion snapshots must be taken on the engine's
 * owning worker; UI callers use dts_playback_status instead. */
typedef struct {
  int64_t ready_bytes, capacity_bytes, position, total;
  int64_t contiguous_bytes; /* Ready bytes up to the first missing range. */
  int http_workers; /* Pinned session download limit, not live TCP connections. */
  uint64_t received_bytes; /* Input payload received since open (stream and audio-only readers). */
} DtsBufferInfo;
void dts_engine_buffer_info(DtsEngine *, DtsBufferInfo *);
DtsEngine *dts_engine_source_create(const char *url, const char *headers, int mib);
/* pending_fd: replacement listener, or -1 for a non-preemptible read. */
int dts_engine_source_read(DtsEngine *, int64_t offset, unsigned char *data, int n, int pending_fd);
int64_t dts_engine_source_total(DtsEngine *);
DtsEngine *dts_engine_create(void);
int dts_engine_available(void);
/* Dolby Vision path, set before open: AC-3/E-AC-3 tracks are selectable and
 * pass through undecoded; HTTP ranges are larger and reuse the final address.
 * NV_DV_TRUEHD_TEST also enables TrueHD conversion to AAC/AC3 on this path. */
void dts_engine_dv(DtsEngine *, int enabled);
/* Worker-owned, before open. Experimental fixed DV output: 0 = v12 routing,
 * 1 = AAC stereo, 2 = AC3 5.1 (stereo input does not gain surround content).
 * Ignored outside the DV packet path and without NV_DV_FIXED_AUDIO_TEST. */
void dts_engine_dv_audio(DtsEngine *, int mode);
/* Explicit opt-in before open: P7 conversion and webOS recovery. Default off. */
void dts_engine_allow_p7(DtsEngine *, int enabled);
/* Snapshot before open; P7 RPU only, no extra video or HTTP buffers. */
void dts_engine_p7_mapping(DtsEngine *, int enabled);
/* Strict experimental dual-track P7 MP4 open; never enabled for MKV or native
 * P5/P8 playback. Only the test build implements this device-local opt-in. */
void dts_engine_mp4_test(DtsEngine *, int enabled);
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
/* Worker only: change a supported audio track on the existing container and
 * byte cache. On failure reopen before reading; cancellation remains permanent. */
int dts_engine_audio(DtsEngine *, int stream, int core_only, double seconds);
/* Only cancel is safe concurrently with open/next/seek. Join before destroy. */
void dts_engine_cancel(DtsEngine *);
void dts_engine_destroy(DtsEngine *);
const char *dts_engine_error(DtsEngine *);
/* Test: audio from another version of the same title (external audio).
 * Worker-owned, before open: the next opens read the video (and subtitles)
 * of the main source and ONLY the audio of this second source, over its own
 * small single-connection ring (DTS_ENGINE_EXTERNAL_RING_MIB). AC-3, E-AC-3
 * and stereo 48 kHz AAC pass through; other codecs fail the open. `langs` is
 * a comma list of preferred languages ("pl,en"), matched with the hook below;
 * no match takes the first usable track. offset_ms shifts the external audio
 * later (positive) or earlier (negative). url NULL/"" turns it off. */
#define DTS_ENGINE_EXTERNAL_STREAM 4096
#define DTS_ENGINE_EXTERNAL_RING_MIB 8
/* Its ring for the next one opened (Settings: 8, 16 or 32 MiB; else 8). */
void dts_engine_set_external_ring_mib(int mib);
void dts_engine_external_audio(DtsEngine *, const char *url, const char *headers,
                               const char *langs, int offset_ms);
/* Worker only: new offset for the next seek (applied by dts_engine_seek). */
void dts_engine_external_offset(DtsEngine *, int offset_ms);
int dts_engine_external_active(DtsEngine *);
/* The other version's stream in use (-1: none open yet). */
int dts_engine_external_stream(DtsEngine *);
/* An MP4 with audio from another source: only this subtitle stream is read
 * (-1: none); a no-op otherwise. From the playback worker only. */
void dts_engine_subtitle_only(DtsEngine *, int stream);
/* Before open: fetch only the donor's audio chunks when the file has an index
 * of them (MP4); Matroska and very fragmented files read the whole file. */
void dts_engine_external_audio_only(DtsEngine *, int on);
/* Before open: play this audio stream of the donor (its demux index) instead
 * of the language pick; -1 = by language. The info's track list carries every
 * usable donor audio track: the playing one as DTS_ENGINE_EXTERNAL_STREAM,
 * the others as DTS_ENGINE_EXTERNAL_STREAM + 1 + donor index. */
void dts_engine_external_track(DtsEngine *, int stream);
/* Worker only, after open: the other version's length minus this one's, in
 * seconds (another cut), and whether its frame rate differs (another speed). */
void dts_engine_external_mismatch(DtsEngine *, int *cut_s, int *speed, int *ratio_ppm);
/* Worker only, while playing: a new offset applied without a seek of this
 * source (the other file moves, timestamps stay continuous). 0 ok; on -1 the
 * caller seeks as before. */
int dts_engine_external_shift(DtsEngine *, int offset_ms);
/* The same for a change from an automatic check made while playing
 * (DtsMatchInfo.later): made in steps of up to 300 ms, each at a pause of the
 * other version's audio at least as long, so the repeated or skipped part is
 * silence. Asked again with the same value it goes on; another value becomes
 * the new aim; dts_engine_external_shift with another value, or a seek,
 * replaces it. */
int dts_engine_external_shift_quiet(DtsEngine *, int offset_ms);
/* Blocking (a worker thread): the audio tracks of a file this TV can play or
 * convert, read from its container header only. Count, -1, or -3 for the
 * debrid placeholder clip (a file under two minutes). */
int dts_engine_probe_audio(const char *url, const char *headers, DtsTrack *out, int max);
/* Worker only, while another version plays: "Match automatically" captures
 * both audios for DTS_MATCH_SECONDS of playback (dts_match.h) and searches
 * +-10 s around the current offset, at the same speed and at the speed of
 * the frame rates. Not found: a background reader of the other version tries
 * the offsets its length suggests (another cut), then +-60 s and +-5 min when
 * that download stays reasonable. After a find at the same speed, a second
 * look 5 minutes later measures a slow drift (another speed).
 * poll: 0 still working (or idle), 1 found, -1 not trusted, failed or
 * interrupted; a result is reported once. */
/* The reach of the sync: +-60 min (an extended cut against the theatrical
 * one gathers tens of minutes, owner's log 07/11). */
#define DTS_ENGINE_EXTERNAL_OFFSET_MAX_MS 3600000
typedef struct {
  int offset_ms;    /* the app's sign: positive = audio later */
  int ratio_ppm;    /* speed of the other version: 0 the same, else (ratio - 1) * 1e6 */
  int score;        /* correlation * 100 */
  int how;          /* found: 1 near, 2 length hint, 3 +-60 s, 4 +-5 min, 5 speed (second look) */
  int hint_ms;      /* not found: offset the lengths suggest (0: none) */
  int too_heavy;    /* not found: the wide search was skipped (too much to download) */
  int automatic;    /* started by the engine (following another cut), not by a click */
  int later;        /* found by a check made while playing (the sync checked all through):
                     * no toast, the change at a quiet moment, not the series memory */
  int ncand;        /* the best places found, for the person to try (offset ms, score %) */
  int cand_ms[3], cand_score[3];
} DtsMatchInfo;
int dts_engine_external_match_arm(DtsEngine *);
/* LG (owner 26/11, the .tpk's way): the audio refine, +-1 s around the
 * offset in force and nothing wider (how 7, automatic); the file's track the
 * match compares, picked by hand (container index; -1 Auto); the edit map
 * (n stretches from ini_ns, this file's time, each with its offset in ms; the
 * one under the picture being read is applied live; n < 2 clears it). */
#define DTS_ENGINE_MAP_MAX 12
int dts_engine_external_match_fine(DtsEngine *);
void dts_engine_external_match_track(DtsEngine *, int stream);
void dts_engine_external_map(DtsEngine *, int n, const int64_t *ini_ns, const int *ms);
/* Any thread, set once: called on the worker when both files of an external
 * session are open (AVFormatContexts as void *; hls 1 for the HLS remux). */
void dts_engine_set_external_open_hook(void (*fn)(void *main_fmt, void *donor_fmt, int hls));
void dts_engine_external_open_notify(void *main_fmt, void *donor_fmt, int hls);
int dts_engine_external_match_poll(DtsEngine *, DtsMatchInfo *out);
/* Before open (or replace): play the other version at this speed
 * (ratio_ppm from a match). Not 0: its audio is decoded, resampled to the
 * speed and re-encoded. */
void dts_engine_external_speed(DtsEngine *, int ratio_ppm);
/* Worker only: fetch about 10 s of the file at `seconds` ahead of a jump
 * there (an intro skip), over a request of its own; a seek landing there is
 * served from those bytes. Stream reader (one connection) only. 0 started. */
int dts_engine_prefetch(DtsEngine *, double seconds);
void dts_engine_prefetch_tidy(DtsEngine *);
/* Worker only: the stream reader starts a fresh connection after what it has. */
void dts_engine_reconnect(DtsEngine *);
/* Before open (or replace): follow another cut through the whole film
 * (Settings). Only for a version whose length differs by over 10 s; its
 * audio is converted. poll: an event (1 this file's own audio in a scene the
 * other lacks, 2 the other resumed at *offset_ms, 3 lost it, 4 a small shift
 * corrected live to *offset_ms), 0 none; *state
 * 0 off, 1 in sync, 2 own audio, 3 lost. */
void dts_engine_external_follow(DtsEngine *, int on);
/* Before open: the track chosen in the last episode's version (series
 * memory): this version's track most like it is used when none is forced
 * (dts_engine_external_track). stream -1: unknown. */
void dts_engine_external_track_hint(DtsEngine *, int stream, const char *lang, const char *codec,
                                    int channels, const char *title);
/* Before open (or replace): "Match automatically" starts by itself once the
 * other version plays (Settings, owner 08/11); results come back with
 * DtsMatchInfo.automatic set. */
void dts_engine_external_automatch(DtsEngine *, int on);
int dts_engine_external_follow_poll(DtsEngine *, int *state, int *offset_ms);
/* Worker only, on an open external session: replace the other version (new
 * file, track, offset, audio-only mode) at `target` seconds without reopening
 * this source. The track list is rebuilt. 0 on success; on failure the session
 * needs a fresh open (the old version is already gone). */
int dts_engine_external_replace(DtsEngine *, const char *url, const char *headers, const char *langs,
                                int offset_ms, int track, int audio_only, double target);
/* Worker only: bytes the donor's reader has downloaded, and its size. */
int64_t dts_engine_external_bytes(DtsEngine *, int64_t *total);
/* Any thread: a request is pending while the worker waits for the other
 * version's data; that wait stops (1 if one was stopped). Worker only:
 * woken = that version gives no packets until its next seek or swap; failed =
 * it stopped answering after its reconnections (the app goes back to the
 * file's own audio), the picture goes on. */
int dts_engine_external_wake(DtsEngine *);
int dts_engine_external_woken(DtsEngine *);
int dts_engine_external_failed(DtsEngine *);
/* Another source's version remembers its server address (behind the add-on's
 * redirects) for 30 min between engines; forget it before reopening that
 * version because its server was slow. */
void dts_engine_forget_final(const char *url, const char *headers);
/* Language comparison for the external track choice (the app passes
 * ling_casa); NULL or unset compares case-insensitively. Set once. */
void dts_engine_set_lang_match(int (*match)(const char *code, const char *pref));
/* Samsung remux (dts_remux.c): open only the container over the ranged reader
 * (4 MiB ranges, final address reused). 0 on success. dts_engine_avformat()
 * then returns the AVFormatContext (as void *: no FFmpeg type crosses this
 * header), valid until the next open or destroy. Cancel stays permanent. */
int dts_engine_open_demux(DtsEngine *, const char *url, const char *headers);
void *dts_engine_avformat(DtsEngine *);
#ifdef NV_WEBOS_HLS_TEST
/* Per-reader buffer for webOS HLS. The Samsung default remains adaptive. */
void dts_engine_hls_read_ahead(DtsEngine *, int mib);
int dts_engine_hls_audio(DtsEngine *, int stream, const char *langs, const DtsTrack *hint,
                         int audio_only, int allow_dts);
#endif
int64_t dts_engine_size(DtsEngine *);
/* Remux: on = 1 makes the read in progress (and the next ones) fail until it
 * is set back to 0; unlike cancel it is not permanent. Any thread. */
void dts_engine_interrupt(DtsEngine *, int on);
/* Remux reader: hangs up on the source and frees its ring; the container,
 * its index and the position stay, and the next read connects again from
 * there (a Range request). For a reader that sits idle (the audio of one
 * source while another one plays): the connection goes back to the budget.
 * Not while another thread reads this engine. */
void dts_engine_release(DtsEngine *);
/* The host the reader ended up on behind the add-on's redirects ("" none
 * yet), for counting connections per host. Host only: no path, no token. */
void dts_engine_final_host(DtsEngine *, char *host, int n);
#endif
