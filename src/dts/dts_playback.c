#include "../app_id.h"
#include "dts_playback.h"
#include "dts_engine.h"
#include "dts_pipeline.h"
#include "../js.h"
#include "../linguas.h"
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DTS_EVENTS 32
#define DTS_EVENT_SIZE 2048
#define DTS_SUB_CUES 64
#define DTS_SUB_BYTES (16u * 1024u * 1024u)
/* Keep enough media queued to cover multi-second HTTP range reads on TVs.
 * Compressed byte limits in the native pipeline remain the hard memory cap. */
#define DTS_FEED_AHEAD_SECONDS 6.0
#define DTS_STALL_SECONDS 8.0
#define DTS_STALL_SEEK_SECONDS 2.0
#define DTS_STALL_RETRIES 2
typedef struct { DtsSubtitle cue; unsigned revision; } DtsQueuedSubtitle;
#define DTS_DV_STARVED_WINDOWS 4
struct DtsPlayback {
  int dv, starved; /* Dolby Vision path: passthrough audio, starvation fallback. */
  pthread_t thread;
  pthread_mutex_t lock;
  pthread_cond_t wake;
  DtsEngine *engine;
  DtsPipeline *pipeline;
  DtsSubtitles *subtitle_decoder;
  DtsQueuedSubtitle subtitles[DTS_SUB_CUES];
  int subtitle_count, subtitle_stream, subtitle_dirty;
  size_t subtitle_bytes;
  unsigned subtitle_revision;
  double subtitle_cursor;
  char url[4096], headers[2048], window[64];
  char events[DTS_EVENTS][DTS_EVENT_SIZE];
  int event_read, event_write, event_count;
  int stop, pause, pause_dirty, seek_dirty, audio_dirty, volume, volume_dirty;
  int audio_stream, major, native_audio_error, load_complete, suppress_events, clock_seen;
  DtsTrack selected;
  int source_ordinal, source_count, selected_known;
  double target, presented, preroll_target;
  DtsPlaybackStatus status;
};

static void clearSubtitles(DtsPlayback *p) {
  pthread_mutex_lock(&p->lock);
  for (int i = 0; i < p->subtitle_count; i++) free(p->subtitles[i].cue.rgba);
  p->subtitle_count = 0; p->subtitle_bytes = 0; p->subtitle_revision++;
  pthread_mutex_unlock(&p->lock);
  dts_subtitles_flush(p->subtitle_decoder);
}
/* Worker only: re-create the decoder after a demux reopen; its parameters are
 * borrowed from that particular FFmpeg context. */
static int selectSubtitle(DtsPlayback *p, int stream) {
  const DtsMediaInfo *info = dts_engine_info(p->engine);
  if (stream < -1) {
    int ordinal = -2 - stream, count = 0;
    stream = -1;
    for (int i = 0; info && i < info->n_tracks; i++)
      if (info->tracks[i].kind == DTS_SUBTITLE && count++ == ordinal) {
        stream = info->tracks[i].stream_index; break;
      }
  }
  clearSubtitles(p);
  dts_subtitles_destroy(p->subtitle_decoder); p->subtitle_decoder = NULL;
  if (stream >= 0) {
    const void *params = dts_engine_subtitle_parameters(p->engine, stream);
    if (!params || !(p->subtitle_decoder = dts_subtitles_create(params))) return 0;
  }
  pthread_mutex_lock(&p->lock);
  p->subtitle_stream = stream;
  pthread_mutex_unlock(&p->lock);
  return 1;
}

int dts_playback_enabled(void) {
#if defined(NV_WEBOS) && defined(NV_DTS_FFMPEG)
  /* Native playback remains first; this only makes error-triggered conversion
   * available when the shipped decoder and firmware adapter can run. */
  return dts_engine_available() && dts_pipeline_available(0);
#else
  return 0;
#endif
}

