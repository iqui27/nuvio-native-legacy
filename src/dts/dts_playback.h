#ifndef NV_DTS_PLAYBACK_H
#define NV_DTS_PLAYBACK_H
#include "dts_media.h"
#include "dts_subtitles.h"
#include "dts_engine.h"
typedef struct DtsPlayback DtsPlayback;
typedef struct {
  int prepared, failed, eof, revision, native_loaded;
  DtsMediaInfo info;
  char media_id[64], error[160];
  DtsBufferInfo buffer;
  double buffered_until;
  unsigned buffering_ms;
  /* The position the player asked for at start (audioext uses it to seek the
   * donor the same way). */
  double start_position;
  /* Other version minus this one, seconds, and a another-speed flag. */
  int ext_cut_s, ext_speed;
  int net_retries;
  /* Follow-another-cut: state (0 off, 1 donor, 2 own), event count, the last
   * transition kind and the offset used. */
  int follow_state, follow_events, follow_kind, follow_offset_ms;
} DtsPlaybackStatus;
/* All expensive work and native Feed calls are serialized on one worker.
 * Callers own a session; close joins it before freeing callback context. */
int dts_playback_enabled(void);
/* Error text of a Dolby Vision session whose source could not keep up. */
#define DTS_PLAYBACK_STARVED "Source too slow for the Dolby Vision path"
/* Main thread, right before dts_playback_start: the next session is a Dolby
 * Vision one (AC-3/E-AC-3 passthrough, larger direct ranges, starvation
 * fallback). Consumed by that start. */
void dts_playback_next_dv(int enabled);
/* Main thread: snapshot the P7/recovery opt-in for the next session. */
void dts_playback_next_p7(int enabled);
void dts_playback_next_mp4_test(int enabled);
/* Main thread: snapshot the experimental P7 mapping opt-in (next session). */
void dts_playback_next_p7_mapping(int enabled);
/* Main thread: snapshot the audio route for the next session (TrueHD
 * conversion, fixed DV audio 0/1 AAC/2 AC-3, DTS row Off). */
void dts_playback_next_audio_route(int truehd, int fixed, int dts_off);
DtsPlayback *dts_playback_start(const char *url, const char *headers,
                               int audio_stream, double position,
                               int paused, const char *window, int webos_major,
                               const DtsTrack *selected, int ordinal, int count);
void dts_playback_status(DtsPlayback *p, DtsPlaybackStatus *out);
int dts_playback_event(DtsPlayback *p, char *json, size_t n);
void dts_playback_pause(DtsPlayback *p, int paused);
void dts_playback_volume(DtsPlayback *p, int percent);
void dts_playback_seek(DtsPlayback *p, double seconds);
void dts_playback_audio(DtsPlayback *p, int stream);
/* EXTERNAL AUDIO. The next session keeps its own picture and takes the audio
 * of `url` (usually another version of the same title). `langs` picks the
 * donor track by language; `offset_ms` shifts it (positive = later). Consumed
 * by that start. */
void dts_playback_next_external_audio(const char *url, const char *headers,
                                      const char *langs, int offset_ms);
/* The donor's playback speed / this one's (PAL slowdown), for the next
 * session. 0 clears. */
void dts_playback_next_external_speed(int ratio_ppm);
/* The next session follows the other cut: scenes the donor lacks keep this
 * file's own audio. */
void dts_playback_next_external_follow(int on);
/* Match automatically when external audio starts (setting snapshot). */
void dts_playback_next_external_automatch(int on);
/* Series memory for the next session: the track chosen in the last episode. */
void dts_playback_next_external_hint(int stream, const char *lang,
                                     const char *codec, int channels, const char *title);
/* The next session fetches only the donor's audio chunks (MP4 index). */
void dts_playback_next_external_audio_only(int on);
/* Donor demux stream index for the next session (-1: by language). */
void dts_playback_next_external_track(int stream);
/* While playing: use this source's audio now, keeping this file's read-ahead
 * (`seconds` = resume target). -1 when a reopen is needed. */
int dts_playback_external_swap(DtsPlayback *p, const char *url, const char *headers,
                               const char *langs, int offset_ms, int track,
                               int audio_only, double seconds);
/* Live donor shift, milliseconds (sync editor). Applied between packets. */
void dts_playback_external_offset(DtsPlayback *p, int offset_ms);
/* Like external_offset but also reported for the live-sync editor. */
void dts_playback_external_offset_live(DtsPlayback *p, int offset_ms);
/* Run the offset match once (sync editor "match automatically"). -1 cannot. */
int dts_playback_external_match(DtsPlayback *p);
/* 0 running, 1 done (out), -1 failed. */
int dts_playback_external_match_result(DtsPlayback *p, DtsMatchInfo *out);
/* Donor speed for the running session, ppm. */
void dts_playback_external_speed(DtsPlayback *p, int ratio_ppm);
/* Fetch ~10 s ahead of a jump target (intro skip prefetch). -1 = cannot. */
int dts_playback_prefetch(DtsPlayback *p, double seconds);
/* 1 = external audio is on for this session. */
int dts_playback_external(DtsPlayback *p);
/* -1 off; -2-ordinal restores a native subtitle; >=0 exact source stream. */
void dts_playback_subtitle(DtsPlayback *p, int stream);
int dts_playback_subtitle_text(DtsPlayback *p, double seconds, char *out, size_t n);
/* Copies changed RGBA into caller-owned storage. A clearing cue has no pixels.
 * previous_revision avoids uploading/copying the same bitmap each frame. */
int dts_playback_subtitle_bitmap(DtsPlayback *p, double seconds,
                                 unsigned previous_revision, DtsSubtitle *out,
                                 unsigned *revision);
void dts_playback_close(DtsPlayback *p);
#endif
