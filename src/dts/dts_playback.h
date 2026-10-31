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
#ifdef NV_DV7_MAPPING_TEST
void dts_playback_next_p7_mapping(int enabled);
#endif
void dts_playback_next_mp4_test(int enabled);
/* Explicit TV-test opt-in. Reuse a healthy native pipeline across compatible
 * seeks/audio changes, with bounded fresh-Load fallback. Default disabled. */
void dts_playback_next_seamless(int enabled);
/* Snapshot before start; 0 v12, 1 AAC stereo, 2 fixed AC3 5.1. DV only.
 * Consumed even if start is rejected; changes apply to the next source. */
void dts_playback_next_dv_audio(int mode);
/* Optional initial account resume, resolved against demux duration before
 * the first native Load. Consumed even when the next start is rejected. */
void dts_playback_next_resume_pct(double percent);
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