static void nativeEvent(void *user, const char *json) {
  DtsPlayback *p = user;
  if (!json) return;
  pthread_mutex_lock(&p->lock);
  if (!p->stop && !p->suppress_events) {
    if (js_num(json, NULL, "errorCode", -1) == 200) {
      p->native_audio_error = 1;
      pthread_cond_signal(&p->wake);
      pthread_mutex_unlock(&p->lock);
      return;
    }
    if (js_tem(json, NULL, "loadCompleted") && !p->load_complete) {
      p->load_complete = 1; p->pause_dirty = 1; p->volume_dirty = 1;
    }
    {
      double time = js_num(json, NULL, "currentTime", -1);
      if (isfinite(time) && time >= 0 && time < 9e12) {
        p->presented = time / 1000.0;
        p->clock_seen = 1;
      }
    }
    if (js_tem(json, NULL, "currentTime") && p->event_count) {
      int last = (p->event_write + DTS_EVENTS - 1) % DTS_EVENTS;
      if (js_tem(p->events[last], NULL, "currentTime") && strlen(json) < DTS_EVENT_SIZE) {
        snprintf(p->events[last], DTS_EVENT_SIZE, "%s", json);
        pthread_mutex_unlock(&p->lock);
        return;
      }
    }
    /* Overflow is a failure, never discard a potentially important event. */
    if (p->event_count == DTS_EVENTS || strlen(json) >= DTS_EVENT_SIZE) {
      p->status.failed = 1;
      snprintf(p->status.error, sizeof p->status.error, "DTS pipeline event queue overflow");
    } else {
      snprintf(p->events[p->event_write], DTS_EVENT_SIZE, "%s", json);
      p->event_write = (p->event_write + 1) % DTS_EVENTS;
      p->event_count++;
    }
  }
  pthread_cond_signal(&p->wake);
  pthread_mutex_unlock(&p->lock);
}
static void fail(DtsPlayback *p, const char *error) {
  pthread_mutex_lock(&p->lock);
  p->status.failed = 1;
  snprintf(p->status.error, sizeof p->status.error, "%s", error ? error : "DTS playback failed");
  pthread_mutex_unlock(&p->lock);
}
static int stopped(DtsPlayback *p) {
  int value;
  pthread_mutex_lock(&p->lock);
  value = p->stop || p->status.failed;
  pthread_mutex_unlock(&p->lock);
  return value;
}
static void rest(DtsPlayback *p) {
  struct timespec t;
  clock_gettime(CLOCK_REALTIME, &t);
  t.tv_nsec += 10000000;
  if (t.tv_nsec >= 1000000000) { t.tv_sec++; t.tv_nsec -= 1000000000; }
  pthread_mutex_lock(&p->lock);
  if (!p->stop) pthread_cond_timedwait(&p->wake, &p->lock, &t);
  pthread_mutex_unlock(&p->lock);
}
/* Codecs this session can feed: DTS is converted; on the Dolby Vision path
 * AC-3 and E-AC-3 pass through. Anything else belongs to the native player. */
