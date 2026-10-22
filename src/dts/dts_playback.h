#ifndef NV_DTS_PLAYBACK_H
#define NV_DTS_PLAYBACK_H
#include "dts_media.h"
#include "dts_subtitles.h"
typedef struct DtsPlayback DtsPlayback;
typedef struct {
  int prepared, failed, eof, revision, native_loaded;
  DtsMediaInfo info;
  char media_id[64], error[160];
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
