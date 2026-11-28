#ifndef NV_DTS_PLAYBACK_H
#define NV_DTS_PLAYBACK_H
#include "dts_media.h"
#include "dts_subtitles.h"
#include "dts_engine.h"
typedef struct DtsPlayback DtsPlayback;
typedef struct {
  int prepared, failed, eof, revision, native_loaded;
  int load_revision; /* Native Load generation; audio-only metadata can change revision. */
  DtsMediaInfo info;
  char media_id[64], error[160];
  DtsBufferInfo buffer;
  double buffered_until;
  double start_position; /* Position applied by the latest successful Load. */
  unsigned buffering_ms; /* Worker-managed refill; user pause remains separate. */
  int ext_cut_s, ext_speed; /* audio of another version: other cut (s) / other speed */
  int ext_ratio_ppm;        /* its speed from the frame rates (0: none) */
  int net_retries;          /* times a slow source was reconnected instead of giving up */
  int ext_starved;          /* audio of another version: 5 s windows in a row waiting for data with under 1 s of video ahead */
  int ext_failures;         /* audio of another version: times it stopped answering after its reconnections (back to the file's own audio) */
  int ext_stream_used;      /* audio of another version: its stream in use + 1 (0: not known yet) */
  int follow_state;         /* another cut followed: 0 off, 1 in sync, 2 own audio (a scene the other lacks), 3 lost */
  int follow_events, follow_kind, follow_offset_ms;   /* the latest follow event (dts_engine_external_follow_poll) */
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
void dts_playback_next_p7_mapping(int enabled);
void dts_playback_next_mp4_test(int enabled);
/* Explicit TV-test opt-in. Reuse a healthy native pipeline across compatible
 * seeks/audio changes, with bounded fresh-Load fallback. Default disabled. */
void dts_playback_next_seamless(int enabled);
/* Snapshot before start; 0 v12, 1 AAC stereo, 2 fixed AC3 5.1. DV only.
 * Consumed even if start is rejected; changes apply to the next source. */
void dts_playback_next_dv_audio(int mode);
/* The audio route of the next session (dts_audio_codec.h): TrueHD conversion
 * allowed, fixed DV audio mode, DTS row Off (2 = NV_DTS_ROUTE_NATIVE). */
void dts_playback_next_audio_route(int truehd, int fixed, int dts_off);
/* Optional initial account resume, resolved against demux duration before
 * the first native Load. Consumed even when the next start is rejected. */
void dts_playback_next_resume_pct(double percent);
/* Test: audio from another version of the same title. Main thread, right
 * before dts_playback_start; consumed by it. The session reads the video of
 * its own source and the audio (AC-3/E-AC-3/stereo AAC, preferred language
 * from `langs`) of this one, shifted by offset_ms. Ignored when the URL is
 * the session's own. Track changes are ignored while it is on. */
void dts_playback_next_external_audio(const char *url, const char *headers,
                                      const char *langs, int offset_ms);
/* New offset; it takes effect with the next seek (the caller seeks to the
 * current position right after, so the native timeline restarts cleanly). */
void dts_playback_external_offset(DtsPlayback *p, int offset_ms);
/* Main thread: a new offset applied at once without a seek or reload (only the
 * other file moves); falls back to a seek if that fails. */
void dts_playback_external_offset_live(DtsPlayback *p, int offset_ms);
/* The same, made at a quiet moment of the other version's audio (a change
 * from an automatic check while playing; dts_engine_external_shift_quiet). */
void dts_playback_external_offset_quiet(DtsPlayback *p, int offset_ms);
/* Main thread: "Match automatically" (about 30 s of playback, longer when it
 * searches further). The result, once: 0 still working, 1 found, -1 not
 * trusted or failed (details in DtsMatchInfo, dts_engine.h). A find can also
 * come later on its own: the second look (how 5) measured another speed. */
void dts_playback_external_match(DtsPlayback *p);
/* LG (owner 26/11): the audio refine (+-1 s, dts_engine_external_match_fine);
 * this file's track the match compares (container index, -1 Auto); the edit
 * map (n stretches from ini_s, this file's time, each with its offset ms). */
void dts_playback_external_match_fine(DtsPlayback *p);
void dts_playback_external_match_track(DtsPlayback *p, int stream);
void dts_playback_external_map(DtsPlayback *p, int n, const double *ini_s, const int *ms);
int dts_playback_external_match_result(DtsPlayback *p, DtsMatchInfo *out);
/* Main thread: the other version's speed for the next swap (a match's
 * ratio_ppm; 0 the same speed). And before a start, consumed by it. */
void dts_playback_external_speed(DtsPlayback *p, int ratio_ppm);
void dts_playback_next_external_speed(int ratio_ppm);
/* Before a start: follow another cut through the whole film (Settings). */
void dts_playback_next_external_follow(int on);
void dts_playback_next_external_automatch(int on);   /* Settings: match automatically at start */
/* Series memory: the track chosen in the last episode's version (stream -1 unknown). */
void dts_playback_next_external_hint(int stream, const char *lang, const char *codec, int channels, const char *title);
/* Main thread: a jump to `seconds` is coming (an intro skip): its bytes are
 * fetched ahead (dts_engine_prefetch). */
void dts_playback_prefetch(DtsPlayback *p, double seconds);
/* Before start, with the call above: fetch only the audio of that file where
 * it allows it (dts_engine_external_audio_only). Consumed by start. */
void dts_playback_next_external_audio_only(int on);
/* Main thread, before the start: the donor's audio stream picked in the menu
 * (-1 = by language). Consumed by that start. */
void dts_playback_next_external_track(int stream);
/* Main thread, on a session playing another version's audio: switch to another
 * version (or another track of it) at `seconds` without reopening this source;
 * only the other file is opened and the TV gets a fresh Load. 1 = queued, 0 =
 * not an external session (start a new one). */
int dts_playback_external_swap(DtsPlayback *p, const char *url, const char *headers, const char *langs,
                               int offset_ms, int track, int audio_only, double seconds);
int dts_playback_external(DtsPlayback *p);
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
/* One worker command: change audio at this position. A pending explicit seek
 * takes precedence. Avoid a separately signalled seek before the track change. */
void dts_playback_audio_at(DtsPlayback *p, int stream, double seconds);
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