static int codecFeeds(const DtsPlayback *p, const char *codec) {
  return !strcmp(codec, "dts") || (p->dv && (!strcmp(codec, "ac3") || !strcmp(codec, "eac3")));
}
static int trackMatches(DtsPlayback *p, const DtsTrack *t) {
  return t->kind == DTS_AUDIO && codecFeeds(p, t->codec) &&
      (!p->selected.language[0] || ling_casa(t->language, p->selected.language)) &&
      (!p->selected.channels || t->channels == p->selected.channels);
}
static double clockSeconds(clockid_t clock);
static int prepare(DtsPlayback *p, int stream, int core, double target) {
  const DtsMediaInfo *info;
  /* A native Load is single-use. Retire its callbacks before reconfiguration. */
  pthread_mutex_lock(&p->lock);
  p->suppress_events = 1;
  p->status.prepared = 0; p->status.media_id[0] = 0;
  p->load_complete = 0;
  pthread_mutex_unlock(&p->lock);
  dts_pipeline_destroy(p->pipeline); p->pipeline = NULL;
  pthread_mutex_lock(&p->lock);
  p->event_count = p->event_read = p->event_write = 0;
  p->load_complete = 0; p->native_audio_error = 0; p->volume_dirty = 1;
  p->presented = p->preroll_target = target; p->clock_seen = 0;
  p->suppress_events = 0;
  pthread_mutex_unlock(&p->lock);
  /* Reloading selects the exact same source stream and resets encoder delay. */
  double began = clockSeconds(CLOCK_MONOTONIC), opened;
  nativeEvent(p,"{\"dtsStage\":{\"name\":\"source-open-requested\"}}");
  if (dts_engine_open(p->engine, p->url, p->headers, stream, core, target) < 0) {
    fail(p, dts_engine_error(p->engine)); return 0;
  }
  info = dts_engine_info(p->engine);
  if (stream < 0 && info) {
    int i, count = 0, desired = -1, matches = 0, first_audio = -1;
    for (i = 0; i < info->n_tracks; i++) {
      const DtsTrack *t = &info->tracks[i];
      if (t->kind != DTS_AUDIO) continue;
      if (first_audio < 0) first_audio = t->stream_index;
      if (count++ == p->source_ordinal) desired = t->stream_index;
    }
    if (!p->source_count) desired = first_audio;
    int ordinal_matches = !p->selected_known;
    for (i = 0; i < info->n_tracks; i++)
      if (info->tracks[i].stream_index == desired && trackMatches(p, &info->tracks[i]))
        ordinal_matches = 1;
    if ((p->source_count && count != p->source_count) || !ordinal_matches) {
      /* A reordered or filtered native list needs a unique metadata match. */
      desired = -1;
      for (i = 0; i < info->n_tracks; i++) {
        const DtsTrack *t = &info->tracks[i];
        if (p->selected_known && p->selected.language[0] && trackMatches(p, t)) {
          desired = t->stream_index; matches++;
        }
      }
      if (matches != 1) desired = -1;
    }
    for (i = 0; i < info->n_tracks; i++)
      if (info->tracks[i].stream_index == desired &&
          !codecFeeds(p, info->tracks[i].codec)) desired = -1;
    if (desired < 0) { fail(p, "Cannot safely identify the selected DTS track"); return 0; }
    if (desired != info->audio_stream) {
      if (dts_engine_open(p->engine, p->url, p->headers, desired, core, target) < 0) {
        fail(p, dts_engine_error(p->engine)); return 0;
      }
      info = dts_engine_info(p->engine);
    }
  }
  /* #203: stage lines print only after prepare, so they carry no time of
   * their own. Say how long the source open and the native Load took. */
  opened = clockSeconds(CLOCK_MONOTONIC);
  {
    char event[128];
    snprintf(event, sizeof event, "{\"dtsStage\":{\"name\":\"source-opened\",\"detail\":\"ms=%.0f\"}}",
             (opened - began) * 1000);
    nativeEvent(p, event);
  }
  p->pipeline = dts_pipeline_create(NV_APP_ID, p->window,
                                    p->major, nativeEvent, p);
  if (!p->pipeline) { fail(p, "DTS native pipeline unavailable"); return 0; }
  if (!info || !dts_pipeline_load(p->pipeline, info, target)) {
    fail(p, dts_pipeline_error(p->pipeline)); return 0;
  }
  {
    char event[128];
    snprintf(event, sizeof event, "{\"dtsStage\":{\"name\":\"native-load-returned\",\"detail\":\"ms=%.0f\"}}",
             (clockSeconds(CLOCK_MONOTONIC) - opened) * 1000);
    nativeEvent(p, event);
  }
  pthread_mutex_lock(&p->lock);
  p->status.info = *info;
  p->status.prepared = 1;
  p->status.revision++;
  p->status.eof = 0;
  snprintf(p->status.media_id, sizeof p->status.media_id, "%s",
           dts_pipeline_media_id(p->pipeline));
  pthread_mutex_unlock(&p->lock);
  {
    int stream;
    pthread_mutex_lock(&p->lock); stream = p->subtitle_stream; pthread_mutex_unlock(&p->lock);
    if (!selectSubtitle(p, stream)) { fail(p, "DTS subtitle decoder unavailable"); return 0; }
  }
  return 1;
}
/* Retry the same selected stream using its DTS core once, resuming at the
 * native presentation clock rather than a timestamp fed ahead of playback. */
