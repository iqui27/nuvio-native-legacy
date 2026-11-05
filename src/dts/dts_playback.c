#include "../app_id.h"
#include "dts_playback.h"
#include "../mediaurl.h"
#include "dts_audio_codec.h"
#include "dts_engine.h"
#include "dts_pipeline.h"
#ifdef NV_HTTP_BUFFER_DIAG
#include "dts_video_diag.h"
#endif
#include "../js.h"
#include "../linguas.h"
#include "../audioinfo.h"
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
#define DTS_DV_SEEK_SECONDS 4.0
#define DTS_DV_SEEK_TOTAL_SECONDS 12.0
#define DTS_DV_AUDIO_HOLD_SECONDS 2.0
#define DTS_DV_AUDIO_HOLD_MAX_SECONDS 6.0
#define DTS_STALL_RETRIES 2
#define DTS_TIMESTAMP_RETRIES 2
/* Reset only after real presentation progress, not time spent reloading. */
#define DTS_TIMESTAMP_PROGRESS_SECONDS 30.0
typedef struct { DtsSubtitle cue; unsigned revision; } DtsQueuedSubtitle;
#define DTS_DV_STARVED_WINDOWS 4
#define DTS_REFILL_SECONDS 2.0
#define DTS_REFILL_TIMEOUT_SECONDS 20.0
#define DTS_REFILL_FULL_SECONDS 0.10
#define DTS_REFILL_MIN_SECONDS 0.25
struct DtsPlayback {
#ifdef NV_MP4_BD_TEST
  int mp4_recovery;
#endif
  int seamless, seamless_disabled, clock_guard;
  double clock_target, clock_anchor;
  int clock_advances;
#ifdef NV_HTTP_BUFFER_DIAG
  unsigned picture_epoch,picture_samples;
  int64_t picture_last_pts;
  size_t picture_last_size;
#endif
  int allow_p7, dv, starved; /* Dolby Vision path: passthrough audio, starvation fallback. */
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
  char url[NV_MEDIA_URL_MAX], headers[2048], window[64];
  char events[DTS_EVENTS][DTS_EVENT_SIZE];
  int event_read, event_write, event_count;
  int stop, pause, pause_dirty, seek_dirty, audio_dirty, volume, volume_dirty;
  int audio_stream, major, native_audio_error, load_complete, suppress_events, clock_seen;
  DtsTrack selected;
  int source_ordinal, source_count, selected_known;
  double target, presented, preroll_target, resume_pct;
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
    if (js_tem(json, NULL, "seekDone")) p->pause_dirty = 1;
    if (js_tem(json, NULL, "loadCompleted") && !p->load_complete) {
      p->load_complete = 1; p->pause_dirty = 1; p->volume_dirty = 1;
    }
    {
      double time = js_num(json, NULL, "currentTime", -1);
      if (isfinite(time) && time >= 0 && time < 9e12) {
        // Flush callbacks can still contain a clock from the retired segment.
        if(p->clock_guard && (time/1000.0<p->clock_target-1.0 || time/1000.0>(p->clock_seen ? p->clock_target+10.0 : p->clock_target+1.0))) {
          pthread_mutex_unlock(&p->lock);return;
        }
        if(p->clock_guard) {
          double seconds=time/1000.0;
          if(!p->clock_seen) p->clock_anchor=seconds;
          else if(seconds>p->clock_anchor+0.02) { p->clock_advances++;p->clock_anchor=seconds; }
        }
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
 * AC-3 and E-AC-3 pass through. The TrueHD experiment converts its audio;
 * anything else belongs to the native player. */
static int codecFeeds(const DtsPlayback *p, const char *codec) {
  return nv_dts_audio_feeds(codec,p->dv);
}
static int trackMatches(DtsPlayback *p, const DtsTrack *t) {
  const char *selected_codec = audioinfo_codec(p->selected.codec);
  const char *track_codec = audioinfo_codec(t->codec);
  // FFmpeg calls every DTS profile "dts"; keep DTS-HD/DTS:X in that family.
  if (!strncmp(selected_codec, "DTS", 3)) selected_codec = "DTS";
  if (!strncmp(track_codec, "DTS", 3)) track_codec = "DTS";
  return t->kind == DTS_AUDIO && codecFeeds(p, t->codec) &&
      (!p->selected.codec[0] || (selected_codec[0] && !strcmp(selected_codec, track_codec))) &&
      (!p->selected.language[0] || ling_casa(t->language, p->selected.language)) &&
      (!p->selected.channels || t->channels == p->selected.channels);
}
static double clockSeconds(clockid_t clock);
static void retirePipeline(DtsPlayback *p, double target) {
  dts_pipeline_destroy(p->pipeline); p->pipeline = NULL;
  pthread_mutex_lock(&p->lock);
  p->event_count = p->event_read = p->event_write = 0;
  p->load_complete = 0; p->native_audio_error = 0; p->volume_dirty = 1;
  p->presented = p->preroll_target = target; p->clock_seen = 0; p->clock_guard = 0;
  p->suppress_events = 0;
  pthread_mutex_unlock(&p->lock);
}
static int prepareModePrimed(DtsPlayback *p, int stream, int core, double target,
                             int reuse, DtsFrame *primed, int source_ready) {
  const DtsMediaInfo *info;
  int hold = primed && p->pipeline && p->dv && p->seamless;
  /* A native Load is single-use. Retire its callbacks before reconfiguration. */
  pthread_mutex_lock(&p->lock);
  p->suppress_events = 1;
  p->status.prepared = 0; p->status.media_id[0] = 0;
  p->status.buffer = (DtsBufferInfo){0};
  p->status.buffered_until = 0;
  p->status.buffering_ms = 0;
  p->load_complete = 0;
  pthread_mutex_unlock(&p->lock);
  /* Keep the existing DV plane paused while configuring the new audio and
   * fetching its first packet. Never run two native decoders or copy/queue a
   * second frame. A refused Pause retains the previous fresh-Load behavior. */
  if (hold && !dts_pipeline_pause(p->pipeline)) hold = 0;
  if (!hold) retirePipeline(p, target);
  /* Reloading selects the exact same source stream and resets encoder delay. */
  double began = clockSeconds(CLOCK_MONOTONIC), opened;
  int reused=source_ready,defer_start=stream<0 && target>0;
  info=dts_engine_info(p->engine);
  if(!reused && reuse && info && stream>=0) {
    nativeEvent(p,"{\"dtsStage\":{\"name\":\"source-seek-requested\"}}");
    reused=(info->audio_stream==stream ? dts_engine_seek(p->engine,target) :
            dts_engine_audio(p->engine,stream,core,target))>=0;
    if(!reused && stopped(p)) return 0;
    if(!reused) nativeEvent(p,"{\"dtsStage\":{\"name\":\"source-seek-reopen\"}}");
  }
  if(!reused) {
    nativeEvent(p,"{\"dtsStage\":{\"name\":\"source-open-requested\"}}");
    if (dts_engine_open(p->engine, p->url, p->headers, stream, core, defer_start?0:target) < 0) {
      fail(p, dts_engine_error(p->engine)); return 0;
    }
  }
  info = dts_engine_info(p->engine);
  /* The demuxer can prove native P5/P8 even when the UI's short MKV probe
   * could not. Keep the worker's seek/audio recovery policy on that same
   * path; never infer DV merely from a filename or HDR transfer function. */
  if(!p->dv && info && (info->dovi_profile==5 || info->dovi_profile==8) &&
     !strcmp(info->video_codec,"hevc") && info->dovi_bl_present &&
     info->dovi_rpu_present && !info->dovi_el_present) {
    p->dv=1;dts_engine_dv(p->engine,1);
    nativeEvent(p,"{\"dtsStage\":{\"name\":\"prepared-dv-route\"}}");
  }
  int pct_resume=p->resume_pct>0;
  if(pct_resume) {
    double pct=p->resume_pct;p->resume_pct=0;
    if(!info || info->duration_ns<=0) { fail(p,"Resume duration unavailable");return 0; }
    target=(info->duration_ns/1e9)*(pct/100.0);
    defer_start=1;
    pthread_mutex_lock(&p->lock);
    p->presented=p->preroll_target=target;
    if(!p->seek_dirty) p->target=target;
    pthread_mutex_unlock(&p->lock);
    nativeEvent(p,"{\"dtsStage\":{\"name\":\"startup-resume\"}}");
  }
  if (stream < 0 && info && !reused) {
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
    if (desired < 0) { fail(p, p->dv ? "Cannot safely identify the selected audio track" : "Cannot safely identify the selected DTS track"); return 0; }
    if (desired != info->audio_stream) {
      /* Mapping is validated against this open container. Reconfigure only
       * audio; keep its DV metadata, demux, index and bounded HTTP cache. */
      nativeEvent(p,"{\"dtsStage\":{\"name\":\"startup-audio-reuse\"}}");
      if (dts_engine_audio(p->engine, desired, core, target) < 0) {
        if(stopped(p)) return 0;
        nativeEvent(p,"{\"dtsStage\":{\"name\":\"startup-audio-reopen\"}}");
        if(dts_engine_open(p->engine,p->url,p->headers,desired,core,target)<0) {
          fail(p,dts_engine_error(p->engine));return 0;
        }
      }
      info = dts_engine_info(p->engine);
    } else if(defer_start && dts_engine_seek(p->engine,target)<0) {
      fail(p,dts_engine_error(p->engine));return 0;
    }
  }
  if(pct_resume && stream>=0 && dts_engine_seek(p->engine,target)<0) {
    fail(p,dts_engine_error(p->engine));return 0;
  }
  /* #203: stage lines print only after prepare, so they carry no time of
   * their own. Say how long the source open and the native Load took. */
  opened = clockSeconds(CLOCK_MONOTONIC);
  {
    char event[128];
    snprintf(event, sizeof event, "{\"dtsStage\":{\"name\":\"%s\",\"detail\":\"ms=%.0f\"}}",
             reused?"source-seeked":"source-opened",(opened - began) * 1000);
    nativeEvent(p, event);
  }
  if(p->dv && (!info || (info->dovi_profile!=5 && info->dovi_profile!=8))) {
    fail(p,"Dolby Vision configuration absent or unsupported");return 0;
  }
  if (hold) {
    int got = dts_engine_next(p->engine, primed);
    if (stopped(p)) return 0;
    if (got <= 0) {
      memset(primed, 0, sizeof *primed);
      /* Let the normal feed loop handle EOF, core retry or decode failure.
       * An unsuccessful prime must not bypass the existing recovery path. */
      if (got < 0 && dts_engine_seek(p->engine, target) < 0) {
        fail(p, dts_engine_error(p->engine)); return 0;
      }
    }
    printf("[dts] dv-audio-reload-prime target=%.3f ms=%.0f packet=%d kind=%d bytes=%zu\n",
      target,(clockSeconds(CLOCK_MONOTONIC)-began)*1000,got>0,
      got>0?primed->kind:0,got>0?primed->size:0);
    fflush(stdout);
    retirePipeline(p, target);
    nativeEvent(p,"{\"dtsStage\":{\"name\":\"dv-audio-prepared-before-unload\"}}");
    opened=clockSeconds(CLOCK_MONOTONIC);
  }
  if(p->dv && info) {
    char event[256];
    snprintf(event,sizeof event,
      "{\"dtsStage\":{\"name\":\"dv-native-load\",\"detail\":\"profile=%d compatibility=%d bl=%d el=%d rpu=%d reused=%d target=%.3f\"}}",
      info->dovi_profile,info->dovi_bl_compatibility_id,info->dovi_bl_present,
      info->dovi_el_present,info->dovi_rpu_present,reused,target);
    nativeEvent(p,event);
  }
#ifdef NV_HTTP_BUFFER_DIAG
  p->picture_epoch++;p->picture_samples=0;p->picture_last_pts=INT64_MIN;p->picture_last_size=0;
#endif
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
  p->status.start_position = target;
  p->status.revision++;
  p->status.load_revision++;
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
static int prepareMode(DtsPlayback *p, int stream, int core, double target,int reuse) {
  return prepareModePrimed(p,stream,core,target,reuse,NULL,0);
}
static int prepare(DtsPlayback *p,int stream,int core,double target) {
  return prepareMode(p,stream,core,target,0);
}
/* Opt-in TV experiment. A format change or an unhealthy renderer uses the
 * proven fresh-Load path. No packet queue or second native player is added. */
enum { REPOSITION_FAILED, REPOSITION_OK, REPOSITION_FORMAT_CHANGED };
static int reposition(DtsPlayback *p,int stream,int core,double target,DtsFrame *primed) {
  const DtsMediaInfo *info=dts_engine_info(p->engine);
  if(!info || !p->pipeline) return 0;
  DtsMediaInfo before=*info;
  pthread_mutex_lock(&p->lock);
  p->suppress_events=1;
  pthread_mutex_unlock(&p->lock);
  /* Hold the existing DV image while the demux seeks and fetches its first
   * packet. Borrow the engine's normal single pending frame: no second queue,
   * no additional video copy and no increase to the HTTP/native byte limits. */
  double began=clockSeconds(CLOCK_MONOTONIC);
  int ok=!p->dv || dts_pipeline_pause(p->pipeline);
  if(ok) ok=(stream==before.audio_stream ? dts_engine_seek(p->engine,target) :
          dts_engine_audio(p->engine,stream,core,target))>=0;
  info=dts_engine_info(p->engine);
  // An expected format change needs Load, not a permanent Seek failure flag.
  int format_changed=ok && info && (strcmp(before.audio_codec,info->audio_codec) ||
     before.channels!=info->channels || before.sample_rate!=info->sample_rate);
  ok=ok && info && !format_changed;
  if(ok && p->dv) {
    ok=dts_engine_next(p->engine,primed)>0;
    if(ok) {
      fprintf(stdout,"[dts] dv-seek-prime ms=%.0f kind=%d pts=%.3f bytes=%zu\n",
        (clockSeconds(CLOCK_MONOTONIC)-began)*1000,primed->kind,primed->pts_ns/1e9,primed->size);
      fflush(stdout);
    }
  }
  if(ok) {
    ok=dts_pipeline_flush(p->pipeline,target);
    /* This survives the seek's deliberate event-queue reset. */
    printf("[dts] native-seek-request target=%.3f accepted=%d dv=%d\n",target,ok,p->dv);fflush(stdout);
  }
  pthread_mutex_lock(&p->lock);
  p->event_count=p->event_read=p->event_write=0;
  if(ok) {
    p->status.info=*info;p->status.eof=0;
    if(stream!=before.audio_stream) p->status.revision++;
    p->status.buffer=(DtsBufferInfo){0};p->status.buffered_until=0;
    p->presented=p->preroll_target=target;p->clock_seen=0;
    p->clock_guard=1;p->clock_target=target;p->clock_advances=0;p->clock_anchor=target;
    p->pause_dirty=p->volume_dirty=1;
  }
  p->suppress_events=0;
  pthread_mutex_unlock(&p->lock);
#ifdef NV_HTTP_BUFFER_DIAG
  if(ok){p->picture_epoch++;p->picture_samples=0;p->picture_last_pts=INT64_MIN;p->picture_last_size=0;}
#endif
  if(format_changed) return REPOSITION_FORMAT_CHANGED; /* No duplicate demux seek. */
  if(!ok) return 0;
  int subtitle;
  pthread_mutex_lock(&p->lock);subtitle=p->subtitle_stream;pthread_mutex_unlock(&p->lock);
  if(!selectSubtitle(p,subtitle)) { fail(p,"DTS subtitle decoder unavailable");return 0; }
  nativeEvent(p,"{\"dtsStage\":{\"name\":\"native-buffered-reposition\"}}");
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
static void reportBuffer(DtsPlayback *p, double audio_pts, double video_pts) {
  DtsBufferInfo b;
  dts_engine_buffer_info(p->engine, &b);
  pthread_mutex_lock(&p->lock);
  if(p->stop || p->status.failed || !p->status.prepared || p->seek_dirty || p->audio_dirty) {
    p->status.buffer = (DtsBufferInfo){0};
    p->status.buffered_until = 0;
  } else {
    p->status.buffer = b;
    p->status.buffered_until = fmax(0, fmin(audio_pts, video_pts));
  }
  pthread_mutex_unlock(&p->lock);
}
#ifdef NV_DTS_WEBOS_RECOVERY
static int64_t refillGoal(const DtsBufferInfo *b) {
  int64_t goal=b->capacity_bytes/4;
  return goal>16*1048576LL?16*1048576LL:goal;
}
static int refillTail(const DtsBufferInfo *b) {
  return b->total>0 && b->position>=b->total-b->contiguous_bytes;
}
static int refillFullReady(double since,double wall,int pending,int preroll,int complete,
                           double audio,double video,double target) {
  return since>=0 && wall-since>=DTS_REFILL_FULL_SECONDS && pending && !preroll && complete &&
         audio>=target+DTS_REFILL_MIN_SECONDS && video>=target+DTS_REFILL_MIN_SECONDS;
}
static void refillPublish(DtsPlayback *p,int active,double elapsed) {
  unsigned ms=active?(unsigned)(fmax(0,elapsed)*1000)+1:0;
  pthread_mutex_lock(&p->lock);
  p->status.buffering_ms=ms;
  pthread_mutex_unlock(&p->lock);
}
#endif
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
  unsigned stall_retries = 0, timestamp_retries = 0;
  double timestamp_anchor = position;
  int native_unhealthy = 0;
  int recovery_waiting = 0, fast_pending = 0;
#ifdef NV_MP4_BD_TEST
  int queued_seek=0;double queued_target=0;
#endif
  int dv_audio_hold=0;
#ifdef NV_DTS_WEBOS_RECOVERY
  int network_hold=0;
  double network_since=0,network_target=0,refill_full_since=-1;
#else
  const int network_hold=0;
#endif
  double hold_since=0,hold_target=0;
  double fast_since=0, fast_started=0, fast_target=0;
  resetStall(&stall, position);
  if (!prepare(p, stream, core, position)) goto done;
  pthread_mutex_lock(&p->lock);position=p->preroll_target;pthread_mutex_unlock(&p->lock);
  timestamp_anchor=position;resetStall(&stall,position);
  stream = dts_engine_info(p->engine)->audio_stream;
  for (;;) {
    int stop, pause, pause_dirty, seek, audio, new_stream, audio_error, complete, clock_seen;
    int subtitle, subtitle_stream, volume, volume_dirty, clock_advances;
    double target, presented, preroll;
    pthread_mutex_lock(&p->lock);
    stop = p->stop || p->status.failed;
    pause = p->pause; pause_dirty = p->pause_dirty; p->pause_dirty = 0;
    seek = p->seek_dirty; p->seek_dirty = 0;
    audio = p->audio_dirty; p->audio_dirty = 0;
    new_stream = p->audio_stream; target = p->target;
    audio_error = p->native_audio_error; p->native_audio_error = 0;
    complete = p->load_complete;
    clock_seen = p->clock_seen;clock_advances=p->clock_advances;
    presented = p->presented; preroll = p->preroll_target;
    volume = p->volume; volume_dirty = p->volume_dirty; p->volume_dirty = 0;
    subtitle = p->subtitle_dirty; p->subtitle_dirty = 0; subtitle_stream = p->subtitle_stream;
    pthread_mutex_unlock(&p->lock);
    if (stop) break;
    double wall = clockSeconds(CLOCK_MONOTONIC);
#ifdef NV_DTS_WEBOS_RECOVERY
    if(!network_hold && !dv_audio_hold)refill_full_since=-1;
    if(network_hold && (seek || audio)) {
      refill_full_since=-1;
      network_hold=0;refillPublish(p,0,0);p->starved=0;
      resetStall(&stall,presented);native_unhealthy=0;
    }
    if(!network_hold && !seek && !audio && !pause && !eof && complete && clock_seen &&
       !seek_preroll && !dv_audio_hold && !fast_pending &&
       (fed_audio-presented<0.5 || fed_video-presented<0.5)) {
      DtsBufferInfo b;dts_engine_buffer_info(p->engine,&b);
      if(b.capacity_bytes>0 && !refillTail(&b) && b.contiguous_bytes<refillGoal(&b)) {
        network_hold=1;network_since=wall;network_target=presented;p->starved=0;
        printf("[dts-rebuffer] event=start reason=underflow target=%.3f contiguousKiB=%lld readyKiB=%lld\n",
          presented,(long long)(b.contiguous_bytes/1024),(long long)(b.ready_bytes/1024));
        fflush(stdout);refillPublish(p,1,0);
      }
    }
    if(network_hold) {
      DtsBufferInfo b;dts_engine_buffer_info(p->engine,&b);
      if(pause)network_since=wall;
      int enough=fed_audio>=network_target+DTS_REFILL_SECONDS &&
                 fed_video>=network_target+DTS_REFILL_SECONDS;
      int bytes_ready=b.contiguous_bytes>=refillGoal(&b) || refillTail(&b);
      int full_ready=refillFullReady(refill_full_since,wall,have_frame,seek_preroll,complete,
                                    fed_audio,fed_video,network_target);
      if(!pause && (enough || full_ready) && bytes_ready) {
        printf("[dts-rebuffer] event=end ms=%.0f aheadA=%.2f aheadV=%.2f contiguousKiB=%lld reason=%s\n",
          (wall-network_since)*1000,fed_audio-network_target,fed_video-network_target,
          (long long)(b.contiguous_bytes/1024),enough?"ready":"native-full");fflush(stdout);
        network_hold=0;refill_full_since=-1;p->starved=0;pause_dirty=1;refillPublish(p,0,0);
        resetStall(&stall,presented);native_unhealthy=0;
        rate=(DtsRate){.start=wall};
      } else if(!pause && wall-network_since>=DTS_REFILL_TIMEOUT_SECONDS) {
        printf("[dts-rebuffer] event=timeout ms=%.0f contiguousKiB=%lld readyKiB=%lld\n",
          (wall-network_since)*1000,(long long)(b.contiguous_bytes/1024),
          (long long)(b.ready_bytes/1024));fflush(stdout);
        fail(p,p->dv?DTS_PLAYBACK_STARVED:"DTS buffer recovery timed out");break;
      } else refillPublish(p,1,wall-network_since);
    }
#endif
    if (clock_seen && presented > stall.position + 0.001) {
      stall.advanced = 1; stall.position = presented; stall.changed_at = wall;
      native_unhealthy = 0;
      recovery_waiting = 0;
    }
    if (!dv_audio_hold && !network_hold && (recovery_waiting || (complete && clock_seen)) && stalled(&stall, wall, DTS_STALL_SEEK_SECONDS)) native_unhealthy = 1;
    if (pause || dv_audio_hold || network_hold || (!recovery_waiting && (!complete || !clock_seen || seek_preroll)) || eof) stall.full_since = -1;
    reportRate(p, &rate, presented, fed_audio, fed_video, clock_seen, pause || network_hold, complete, seek_preroll);
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
    if (p->starved >= DTS_DV_STARVED_WINDOWS && !network_hold) {
      fail(p, DTS_PLAYBACK_STARVED);
      break;
    }
    if(dv_audio_hold) {
      DtsBufferInfo b;dts_engine_buffer_info(p->engine,&b);
      int64_t goal=b.capacity_bytes/4;
      if(goal>16*1024*1024)goal=16*1024*1024;
      int enough=fed_audio>=hold_target+DTS_DV_AUDIO_HOLD_SECONDS &&
                 fed_video>=hold_target+DTS_DV_AUDIO_HOLD_SECONDS;
      int tail=b.total>0 && b.position>=b.total-b.contiguous_bytes;
      int bytes_ready=b.capacity_bytes<=0 || b.contiguous_bytes>=goal || tail;
      if(pause)hold_since=wall;
      int full_ready=0;
#ifdef NV_DTS_WEBOS_RECOVERY
      full_ready=refillFullReady(refill_full_since,wall,have_frame,seek_preroll,complete,
                                fed_audio,fed_video,hold_target);
#endif
      if(!pause && (((enough || full_ready) && bytes_ready) || wall-hold_since>=DTS_DV_AUDIO_HOLD_MAX_SECONDS)) {
        printf("[dts] dv-audio-hold-release reason=%s ms=%.0f aheadA=%.2f aheadV=%.2f contiguousKiB=%lld\n",
          enough && bytes_ready?"ready":full_ready && bytes_ready?"native-full":"deadline",(wall-hold_since)*1000,
          fed_audio-hold_target,fed_video-hold_target,(long long)(b.contiguous_bytes/1024));
        fflush(stdout);dv_audio_hold=0;pause_dirty=1;
        fast_since=fast_started=wall;resetStall(&stall,hold_target);native_unhealthy=0;
      }
    }
    if(fast_pending && (pause || dv_audio_hold || network_hold || (seek_preroll && !have_frame)))
      fast_since=fast_started=wall;
    if(fast_pending && !dv_audio_hold && !network_hold && clock_seen && clock_advances>=2 && presented>fast_target+0.25) {
      fast_pending=0;
      pthread_mutex_lock(&p->lock);p->clock_guard=0;pthread_mutex_unlock(&p->lock);
      nativeEvent(p,"{\"dtsStage\":{\"name\":\"native-seek-progress-confirmed\"}}");
    }
    // A successful flush call is not proof that the firmware changed timeline.
    // Retry with fresh Load once after a bounded no-progress interval; disable
    // the experimental path for the rest of this playback session.
    int fast_failed=fast_pending && !pause && !dv_audio_hold && !network_hold &&
      (wall-fast_since>=(p->dv?DTS_DV_SEEK_SECONDS:DTS_STALL_SEEK_SECONDS) ||
       (p->dv && wall-fast_started>=DTS_DV_SEEK_TOTAL_SECONDS));
    if(fast_failed) {
      p->seamless_disabled=1;fast_pending=0;
      printf("[dts] native-seek-fallback reason=no-progress target=%.3f waitMs=%.0f totalMs=%.0f\n",
        fast_target,(wall-fast_since)*1000,(wall-fast_started)*1000);fflush(stdout);
      nativeEvent(p,"{\"dtsStage\":{\"name\":\"native-seek-fallback\",\"detail\":\"no presentation progress\"}}");
    }
#ifdef NV_MP4_BD_TEST
    /* A second command can arrive before firmware reports the first accepted
     * seek's clock. Feed that seek to completion and retain only the latest
     * target, instead of treating the missing intermediate clock as failure.
     * Existing no-progress deadlines and reload fallback remain authoritative. */
    if(p->mp4_recovery) {
      if(seek && fast_pending && !fast_failed && !pause && !audio &&
         p->seamless && !p->seamless_disabled && !native_unhealthy && complete && !eof) {
        queued_seek=1;queued_target=target;seek=0;
        printf("[mp4-seek] deferred target=%.3f pending=%.3f\n",queued_target,fast_target);fflush(stdout);
      }
      if(queued_seek && (!fast_pending || pause || audio)) {
        if(!seek)target=queued_target;
        if(fast_failed)fast_target=target;
        else if(!audio)seek=1;
        printf("[mp4-seek] latest target=%.3f reload=%d audio=%d\n",target,fast_failed,audio);fflush(stdout);
        queued_seek=0;
      }
    }
#endif
    /* The adapter now requests a native segment seek and gates controls on
     * SEEKDONE. Test both DV and DTS while retaining one bounded cache/player. */
    int audio_prepared=0;
    if((seek || audio) && p->seamless && !p->seamless_disabled && !native_unhealthy &&
       complete && (clock_seen || pause) && !eof) {
      int selected=audio?new_stream:stream;
      int moved=reposition(p,selected,audio?0:core,target,&frame);
      if(moved==REPOSITION_OK) {
        stream=selected;if(audio) core=core_tried=0;
        position=preroll=presented=target;fed_audio=fed_video=target;
        have_frame=!!p->dv;eof=0;seek_preroll=1;seek_audio=seek_video=0;
        pause_dirty=volume_dirty=1;clock_seen=0;
        timestamp_retries=0;timestamp_anchor=target;
        resetStall(&stall,target);recovery_waiting=1;
        fast_pending=1;fast_since=fast_started=clockSeconds(CLOCK_MONOTONIC);fast_target=target;
        dv_audio_hold=audio && p->dv;
        hold_since=fast_since;hold_target=target;
        if(dv_audio_hold)nativeEvent(p,"{\"dtsStage\":{\"name\":\"dv-audio-hold-start\"}}");
#ifdef NV_DTS_WEBOS_RECOVERY
        if(!audio) {
          DtsBufferInfo b;dts_engine_buffer_info(p->engine,&b);
          if(b.capacity_bytes>0 && !refillTail(&b) && b.contiguous_bytes<refillGoal(&b)) {
            network_hold=1;network_since=fast_since;network_target=target;
            p->starved=0;refillPublish(p,1,0);
            printf("[dts-rebuffer] event=start reason=seek target=%.3f contiguousKiB=%lld readyKiB=%lld\n",
              target,(long long)(b.contiguous_bytes/1024),(long long)(b.ready_bytes/1024));fflush(stdout);
          }
        }
#endif
        cpu_window=clockSeconds(CLOCK_MONOTONIC);cpu_used=0;
        seek=audio=0;
      } else if(moved==REPOSITION_FORMAT_CHANGED) {
        audio_prepared=1;
        nativeEvent(p,"{\"dtsStage\":{\"name\":\"audio-format-reload\"}}");
      } else {
        p->seamless_disabled=1;
        nativeEvent(p,"{\"dtsStage\":{\"name\":\"native-seek-fallback\",\"detail\":\"unavailable, refused or preparation failed\"}}");
      }
    }
    if(stopped(p))break;
    int seek_reload = !audio && seek;
    int stall_seek = seek_reload && native_unhealthy;
    int stall_reload = !audio && !seek && !pause && !eof &&
                       !dv_audio_hold && !network_hold && (fast_failed || (!fast_pending && (recovery_waiting || (complete && clock_seen)) &&
                       stalled(&stall, wall, DTS_STALL_SECONDS)));
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
      if (seek_reload) { timestamp_retries = 0; timestamp_anchor = target; }
      if (stall_reload) { target = fast_failed?fast_target:presented; stall_retries++; }
      double stale_seconds = wall - stall.changed_at;
      int blocked_kind = have_frame ? frame.kind : 0;
      double blocked_pts = have_frame ? frame.pts_ns / 1e9 : 0;
      /* Missing/refused native Seek or a renderer without presentation
       * progress uses the validated fresh Load with full HDR metadata. */
      /* Healthy explicit seeks can keep the validated container/track
       * metadata and reset demux/decoder/encoder through engine_seek. The LG
       * native pipeline still receives a fresh Load and codec preroll. Error
       * recovery keeps a full source reopen. A failed native flush, however,
       * invalidates only the renderer: retain the validated demux and cache.
       * prepareMode still reopens if engine_seek itself fails. */
      if (!prepareMode(p, stream, core, target,fast_failed || (seek_reload && !stall_seek))) break;
      fast_pending=0;dv_audio_hold=0;
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
      seek_preroll = 1; seek_audio = seek_video = 0;
      stream = new_stream; core = core_tried = 0;
      timestamp_retries = 0; timestamp_anchor = target;
      DtsFrame primed={0};
      if (!prepareModePrimed(p, stream, core, target,1,
                            p->dv && p->seamless ? &primed : NULL,audio_prepared)) break;
      fast_pending=0;
      dv_audio_hold=p->dv && p->seamless;hold_since=clockSeconds(CLOCK_MONOTONIC);hold_target=target;
      if(dv_audio_hold)nativeEvent(p,"{\"dtsStage\":{\"name\":\"dv-audio-hold-start\"}}");
      position = preroll = presented = target; complete = clock_seen = 0;
      fed_audio = fed_video = target;
      frame=primed;have_frame=primed.data && primed.size;
      eof = 0; pause_dirty = 1; volume_dirty = 1;
      cpu_window = clockSeconds(CLOCK_MONOTONIC); cpu_used = 0;
      resetStall(&stall, target);
      native_unhealthy = 0;
      recovery_waiting = stall.advanced = !!seek;
    }
    if (subtitle && !selectSubtitle(p, subtitle_stream)) {
      fail(p, "DTS subtitle decoder unavailable"); break;
    }
    int hold_pause=pause || dv_audio_hold || network_hold;
    if ((complete && pause_dirty) || paused != hold_pause) {
      if (!(hold_pause ? dts_pipeline_pause(p->pipeline) : dts_pipeline_play(p->pipeline))) {
        fail(p, "DTS pause/play failed"); break;
      }
      paused = hold_pause;
    }
    /* Native volume is optional. Reapply after each Load completion. */
    if (volume_dirty) (void)dts_pipeline_volume(p->pipeline, volume);
    reportBuffer(p, fed_audio, fed_video);
    /* Native load completion needs preroll even when opened paused. */
    if (paused && !seek_preroll && !dv_audio_hold && !network_hold && (complete || position > preroll + 2.0)) {
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
      /* HTTP/demux preparation can block the sole worker. Count that time
       * separately from renderer no-progress, while retaining an absolute
       * post-resume deadline so a bad timeline still gets bounded recovery. */
      if(fast_pending && p->dv && !dv_audio_hold && read_started>=0 && now-read_started>=0.25)
        fast_since+=now-read_started;
      if (begun >= 0 && ended >= begun) cpu_used += ended - begun;
      if (begun >= 0 && ended >= begun) rate.decode_cpu += ended - begun;
      if (read_started >= 0 && now >= read_started) rate.read_wall += now - read_started;
      if (stopped(p)) break;
      const char *error = r < 0 ? dts_engine_error(p->engine) : "";
      if (r < 0 && !strcmp(error, DTS_ENGINE_TIMESTAMP_DISCONTINUITY)) {
#ifndef NV_DTS_WEBOS_RECOVERY
        fail(p, error); break;
#endif
        /* Use the latest presentation clock, never a packet fed ahead.
         * Pending user seek/track requests are handled by the next iteration. */
        int pending_control;
        pthread_mutex_lock(&p->lock);
        target = p->presented;
        pending_control = p->seek_dirty || p->audio_dirty;
        pthread_mutex_unlock(&p->lock);
        if (pending_control) continue;
        if (timestamp_retries && target >= timestamp_anchor + DTS_TIMESTAMP_PROGRESS_SECONDS)
          timestamp_retries = 0;
        if (timestamp_retries >= DTS_TIMESTAMP_RETRIES) {
          char event[192];
          snprintf(event, sizeof event,
            "{\"dtsStage\":{\"name\":\"timestamp-recovery-exhausted\",\"detail\":\"at=%.3f attempts=%u\"}}",
            target, timestamp_retries);
          nativeEvent(p, event);
          fail(p, "DTS timestamp recovery exhausted after two reload attempts");
          break;
        }
        timestamp_retries++; timestamp_anchor = target;
#ifdef NV_DTS_WEBOS_RECOVERY
        if(network_hold) {network_hold=0;refillPublish(p,0,0);}
#endif
        printf("[dts] timestamp-recovery-request at=%.3f attempt=%u core=%d stream=%d dv=%d\n",
               target, timestamp_retries, core, stream, p->dv);
        fflush(stdout);
        if (!prepare(p, stream, core, target)) break;
        char event[192];
        snprintf(event, sizeof event,
          "{\"dtsStage\":{\"name\":\"timestamp-reload\",\"detail\":\"at=%.3f attempt=%u core=%d stream=%d dv=%d\"}}",
          target, timestamp_retries, core, stream, p->dv);
        nativeEvent(p, event);
        position = fed_audio = fed_video = target;
        have_frame = eof = seek_preroll = seek_audio = seek_video = 0;
        dv_audio_hold=fast_pending=0;
        resetStall(&stall, target);
        recovery_waiting = stall.advanced = 1;
        native_unhealthy = 0; p->starved = 0;
        cpu_window = clockSeconds(CLOCK_MONOTONIC); cpu_used = 0;
        rate = (DtsRate){.start = cpu_window};
        continue;
      }
      int decode_error = r < 0 && !strncmp(error, "DTS decode", 10);
      int encoder_error = r < 0 && (!strncmp(error, "Audio encode", 12) ||
                                   !strncmp(error, "Audio output", 12));
      int cpu_overrun = r > 0 && cpu_window >= 0 && now - cpu_window >= 5.0 &&
                        cpu_used > (now - cpu_window) * 0.80;
      if (now >= 0 && now - cpu_window >= 5.0) { cpu_window = now; cpu_used = 0; }
      if (decode_error || encoder_error || cpu_overrun) {
#ifdef NV_DTS_WEBOS_RECOVERY
        if(network_hold) {network_hold=0;refillPublish(p,0,0);}
#endif
        pthread_mutex_lock(&p->lock); presented = p->presented; pthread_mutex_unlock(&p->lock);
        if (recover(p, &core, &core_tried, stream, presented, decode_error || cpu_overrun)) {
          seek_preroll = 0;dv_audio_hold=0;fast_pending=0;
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
#ifdef NV_DTS_WEBOS_RECOVERY
        if(network_hold) {network_hold=0;pause_dirty=1;refillPublish(p,0,0);}
#endif
        if(dv_audio_hold) { dv_audio_hold=0;pause_dirty=1; }
        seek_preroll = 0;
        if (!dts_pipeline_eos(p->pipeline)) { fail(p, "DTS end of stream failed"); break; }
        pthread_mutex_lock(&p->lock);
        p->status.eof = 1;
        pthread_mutex_unlock(&p->lock);
        eof = 1;
        continue;
      }
      have_frame = 1;
    }
    if ((frame.kind == DTS_VIDEO && frame.size > 16 * 1024 * 1024) ||
        (frame.kind == DTS_AUDIO && frame.size > 2 * 1024 * 1024)) {
      fail(p, "DTS packet exceeds playback memory budget"); break;
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
#ifdef NV_HTTP_BUFFER_DIAG
      if(frame.kind==DTS_VIDEO && p->picture_samples<4 &&
         (frame.pts_ns!=p->picture_last_pts || frame.size!=p->picture_last_size)) {
        const DtsMediaInfo *mi=dts_engine_info(p->engine);
        NvVideoPacketDiag d=nv_video_packet_diag(frame.data,frame.size,mi?mi->video_codec:"");
        p->picture_samples++;p->picture_last_pts=frame.pts_ns;p->picture_last_size=frame.size;
        printf("[video-packet] epoch=%u sample=%u bytes=%zu ptsNs=%lld dtsNs=%lld durNs=%lld vcl=%d irap=%d vps=%d sps=%d pps=%d rpu=%d el=%d scanned=%u clipped=%d headerErr=%d feed=%d\n",
          p->picture_epoch,p->picture_samples,frame.size,(long long)frame.pts_ns,(long long)frame.dts_ns,
          (long long)frame.duration_ns,d.vcl,d.irap,d.vps,d.sps,d.pps,d.rpu,d.el,d.scanned,d.clipped,d.invalid,r);fflush(stdout);
      }
#endif
      if (r < 0) { fail(p, dts_pipeline_error(p->pipeline)); break; }
      if (r) {
#ifdef NV_DTS_WEBOS_RECOVERY
        refill_full_since=-1;
#endif
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
#ifdef NV_DTS_WEBOS_RECOVERY
        if((network_hold || dv_audio_hold) && !seek_preroll && complete && refill_full_since<0)
          refill_full_since=feed_ended;
#endif
        rate.full++;
        if (!pause && (recovery_waiting || (complete && clock_seen && !seek_preroll)) && stall.full_since < 0)
          stall.full_since = feed_ended;
        rest(p);
      }
    }
  }
done:
  /* Ignore closing callbacks after terminal failure, too. */
  pthread_mutex_lock(&p->lock); p->suppress_events = 1; pthread_mutex_unlock(&p->lock);
  pthread_mutex_lock(&p->lock);
  p->status.buffer = (DtsBufferInfo){0};p->status.buffered_until = 0;p->status.buffering_ms=0;
  pthread_mutex_unlock(&p->lock);
  dts_subtitles_destroy(p->subtitle_decoder); p->subtitle_decoder = NULL;
  dts_pipeline_destroy(p->pipeline);
  p->pipeline = NULL;
  return NULL;
}

static int dvNext, p7Next, seamlessNext, dvAudioNext, mp4Next;
#ifdef NV_DV7_MAPPING_TEST
static int p7MappingNext;
void dts_playback_next_p7_mapping(int enabled) { p7MappingNext=!!enabled; }
#endif
static double resumePctNext;
void dts_playback_next_resume_pct(double pct) {
  resumePctNext=isfinite(pct) && pct>0 && pct<100 ? pct : 0;
}
void dts_playback_next_seamless(int enabled) { seamlessNext=!!enabled; }
void dts_playback_next_dv_audio(int mode) { dvAudioNext=mode==1 || mode==2 ? mode : 0; }
void dts_playback_next_p7(int enabled) { p7Next = !!enabled; }
void dts_playback_next_mp4_test(int enabled) { mp4Next=!!enabled; }
void dts_playback_next_dv(int enabled) { dvNext = !!enabled; }
DtsPlayback *dts_playback_start(const char *url, const char *headers,
                               int audio_stream, double position,
                               int paused, const char *window, int major,
                               const DtsTrack *selected, int ordinal, int count) {
  DtsPlayback *p;
  double resume_pct=resumePctNext;resumePctNext=0;
  int dv_audio=dvAudioNext;dvAudioNext=0;
  int mp4_test=mp4Next;mp4Next=0;
#ifdef NV_DV7_MAPPING_TEST
  int p7_mapping=p7MappingNext;p7MappingNext=0;
#endif
  if (!url || !*url || strlen(url) >= NV_MEDIA_URL_MAX || (headers && strlen(headers) >= 2048) ||
      (window && strlen(window) >= 64) || !isfinite(position) || position >= 9e9 || !dts_engine_available()) return NULL;
  p = calloc(1, sizeof *p);
  if (!p) return NULL;
#ifdef NV_MP4_BD_TEST
  p->mp4_recovery=mp4_test;
#endif
  pthread_mutex_init(&p->lock, NULL);
  pthread_cond_init(&p->wake, NULL);
  p->engine = dts_engine_create();
  if (!p->engine) { dts_playback_close(p); return NULL; }
  p->seamless=seamlessNext;seamlessNext=0;
  p->dv = dvNext; dvNext = 0;
  dts_engine_dv(p->engine, p->dv);
  dts_engine_dv_audio(p->engine,p->dv && p->seamless ? dv_audio : 0);
  {
    int compiled=0;
#ifdef NV_DV_FIXED_AUDIO_TEST
    compiled=1;
#endif
    printf("[dts] audio-policy requested=%d fixed=%d dv=%d seamless=%d compiled=%d\n",
      dv_audio,compiled && p->dv && p->seamless ? dv_audio : 0,p->dv,p->seamless,compiled);
    fflush(stdout);
  }
  p->allow_p7 = p7Next; p7Next = 0;
  dts_engine_allow_p7(p->engine, p->allow_p7);
#ifdef NV_DV7_MAPPING_TEST
  dts_engine_p7_mapping(p->engine,p->dv && p->allow_p7 && p7_mapping);
#endif
  dts_engine_mp4_test(p->engine,mp4_test);
  snprintf(p->url, sizeof p->url, "%s", url);
  snprintf(p->headers, sizeof p->headers, "%s", headers ? headers : "");
  snprintf(p->window, sizeof p->window, "%s", window ? window : "");
  p->audio_stream = audio_stream;
  p->volume = 100; p->volume_dirty = 1;
  p->subtitle_stream = -1;
  p->major = major; p->target = position < 0 ? 0 : position;
  p->resume_pct=p->target>0?0:resume_pct;
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
  p->status.buffer = (DtsBufferInfo){0};p->status.buffered_until = 0;
  pthread_cond_signal(&p->wake); pthread_mutex_unlock(&p->lock);
}
void dts_playback_audio(DtsPlayback *p, int stream) {
  if (!p || stream < 0) return;
  pthread_mutex_lock(&p->lock);
  if (!p->seek_dirty) p->target = p->presented;
  p->audio_stream = stream; p->audio_dirty = 1;
  p->status.buffer = (DtsBufferInfo){0};p->status.buffered_until = 0;
  pthread_cond_signal(&p->wake); pthread_mutex_unlock(&p->lock);
}
void dts_playback_audio_at(DtsPlayback *p, int stream, double seconds) {
  if (!p || stream < 0 || !isfinite(seconds) || seconds >= 9e9) return;
  pthread_mutex_lock(&p->lock);
  if (!p->seek_dirty) p->target = seconds < 0 ? 0 : seconds;
  p->audio_stream = stream; p->audio_dirty = 1;
  p->status.buffer = (DtsBufferInfo){0};p->status.buffered_until = 0;
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