static int recover(DtsPlayback *p, int *core, int *core_tried,
                   int stream, double target, int allow_core) {
  if (!allow_core || *core_tried || *core || !dts_engine_has_core(p->engine)) return 0;
  *core_tried = 1; *core = 1;
  return prepare(p, stream, *core, target);
}
static double clockSeconds(clockid_t clock) {
  struct timespec t;
  return clock_gettime(clock, &t) ? -1 : t.tv_sec + t.tv_nsec / 1e9;
}
typedef struct {
  double start, read_wall, decode_cpu, feed_wall;
  unsigned audio, video, full, paced;
} DtsRate;
typedef struct {
  double position, changed_at, full_since;
  int advanced;
} DtsStall;
static void resetStall(DtsStall *stall, double position) {
  *stall = (DtsStall){.position = position, .changed_at = clockSeconds(CLOCK_MONOTONIC), .full_since = -1};
}
static int stalled(const DtsStall *s, double now, double limit) {
  return s->advanced && s->full_since >= 0 && now >= 0 &&
         now - s->full_since >= limit && now - s->changed_at >= limit;
}
static void reportRate(DtsPlayback *p, DtsRate *rate, double presented,
                       double audio_pts, double video_pts, int clock_seen,
                       int pause, int complete, int preroll) {
  double now = clockSeconds(CLOCK_MONOTONIC);
  if (now < 0 || now - rate->start < 5.0) return;
  char event[512];
  snprintf(event, sizeof event,
    "{\"dtsStage\":{\"name\":\"worker-rate\",\"detail\":\"pause=%d ready=%d preroll=%d wall=%.1f readMs=%.0f cpuMs=%.0f feedMs=%.0f a=%u v=%u full=%u pace=%u clock=%d at=%.2f aheadA=%.2f aheadV=%.2f\"}}",
    pause, complete, preroll, now-rate->start, rate->read_wall*1000, rate->decode_cpu*1000, rate->feed_wall*1000,
    rate->audio, rate->video, rate->full, rate->paced, clock_seen, presented,
    audio_pts-presented, video_pts-presented);
  nativeEvent(p, event);
  /* Dolby Vision path: the source cannot keep up when, window after window,
   * the worker spends its time waiting for the network with under a second of
   * video ahead. Four windows (20 s) hand playback back to the native player. */
  if (p->dv && complete && clock_seen && !pause && !preroll &&
      video_pts - presented < 1.0 && rate->read_wall > 0.5 * (now - rate->start)) p->starved++;
  else if (!pause) p->starved = 0;
  memset(rate, 0, sizeof *rate); rate->start = now;
}
static void *run(void *user) {
  DtsPlayback *p = user;
  DtsFrame frame;
  int have_frame = 0, paused = 0, eof = 0;
  int seek_preroll = 0, seek_audio = 0, seek_video = 0;
  int stream = p->audio_stream, core = 0, core_tried = 0;
  double position = p->target, cpu_window = clockSeconds(CLOCK_MONOTONIC), cpu_used = 0;
  double fed_audio = position, fed_video = position;
  DtsRate rate = {.start = cpu_window};
  DtsStall stall;
  unsigned stall_retries = 0;
  int native_unhealthy = 0;
  int recovery_waiting = 0;
  resetStall(&stall, position);
  if (!prepare(p, stream, core, position)) goto done;
  stream = dts_engine_info(p->engine)->audio_stream;
  for (;;) {
    int stop, pause, pause_dirty, seek, audio, new_stream, audio_error, complete, clock_seen;
    int subtitle, subtitle_stream, volume, volume_dirty;
    double target, presented, preroll;
    pthread_mutex_lock(&p->lock);
    stop = p->stop || p->status.failed;
    pause = p->pause; pause_dirty = p->pause_dirty; p->pause_dirty = 0;
    seek = p->seek_dirty; p->seek_dirty = 0;
    audio = p->audio_dirty; p->audio_dirty = 0;
    new_stream = p->audio_stream; target = p->target;
    audio_error = p->native_audio_error; p->native_audio_error = 0;
    complete = p->load_complete;
    clock_seen = p->clock_seen;
    presented = p->presented; preroll = p->preroll_target;
    volume = p->volume; volume_dirty = p->volume_dirty; p->volume_dirty = 0;
    subtitle = p->subtitle_dirty; p->subtitle_dirty = 0; subtitle_stream = p->subtitle_stream;
    pthread_mutex_unlock(&p->lock);
    if (stop) break;
    double wall = clockSeconds(CLOCK_MONOTONIC);
    if (clock_seen && presented > stall.position + 0.001) {
      stall.advanced = 1; stall.position = presented; stall.changed_at = wall;
      native_unhealthy = 0;
      recovery_waiting = 0;
    }
    if ((recovery_waiting || (complete && clock_seen)) && stalled(&stall, wall, DTS_STALL_SEEK_SECONDS)) native_unhealthy = 1;
    if (pause || (!recovery_waiting && (!complete || !clock_seen || seek_preroll)) || eof) stall.full_since = -1;
    reportRate(p, &rate, presented, fed_audio, fed_video, clock_seen, pause, complete, seek_preroll);
    /* Media IDs may appear only after native Load/preroll. Publish them even
     * when the source metadata revision has not changed; ACB needs the actual
     * firmware identity, never the app ID or an invented placeholder. */
    {
      const char *media_id = dts_pipeline_media_id(p->pipeline);
      pthread_mutex_lock(&p->lock);
      snprintf(p->status.media_id,sizeof p->status.media_id,"%s",media_id ? media_id : "");
      pthread_mutex_unlock(&p->lock);
    }
    if (audio_error) {
      fail(p, "DTS stereo output is not supported by this pipeline");
      break;
    }
    if (p->starved >= DTS_DV_STARVED_WINDOWS) {
      fail(p, DTS_PLAYBACK_STARVED);
      break;
    }
    int seek_reload = !audio && seek;
    int stall_seek = seek_reload && native_unhealthy;
    int stall_reload = !audio && !seek && !pause && !eof && (recovery_waiting || (complete && clock_seen)) &&
                       stalled(&stall, wall, DTS_STALL_SECONDS);
    if (stall_reload || seek_reload) {
      if (stall_reload && stall_retries >= DTS_STALL_RETRIES) {
        char event[256];
        snprintf(event, sizeof event,
          "{\"dtsStage\":{\"name\":\"native-stall-failed\",\"detail\":\"at=%.3f ready=%d clock=%d blocked=%d pts=%.3f stale=%.1fs\"}}",
          presented, complete, clock_seen, have_frame ? frame.kind : 0,
          have_frame ? frame.pts_ns / 1e9 : 0, wall - stall.changed_at);
        nativeEvent(p, event);
        fail(p, "DTS native playback stalled after two recovery attempts"); break;
      }
      if (stall_reload) { target = presented; stall_retries++; }
      double stale_seconds = wall - stall.changed_at;
      int blocked_kind = have_frame ? frame.kind : 0;
      double blocked_pts = have_frame ? frame.pts_ns / 1e9 : 0;
      /* BUFFERSTREAM flush leaves the old segment/decoder timeline in place
       * on these TVs. Every seek needs a fresh Load with ptsToDecode and new
       * codec preroll, including healthy playback and paused scrubbing. */
      if (!prepare(p, stream, core, target)) break;
      char event[256];
      snprintf(event, sizeof event,
        "{\"dtsStage\":{\"name\":\"%s\",\"detail\":\"at=%.3f attempt=%u seek=%d blocked=%d pts=%.3f stale=%.1fs\"}}",
        stall_reload || stall_seek ? "native-stall-reload" : "native-seek-reload",
        target, stall_retries, seek_reload, blocked_kind, blocked_pts, stale_seconds);
      nativeEvent(p, event);
      position = preroll = presented = target; complete = clock_seen = 0;
      fed_audio = fed_video = target;
      have_frame = eof = 0; seek_preroll = seek_reload; seek_audio = seek_video = 0;
      pause_dirty = volume_dirty = 1;
      resetStall(&stall, target);
      /* This instance follows a seek or proven stall. If it loads but never
       * returns a fresh clock, continuous refusal must still exhaust the
       * bounded recovery budget; ordinary initial startup is excluded. */
      recovery_waiting = stall.advanced = 1;
      native_unhealthy = 0;
      cpu_window = clockSeconds(CLOCK_MONOTONIC); cpu_used = 0;
    } else if (audio) {
      seek_preroll = seek; seek_audio = seek_video = 0;
      stream = new_stream; core = core_tried = 0;
      if (!prepare(p, stream, core, target)) break;
      position = preroll = presented = target; complete = clock_seen = 0;
      fed_audio = fed_video = target;
      have_frame = 0; eof = 0; pause_dirty = 1; volume_dirty = 1;
      cpu_window = clockSeconds(CLOCK_MONOTONIC); cpu_used = 0;
      resetStall(&stall, target);
      native_unhealthy = 0;
      recovery_waiting = stall.advanced = !!seek;
    }
    if (subtitle && !selectSubtitle(p, subtitle_stream)) {
      fail(p, "DTS subtitle decoder unavailable"); break;
    }
    if ((complete && pause_dirty) || paused != pause) {
      if (!(pause ? dts_pipeline_pause(p->pipeline) : dts_pipeline_play(p->pipeline))) {
        fail(p, "DTS pause/play failed"); break;
      }
      paused = pause;
    }
    /* Native volume is optional. Reapply after each Load completion. */
    if (volume_dirty) (void)dts_pipeline_volume(p->pipeline, volume);
    /* Native load completion needs preroll even when opened paused. */
    if (paused && !seek_preroll && (complete || position > preroll + 2.0)) {
      cpu_window = clockSeconds(CLOCK_MONOTONIC); cpu_used = 0;
      rest(p); continue;
    }
    if (eof) { rest(p); continue; }
    if (!have_frame) {
      double begun = clockSeconds(CLOCK_THREAD_CPUTIME_ID);
      double read_started = clockSeconds(CLOCK_MONOTONIC);
      int r = dts_engine_next(p->engine, &frame);
      double ended = clockSeconds(CLOCK_THREAD_CPUTIME_ID);
      double now = clockSeconds(CLOCK_MONOTONIC);
      if (begun >= 0 && ended >= begun) cpu_used += ended - begun;
      if (begun >= 0 && ended >= begun) rate.decode_cpu += ended - begun;
      if (read_started >= 0 && now >= read_started) rate.read_wall += now - read_started;
      if (stopped(p)) break;
      const char *error = r < 0 ? dts_engine_error(p->engine) : "";
      int decode_error = r < 0 && !strncmp(error, "DTS decode", 10);
      int encoder_error = r < 0 && (!strncmp(error, "Audio encode", 12) ||
                                   !strncmp(error, "Audio output", 12));
      int cpu_overrun = r > 0 && cpu_window >= 0 && now - cpu_window >= 5.0 &&
                        cpu_used > (now - cpu_window) * 0.80;
      if (now >= 0 && now - cpu_window >= 5.0) { cpu_window = now; cpu_used = 0; }
      if (decode_error || encoder_error || cpu_overrun) {
        pthread_mutex_lock(&p->lock); presented = p->presented; pthread_mutex_unlock(&p->lock);
        if (recover(p, &core, &core_tried, stream, presented, decode_error || cpu_overrun)) {
          seek_preroll = 0;
          position = presented; have_frame = 0; eof = 0;
          fed_audio = fed_video = presented;
          cpu_window = clockSeconds(CLOCK_MONOTONIC); cpu_used = 0;
          resetStall(&stall, presented);
          native_unhealthy = 0;
          recovery_waiting = 0;
          continue;
        }
        if (!stopped(p)) fail(p, cpu_overrun ? "DTS conversion exceeds the sustained CPU budget" : error);
        break;
      }
      if (r < 0) { fail(p, error); break; }
      if (r == 0) {
        seek_preroll = 0;
        if (!dts_pipeline_eos(p->pipeline)) { fail(p, "DTS end of stream failed"); break; }
        pthread_mutex_lock(&p->lock);
        p->status.eof = 1;
        pthread_mutex_unlock(&p->lock);
        eof = 1;
        continue;
      }
      if ((frame.kind == DTS_VIDEO && frame.size > 16 * 1024 * 1024) ||
          (frame.kind == DTS_AUDIO && frame.size > 2 * 1024 * 1024)) {
        fail(p, "DTS packet exceeds playback memory budget"); break;
      }
      have_frame = 1;
    }
    if (frame.kind == DTS_SUBTITLE) {
      DtsSubtitle cue;
      int selected;
      pthread_mutex_lock(&p->lock); selected = p->subtitle_stream; pthread_mutex_unlock(&p->lock);
      if (frame.stream_index == selected && p->subtitle_decoder) {
        int r = dts_subtitles_decode(p->subtitle_decoder, &frame, &cue);
        if (r < 0) { fail(p, "DTS subtitle packet decode failed"); break; }
        if (r) {
          size_t bytes = cue.rgba ? (size_t)cue.w * (size_t)cue.h * 4u : 0;
          unsigned char *pixels = bytes ? malloc(bytes) : NULL;
          if (bytes && !pixels) { fail(p, "DTS subtitle allocation failed"); break; }
          if (bytes) memcpy(pixels, cue.rgba, bytes);
          pthread_mutex_lock(&p->lock);
          if (p->subtitle_count == DTS_SUB_CUES || bytes > DTS_SUB_BYTES - p->subtitle_bytes) {
            pthread_mutex_unlock(&p->lock); free(pixels);
            fail(p, "DTS subtitle queue exceeds memory budget"); break;
          }
          if (p->subtitle_count) {
            DtsSubtitle *previous = &p->subtitles[p->subtitle_count - 1].cue;
            if (previous->end_ns == INT64_MAX) previous->end_ns = cue.start_ns;
          }
          cue.rgba = pixels;
          p->subtitles[p->subtitle_count].cue = cue;
          p->subtitles[p->subtitle_count++].revision = ++p->subtitle_revision;
          p->subtitle_bytes += bytes;
          pthread_mutex_unlock(&p->lock);
        }
      }
      have_frame = 0; continue;
    }
    {
      /* Native byte limits can hold tens of seconds of compressed audio.
       * Bound the common A/V lead, while still reading skewed container order
       * until BOTH streams have lead. Stopping on one audio packet alone could
       * hide later video packets and stall the native video presentation clock.
       * Native byte backpressure remains the fallback for extreme skew. */
      if (complete && fed_audio - presented > DTS_FEED_AHEAD_SECONDS + 0.000001 &&
          fed_video - presented > DTS_FEED_AHEAD_SECONDS + 0.000001) {
        if (!pause && (clock_seen || recovery_waiting) && stall.full_since < 0) stall.full_since = clockSeconds(CLOCK_MONOTONIC);
        rate.paced++; rest(p); continue;
      }
      double feed_started = clockSeconds(CLOCK_MONOTONIC);
      int r = dts_pipeline_feed(p->pipeline, &frame);
      double feed_ended = clockSeconds(CLOCK_MONOTONIC);
      if (feed_started >= 0 && feed_ended >= feed_started) rate.feed_wall += feed_ended-feed_started;
      if (r < 0) { fail(p, dts_pipeline_error(p->pipeline)); break; }
      if (r) {
        stall.full_since = -1;
        position = frame.pts_ns / 1e9;
        /* A paused seek still needs the keyframe preroll and both streams at
         * its target; Load completion alone does not establish video preroll
         * for this decoder state. Resume can arrive during this feed. */
        if (seek_preroll && position >= preroll) {
          if (frame.kind == DTS_AUDIO) seek_audio = 1;
          else if (frame.kind == DTS_VIDEO) seek_video = 1;
          if (seek_audio && seek_video) seek_preroll = 0;
        }
        if (frame.kind == DTS_AUDIO) { rate.audio++; if (position > fed_audio) fed_audio = position; }
        else { rate.video++; if (position > fed_video) fed_video = position; }
        have_frame = 0;
      } else {
        rate.full++;
        if (!pause && (recovery_waiting || (complete && clock_seen && !seek_preroll)) && stall.full_since < 0)
          stall.full_since = feed_ended;
        rest(p);
      }
    }
  }
done:
  dts_subtitles_destroy(p->subtitle_decoder); p->subtitle_decoder = NULL;
  dts_pipeline_destroy(p->pipeline);
  p->pipeline = NULL;
  return NULL;
}

static int dvNext;
void dts_playback_next_dv(int enabled) { dvNext = !!enabled; }
DtsPlayback *dts_playback_start(const char *url, const char *headers,
                               int audio_stream, double position,
                               int paused, const char *window, int major,
                               const DtsTrack *selected, int ordinal, int count) {
  DtsPlayback *p;
  if (!url || !*url || strlen(url) >= 4096 || (headers && strlen(headers) >= 2048) ||
      (window && strlen(window) >= 64) || !isfinite(position) || position >= 9e9 || !dts_engine_available()) return NULL;
  p = calloc(1, sizeof *p);
  if (!p) return NULL;
  pthread_mutex_init(&p->lock, NULL);
  pthread_cond_init(&p->wake, NULL);
  p->engine = dts_engine_create();
  if (!p->engine) { dts_playback_close(p); return NULL; }
  p->dv = dvNext; dvNext = 0;
  dts_engine_dv(p->engine, p->dv);
  snprintf(p->url, sizeof p->url, "%s", url);
  snprintf(p->headers, sizeof p->headers, "%s", headers ? headers : "");
  snprintf(p->window, sizeof p->window, "%s", window ? window : "");
  p->audio_stream = audio_stream;
  p->volume = 100; p->volume_dirty = 1;
  p->subtitle_stream = -1;
  p->major = major; p->target = position < 0 ? 0 : position;
  p->presented = p->preroll_target = p->target;
  if (selected) { p->selected = *selected; p->selected_known = 1; }
  p->source_ordinal = ordinal; p->source_count = count;
  p->pause = !!paused; p->pause_dirty = 1;
  if (pthread_create(&p->thread, NULL, run, p)) {
    dts_engine_destroy(p->engine);
    pthread_cond_destroy(&p->wake); pthread_mutex_destroy(&p->lock); free(p); return NULL;
  }
  return p;
}
void dts_playback_status(DtsPlayback *p, DtsPlaybackStatus *out) {
  if (!p || !out) return;
  pthread_mutex_lock(&p->lock); *out = p->status; out->native_loaded = p->load_complete; pthread_mutex_unlock(&p->lock);
}
int dts_playback_event(DtsPlayback *p, char *json, size_t n) {
  int have;
  if (!p || !json || !n) return 0;
  pthread_mutex_lock(&p->lock);
  have = p->event_count != 0;
  if (have) {
    snprintf(json, n, "%s", p->events[p->event_read]);
    p->event_read = (p->event_read + 1) % DTS_EVENTS; p->event_count--;
  }
  pthread_mutex_unlock(&p->lock);
  return have;
}
void dts_playback_volume(DtsPlayback *p, int percent) {
  if (!p) return;
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  pthread_mutex_lock(&p->lock);
  p->volume = percent; p->volume_dirty = 1;
  pthread_cond_signal(&p->wake);
  pthread_mutex_unlock(&p->lock);
}
void dts_playback_pause(DtsPlayback *p, int paused) {
  if (!p) return;
  pthread_mutex_lock(&p->lock);
  if (p->pause != !!paused) { p->pause = !!paused; p->pause_dirty = 1; }
  pthread_cond_signal(&p->wake); pthread_mutex_unlock(&p->lock);
}
void dts_playback_seek(DtsPlayback *p, double seconds) {
  if (!p || !isfinite(seconds) || seconds >= 9e9) return;
  pthread_mutex_lock(&p->lock); p->target = seconds < 0 ? 0 : seconds; p->seek_dirty = 1;
  pthread_cond_signal(&p->wake); pthread_mutex_unlock(&p->lock);
}
void dts_playback_audio(DtsPlayback *p, int stream) {
  if (!p || stream < 0) return;
  pthread_mutex_lock(&p->lock);
  if (!p->seek_dirty) p->target = p->presented;
  p->audio_stream = stream; p->audio_dirty = 1;
  pthread_cond_signal(&p->wake); pthread_mutex_unlock(&p->lock);
}
void dts_playback_close(DtsPlayback *p) {
  if (!p) return;
  if (p->engine) {
    pthread_mutex_lock(&p->lock); p->stop = 1;
    dts_engine_cancel(p->engine);
    pthread_cond_signal(&p->wake); pthread_mutex_unlock(&p->lock);
    /* An engine implies a started worker except during allocation failure. */
    pthread_join(p->thread, NULL);
    dts_engine_destroy(p->engine);
  }
  pthread_cond_destroy(&p->wake); pthread_mutex_destroy(&p->lock);
  for (int i = 0; i < p->subtitle_count; i++) free(p->subtitles[i].cue.rgba);
  memset(p->headers, 0, sizeof p->headers);
  free(p);
}

void dts_playback_subtitle(DtsPlayback *p, int stream) {
  if (!p) return;
  pthread_mutex_lock(&p->lock);
  p->subtitle_stream = stream; p->subtitle_dirty = 1;
  pthread_cond_signal(&p->wake); pthread_mutex_unlock(&p->lock);
}
static DtsQueuedSubtitle *subtitleAt(DtsPlayback *p, double seconds) {
  int64_t time = (int64_t)(seconds * 1e9);
  int discard = 0;
  DtsQueuedSubtitle *selected = NULL;
  /* Retain five seconds of history for the existing subtitle-delay control. */
  while (discard < p->subtitle_count && p->subtitles[discard].cue.end_ns < time - 5000000000LL) {
    DtsSubtitle *cue = &p->subtitles[discard++].cue;
    if (cue->rgba) p->subtitle_bytes -= (size_t)cue->w * (size_t)cue->h * 4u;
    free(cue->rgba);
  }
  if (discard) {
    memmove(p->subtitles, p->subtitles + discard,
             (size_t)(p->subtitle_count - discard) * sizeof p->subtitles[0]);
    p->subtitle_count -= discard;
  }
  for (int i = 0; i < p->subtitle_count; i++)
    if (p->subtitles[i].cue.start_ns <= time && p->subtitles[i].cue.end_ns > time)
      selected = &p->subtitles[i];
  return selected;
}
int dts_playback_subtitle_text(DtsPlayback *p, double seconds, char *out, size_t n) {
  DtsQueuedSubtitle *cue;
  int present = 0;
  if (!out || !n) return 0;
  out[0] = 0;
  if (!p || !isfinite(seconds) || seconds < 0 || seconds >= 9e9) return 0;
  pthread_mutex_lock(&p->lock);
  cue = subtitleAt(p, seconds);
  if (cue && cue->cue.text[0]) { snprintf(out, n, "%s", cue->cue.text); present = 1; }
  pthread_mutex_unlock(&p->lock);
  return present;
}
int dts_playback_subtitle_bitmap(DtsPlayback *p, double seconds,
                                 unsigned previous, DtsSubtitle *out, unsigned *revision) {
  DtsQueuedSubtitle *cue;
  unsigned current;
  size_t bytes;
  if (!p || !out || !revision || !isfinite(seconds) || seconds < 0 || seconds >= 9e9) return 0;
  pthread_mutex_lock(&p->lock);
  cue = subtitleAt(p, seconds);
  current = cue && cue->cue.rgba ? cue->revision : 0;
  if (current == previous) { pthread_mutex_unlock(&p->lock); return 0; }
  memset(out, 0, sizeof *out);
  if (current) {
    *out = cue->cue;
    bytes = (size_t)out->w * (size_t)out->h * 4u;
    out->rgba = malloc(bytes);
    if (!out->rgba) { pthread_mutex_unlock(&p->lock); return 0; }
    memcpy(out->rgba, cue->cue.rgba, bytes);
  }
  *revision = current;
  pthread_mutex_unlock(&p->lock);
  return 1;
}
