#define _POSIX_C_SOURCE 200809L
#include "dts/dts_playback.h"
#include "dts/dts_engine.h"
#include "dts/dts_pipeline.h"
#include <assert.h>
#include <stdatomic.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
/* Real controller, fake engine/native boundary. No HTTP, TV, UV/uMS or FFmpeg. */
static pthread_mutex_t fixture = PTHREAD_MUTEX_INITIALIZER;
static int scenario, opens, loads, creates, destroys, nexts, accepted, feeds, plays, pauses, seeks, flushes, eoses;
static int last_stream, backpressure_left, pending_serial, bad_retry;
static double last_seek, last_open_target, last_audio_target, last_load_target;
static int64_t fixture_duration;
static int core_opens, volumes, last_volume, sub_decoded, sub_created, sub_destroyed, seek_rendered;
static int64_t last_sub_pts;
static atomic_int allow_decode_failure, transport_stalled, release_transport, stall_transport;
static atomic_int jam_native, native_full, seek_failure;
static atomic_int timestamp_fault, timestamp_fail_open, timestamp_block_open, timestamp_open_entered;
static int last_dv, audio_reconfigs, audio_fail_once, missing_dv;
static int truehd_fixture;
static atomic_int prime_block, prime_entered, prime_release;
static int prime_serial;
static int seamless_fixture, flush_refuse, audio_format_change, stale_clock;
static int handoff_fixture, pause_refuse, prime_failure;
static int hold_fixture,hold_deadline;
static atomic_int hold_slow_reads,hold_no_clock;
static atomic_llong hold_wall_offset;
static atomic_int handoff_cancelled;
static int last_p7,mp4_fixture,mp4_seek_case;
static atomic_int mp4_no_clock;
static atomic_llong buffer_ready = 21*1048576LL;
static atomic_llong buffer_contiguous = 20*1048576LL;
void dts_engine_buffer_info(DtsEngine *e,DtsBufferInfo *out) {
 (void)e;
 *out=(DtsBufferInfo){.ready_bytes=atomic_load(&buffer_ready),.capacity_bytes=68*1048576LL,
                     .position=1048576,.total=300*1048576LL,.contiguous_bytes=atomic_load(&buffer_contiguous)};
}
static atomic_llong stall_wall_ns;
static int64_t frozen_clock, original_stall_clock;
static int64_t stall_audio_pts, stall_video_pts;
static int64_t fed_audio_pts, fed_video_pts;
static long fake_wall=100, fake_thread_cpu;
static void (*native_event)(void *,const char *);
static void *native_user;
enum { NORMAL, OPEN_FAIL, BACKPRESSURE, BLOCK_READ, EOF_MODE, MAPPING, MAPPING_BAD, SUBTITLES, CORE_RECOVERY, NO_CORE_FAIL, CPU_BUDGET, DELAYED_ID, PACING, SUSTAINED, SEEK_PREROLL, SEEK_SEGMENT, TIMESTAMP_ONCE, TIMESTAMP_REPEAT, TIMESTAMP_PAUSED, TIMESTAMP_CANCEL, NATIVE_STALL, NATIVE_REPEAT, NATIVE_NO_CLOCK, NATIVE_TRANSIENT, NATIVE_RECOVERY_NO_CLOCK, NATIVE_RECOVERY_UNLOADED, DV_MAPPING };
static int timestampScenario(void) { return scenario>=TIMESTAMP_ONCE && scenario<=TIMESTAMP_CANCEL; }
struct DtsEngine { DtsMediaInfo info; atomic_int cancelled; unsigned char packet[98304]; int serial, core, dv_audio; double base; };
struct DtsPipeline { void (*event)(void *,const char *); void *user; int loaded, completed, running, flushed, audio_ready, video_ready, generation, packets, broken_segment; double target; };
static void nap(void) { struct timespec t={0,1000000};nanosleep(&t,NULL); }
static void emit(const char *json) {
 void (*cb)(void *,const char *);void *u;
 pthread_mutex_lock(&fixture);cb=native_event;u=native_user;pthread_mutex_unlock(&fixture);
 assert(cb);cb(u,json);
}
int dts_engine_available(void) { return 1; }
#ifdef NV_DV7_MAPPING_TEST
static int last_p7_mapping;
void dts_engine_p7_mapping(DtsEngine *e,int enabled) {assert(e);last_p7_mapping=enabled;}
#endif
void dts_engine_allow_p7(DtsEngine *e,int on) { (void)e; (void)on;
 last_p7=on;
}
void dts_engine_dv(DtsEngine *e,int on) { (void)e; last_dv=on; }
void dts_engine_mp4_test(DtsEngine *e,int on) { (void)e;assert(on==mp4_fixture); }
void dts_engine_dv_audio(DtsEngine *e,int mode) { e->dv_audio=mode; }
static void fixedInfo(DtsEngine *e) {
 if(e->dv_audio) {
  strcpy(e->info.audio_codec,e->dv_audio==2?"ac3":"aac");
  e->info.channels=e->dv_audio==2?6:2;e->info.sample_rate=48000;
 }
}
DtsEngine *dts_engine_create(void) { return calloc(1,sizeof(DtsEngine)); }
int dts_engine_open(DtsEngine *e,const char *url,const char *headers,int stream,int core,double target) {
 (void)headers;assert(!strcmp(url,"fixture://movie"));
 pthread_mutex_lock(&fixture);opens++;last_stream=stream;last_open_target=target;if(core)core_opens++;pthread_mutex_unlock(&fixture);
 if(scenario==OPEN_FAIL || atomic_load(&timestamp_fail_open)) return -1;
 if(atomic_load(&timestamp_block_open)) {
  atomic_store(&timestamp_open_entered,1);
  while(!atomic_load(&e->cancelled)) nap();
  return -1;
 }
 memset(&e->info,0,sizeof e->info);strcpy(e->info.video_codec,"hevc");strcpy(e->info.audio_codec,"aac");
 e->info.width=1920;e->info.height=1080;e->info.audio_stream=stream<0?4:stream;e->info.channels=2;e->info.sample_rate=48000;
 e->info.duration_ns=fixture_duration;
 if(last_dv && !missing_dv) { e->info.dovi_profile=8;e->info.dovi_bl_present=e->info.dovi_rpu_present=1;strcpy(e->info.hdr,"DolbyVision"); }
 e->info.n_tracks=4;
 e->info.tracks[0]=(DtsTrack){.stream_index=0,.kind=DTS_VIDEO};
 e->info.tracks[1]=(DtsTrack){.stream_index=1,.kind=DTS_AUDIO,.channels=2};strcpy(e->info.tracks[1].codec,"aac");strcpy(e->info.tracks[1].language,"en");
 e->info.tracks[2]=(DtsTrack){.stream_index=4,.kind=DTS_AUDIO,.channels=6};strcpy(e->info.tracks[2].codec,"dts");strcpy(e->info.tracks[2].language,"pt");
 e->info.tracks[3]=(DtsTrack){.stream_index=9,.kind=DTS_AUDIO,.channels=2};strcpy(e->info.tracks[3].codec,"dts");strcpy(e->info.tracks[3].language,"ja");
 if(scenario==DV_MAPPING) {
  // Same-language/channel AC3 and DTS must not be conflated on the DV path.
  strcpy(e->info.tracks[2].codec,"ac3");strcpy(e->info.tracks[2].language,"ru");
  strcpy(e->info.tracks[3].language,"ru");e->info.tracks[3].channels=6;
  if(truehd_fixture)strcpy(e->info.tracks[3].codec,"truehd");
 }
 if(scenario==MAPPING_BAD) { e->info.tracks[4]=e->info.tracks[3];e->info.tracks[4].stream_index=10;e->info.n_tracks=5; }
 if(scenario==SUBTITLES || scenario==SEEK_SEGMENT || timestampScenario()) {
  e->info.tracks[4]=(DtsTrack){.stream_index=20,.kind=DTS_SUBTITLE};strcpy(e->info.tracks[4].codec,"srt");e->info.n_tracks=5;
 }
 fixedInfo(e);
 e->core=core;e->base=target;e->serial=0;return 0;
}
int dts_engine_audio(DtsEngine *e,int stream,int core,double target) {
 if(audio_fail_once) { audio_fail_once=0;return -1; }
 if(atomic_load(&seek_failure)) return -1;
 e->info.audio_stream=stream;e->core=core;e->base=target;e->serial=0;
 if(audio_format_change) strcpy(e->info.audio_codec,"ac3");
 if(handoff_fixture) {
  strcpy(e->info.audio_codec,stream==9?"ac3":"aac");
  e->info.channels=stream==9?6:2;
 }
 fixedInfo(e);
 pthread_mutex_lock(&fixture);audio_reconfigs++;last_stream=stream;last_audio_target=target;pthread_mutex_unlock(&fixture);
 return 0;
}
const DtsMediaInfo *dts_engine_info(DtsEngine *e) { return &e->info; }
const void *dts_engine_subtitle_parameters(DtsEngine *e,int stream) { (void)e;static int params;return stream==20?&params:NULL; }
int dts_engine_has_core(DtsEngine *e) { (void)e;return scenario==CORE_RECOVERY || scenario==CPU_BUDGET; }
struct DtsSubtitles { int unused; };
DtsSubtitles *dts_subtitles_create(const void *params) {
 assert(params);pthread_mutex_lock(&fixture);sub_created++;pthread_mutex_unlock(&fixture);return calloc(1,sizeof(DtsSubtitles));
}
int dts_subtitles_decode(DtsSubtitles *s,const DtsFrame *f,DtsSubtitle *out) {
 static uint8_t pixels[16]={3,4,5,255};assert(s);
 *out=(DtsSubtitle){.rgba=pixels,.w=2,.h=2,.canvas_w=1920,.canvas_h=1080,.start_ns=f->pts_ns,.end_ns=f->pts_ns+1000000000};
 strcpy(out->text,"fixture cue");pthread_mutex_lock(&fixture);sub_decoded++;last_sub_pts=f->pts_ns;pthread_mutex_unlock(&fixture);return 1;
}
void dts_subtitles_flush(DtsSubtitles *s) { (void)s; }
void dts_subtitles_destroy(DtsSubtitles *s) {
 if(!s) return;
 pthread_mutex_lock(&fixture);sub_destroyed++;pthread_mutex_unlock(&fixture);free(s);
}
int dts_engine_next(DtsEngine *e,DtsFrame *f) {
 if(atomic_load(&prime_block) && e->base>0 && (!handoff_fixture || e->info.audio_stream==9)) {
  atomic_store(&prime_entered,1);
  while(!atomic_load(&prime_release) && !atomic_load(&e->cancelled))nap();
  if(atomic_load(&e->cancelled))return -1;
 }
 if(handoff_fixture && e->base>0 && prime_failure) { prime_failure=0;return -1; }
 if(scenario==CPU_BUDGET) { fake_wall++;fake_thread_cpu++; }
 pthread_mutex_lock(&fixture);nexts++;pthread_mutex_unlock(&fixture);
 if(timestampScenario()) {
  if(scenario==TIMESTAMP_PAUSED && !e->serial && opens==1)
   while(!atomic_load(&timestamp_fault) && !atomic_load(&e->cancelled)) nap();
  if(atomic_exchange(&timestamp_fault,0)) return -1;
 }
 if(scenario==BLOCK_READ) { while(!atomic_load(&e->cancelled)) nap();return -1; }
 if(scenario==CORE_RECOVERY && !e->core)
  while(!atomic_load(&allow_decode_failure) && !atomic_load(&e->cancelled)) nap();
 if((scenario==CORE_RECOVERY && !e->core) ||
    scenario==NO_CORE_FAIL) return -1;
 if(scenario==EOF_MODE && e->serial==3) return 0;
 if(atomic_load(&e->cancelled)) return -1;
 if(scenario==SUSTAINED && atomic_load(&stall_transport)) {
  atomic_store(&transport_stalled,1);
  while(!atomic_load(&release_transport) && !atomic_load(&e->cancelled)) nap();
 }
 nap();e->serial++;e->packet[0]=(unsigned char)e->serial;
 if(hold_slow_reads && creates==1 && e->base>0 && e->serial<=hold_slow_reads)
  atomic_fetch_add(&hold_wall_offset,INT64_C(2000000000));
 *f=(DtsFrame){.kind=DTS_AUDIO,.stream_index=e->info.audio_stream,.data=e->packet,.size=4,.pts_ns=(int64_t)(e->base*1000000000.0)+(int64_t)e->serial*100000000};
 if(scenario==NATIVE_STALL && e->base==0) f->pts_ns=INT64_C(303381000000)+(int64_t)e->serial*5000000;
 if(scenario==SUSTAINED) {
  f->kind=e->serial%2?DTS_AUDIO:DTS_VIDEO;
  f->pts_ns=((e->serial+1)/2)*INT64_C(100000000);
 }
 if(scenario==SEEK_PREROLL || scenario>=NATIVE_STALL) f->kind=e->serial%2?DTS_AUDIO:DTS_VIDEO;
 if(scenario==SEEK_SEGMENT) {
  f->kind=e->serial%2?DTS_VIDEO:DTS_AUDIO;
  if(f->kind==DTS_VIDEO) { f->size=95000;f->stream_index=0; }
  if(e->base==546.05 && e->serial==1) f->pts_ns=INT64_C(545503000000);
 }
 if(scenario==PACING && e->serial>70) {
  /* A clustered container puts 7 seconds of audio before the first video.
   * A per-stream cap on the sole pending packet would never reach that video. */
  if(e->serial<=131) { f->kind=DTS_VIDEO;f->pts_ns=(e->serial-70)*INT64_C(100000000); }
  else {
   f->kind=e->serial%2?DTS_AUDIO:DTS_VIDEO;
   f->pts_ns=(f->kind==DTS_AUDIO?INT64_C(7000000000):INT64_C(6100000000))+(e->serial-131)*INT64_C(100000000);
  }
 }
 if(scenario==SUBTITLES && !(e->serial%2)) { f->kind=DTS_SUBTITLE;f->stream_index=20; }
 return 1;
}
int dts_engine_seek(DtsEngine *e,double target) {
 pthread_mutex_lock(&fixture);seeks++;last_seek=target;pthread_mutex_unlock(&fixture);
 if(atomic_load(&seek_failure)) return -1;
 e->base=target;e->serial=0;return 0;
}
void dts_engine_cancel(DtsEngine *e) { atomic_store(&handoff_cancelled,1);atomic_store(&e->cancelled,1); }
void dts_engine_destroy(DtsEngine *e) { free(e); }
const char *dts_engine_error(DtsEngine *e) {
 (void)e;if(timestampScenario()) return (opens>1 && atomic_load(&timestamp_fail_open)) ? "fixture reopen failure" : DTS_ENGINE_TIMESTAMP_DISCONTINUITY;
return scenario==CORE_RECOVERY || scenario==NO_CORE_FAIL ? "DTS decode fixture failure" : "fixture engine failure";
}
int dts_pipeline_available(int major) { (void)major;return 1; }
DtsPipeline *dts_pipeline_create(const char *a,const char *w,int m,void (*cb)(void *,const char *),void *u) {
 (void)a;(void)w;(void)m;DtsPipeline *p=calloc(1,sizeof *p);p->event=cb;p->user=u;
 pthread_mutex_lock(&fixture);creates++;p->generation=creates;p->flushed=creates>1;native_event=cb;native_user=u;pthread_mutex_unlock(&fixture);return p;
}
int dts_pipeline_load(DtsPipeline *p,const DtsMediaInfo *m,double t) {
 (void)m; if(p->loaded) return 0;p->loaded=1;p->target=t;
 pthread_mutex_lock(&fixture);loads++;last_load_target=t;pthread_mutex_unlock(&fixture);return 1;
}
int dts_pipeline_feed(DtsPipeline *p,const DtsFrame *f) {
 assert(p->loaded);int refused=0;
 if(scenario==SEEK_SEGMENT && p->broken_segment && f->kind==DTS_VIDEO) return 0;
 if(scenario>=NATIVE_STALL &&
    ((scenario==NATIVE_REPEAT && p->packets>=4) ||
     (scenario==NATIVE_RECOVERY_NO_CLOCK && p->generation>1 && p->packets>=1) ||
     (scenario==NATIVE_RECOVERY_UNLOADED && p->generation>1) ||
     (atomic_load(&jam_native) && p->generation==1))) {
  int full=atomic_fetch_add(&native_full,1)+1;
  if(scenario!=NATIVE_TRANSIENT || full<=30) {
   pthread_mutex_lock(&fixture);
   if(!original_stall_clock) { original_stall_clock=frozen_clock;stall_audio_pts=fed_audio_pts;stall_video_pts=fed_video_pts; }
   pthread_mutex_unlock(&fixture);
   atomic_fetch_add(&stall_wall_ns,INT64_C(100000000));
   pthread_mutex_lock(&fixture);feeds++;pthread_mutex_unlock(&fixture);return 0;
  }
 }
 pthread_mutex_lock(&fixture);feeds++;
 if(backpressure_left) {
  if(pending_serial && pending_serial!=f->data[0]) bad_retry=1;
  pending_serial=f->data[0];backpressure_left--;refused=1;
 } else { accepted++;pending_serial=0;
  if(f->kind==DTS_AUDIO) fed_audio_pts=f->pts_ns;else fed_video_pts=f->pts_ns;
 }
 pthread_mutex_unlock(&fixture);
 if(refused) return 0;
 if(scenario==SEEK_PREROLL && p->flushed) {
  if(f->kind==DTS_AUDIO) p->audio_ready=1;else if(f->kind==DTS_VIDEO) p->video_ready=1;
  if(p->running && p->audio_ready && p->video_ready) {
   pthread_mutex_lock(&fixture);seek_rendered++;pthread_mutex_unlock(&fixture);
  }
 }
 if(!p->completed) { p->completed=1;p->event(p->user,"{\"loadCompleted\":{}}"); }
 if((scenario==SEEK_SEGMENT || seamless_fixture) &&
    !(mp4_seek_case && atomic_load(&mp4_no_clock) && p->flushed && p->generation==1) &&
    (!hold_fixture || (p->running && !(hold_no_clock && p->flushed && p->generation==1) &&
      !(hold_slow_reads && p->flushed && p->generation==1 && p->packets<hold_slow_reads)))) {
  int64_t native_pts=stale_clock && p->flushed && p->generation==1 ? (int64_t)((p->target+0.5)*1e9) : f->pts_ns;
  char clock[96];snprintf(clock,sizeof clock,"{\"currentTime\":{\"currentTime\":%lld}}",(long long)(native_pts/1000000));
  pthread_mutex_lock(&fixture);frozen_clock=f->pts_ns;pthread_mutex_unlock(&fixture);
  p->event(p->user,clock);
 }
 if(hold_fixture)p->packets++;
 if(scenario>=NATIVE_STALL) {
  p->packets++;
  if(scenario!=NATIVE_NO_CLOCK && !(scenario==NATIVE_RECOVERY_NO_CLOCK && p->generation>1)) {
   int64_t presented=f->pts_ns;
   if(scenario==NATIVE_STALL && p->generation==1) presented=p->packets==1?INT64_C(298000000000):INT64_C(298381000000);
   char clock[96];snprintf(clock,sizeof clock,"{\"currentTime\":{\"currentTime\":%lld}}",(long long)(presented/1000000));
   pthread_mutex_lock(&fixture);frozen_clock=presented;pthread_mutex_unlock(&fixture);
   p->event(p->user,clock);
  }
 }
 return 1;
}
int dts_pipeline_play(DtsPipeline *p) {
 if(hold_fixture && p->flushed && !hold_slow_reads && !hold_deadline) {
  assert(fed_audio_pts>=(int64_t)((p->target+2)*1e9));
  assert(fed_video_pts>=(int64_t)((p->target+2)*1e9));
  assert(atomic_load(&buffer_contiguous)>=16*1048576LL);
 }
 p->running=1;pthread_mutex_lock(&fixture);plays++;
 if(scenario==SEEK_PREROLL && p->flushed && p->audio_ready && p->video_ready) seek_rendered++;
 pthread_mutex_unlock(&fixture);return 1;
}
int dts_pipeline_pause(DtsPipeline *p) {
 if(pause_refuse && p->generation==1)return 0;
 p->running=0;pthread_mutex_lock(&fixture);pauses++;pthread_mutex_unlock(&fixture);return 1;
}
int dts_pipeline_flush(DtsPipeline *p,double t) {
 if(last_dv) {
  // DV seeks must hold the old frame and fetch one borrowed packet first.
  assert(!p->running);assert(atomic_load(&prime_block)?atomic_load(&prime_release):1);
  pthread_mutex_lock(&fixture);prime_serial=nexts;pthread_mutex_unlock(&fixture);
 }
 if(flush_refuse) return 0;
 p->target=t;
 if(scenario==SEEK_SEGMENT) {
  /* Captured failure: flush emits one old-segment keyframe clock, then a
   * normal 95KB video packet fills the decoder forever. Fresh Load clears it. */
  p->broken_segment=1;p->event(p->user,"{\"currentTime\":{\"currentTime\":545503}}");
 }
 if(scenario==NATIVE_STALL && atomic_load(&jam_native) && p->generation==1) return 0;
 (void)t;p->running=0;p->flushed=1;p->audio_ready=p->video_ready=0;
 if(hold_fixture)p->packets=0;
 /* Firmware keeps the Load complete state but flushes its stopped decoder
  * queues. Presentation requires fresh A/V preroll and a new Play. */
 if(scenario==SEEK_PREROLL) p->event(p->user,"{\"currentTime\":{\"currentTime\":1697863}}");
 pthread_mutex_lock(&fixture);flushes++;pthread_mutex_unlock(&fixture);return 1;
}
int dts_pipeline_eos(DtsPipeline *p) { (void)p;pthread_mutex_lock(&fixture);eoses++;pthread_mutex_unlock(&fixture);return 1; }
void dts_pipeline_destroy(DtsPipeline *p) {
 if(!p)return;
 if(handoff_fixture && p->generation==1 && atomic_load(&prime_block)) {
  assert(!p->running);
  assert(atomic_load(&prime_entered));
  assert(atomic_load(&prime_release) || atomic_load(&handoff_cancelled));
 }
 if(scenario==SEEK_PREROLL || scenario==SEEK_SEGMENT || timestampScenario()) p->event(p->user,"{\"currentTime\":{\"currentTime\":1697863}}");
 pthread_mutex_lock(&fixture);destroys++;native_event=NULL;pthread_mutex_unlock(&fixture);free(p);
}
const char *dts_pipeline_media_id(DtsPipeline *p) { return scenario==DELAYED_ID && !p->completed ? "" : "fixture-media"; }
const char *dts_pipeline_error(DtsPipeline *p) { (void)p;return "fixture pipeline failure/repeated Load"; }
static int count(int *v) { int n;pthread_mutex_lock(&fixture);n=*v;pthread_mutex_unlock(&fixture);return n; }
static void wait_count(int *v,int expected) { int n;for(n=0;n<1500 && count(v)<expected;n++)nap();assert(count(v)>=expected); }
static void wait_settled(void) {
 int previous=count(&accepted),stable=0;
 for(int i=0;i<1500 && stable<30;i++) {
  nap();int current=count(&accepted);
  stable=current==previous?stable+1:0;previous=current;
 }
 assert(stable==30);
}
static DtsPlaybackStatus wait_status(DtsPlayback *p,int fail,int eof) {
 DtsPlaybackStatus s={0};int i;
 for(i=0;i<1500;i++) { dts_playback_status(p,&s);if((fail?s.failed:s.prepared) && (!eof || s.eof))return s;nap(); }
 fprintf(stderr,"status prepared=%d failed=%d eof=%d error=%s\n",s.prepared,s.failed,s.eof,s.error);assert(0);return s;
}
static DtsPlayback *start(int paused,int stream,const DtsTrack *selected,int ordinal,int total) {
 dts_playback_next_p7(!getenv("DV7_OFF"));
 DtsPlayback *p=dts_playback_start("fixture://movie","X-Token: fixture",stream,0,paused,"window",0,selected,ordinal,total);assert(p);return p;
}
int main(int argc,char **argv) {
 assert(argc==2);assert(!dts_playback_enabled());DtsPlayback *p;DtsPlaybackStatus s;
#ifdef NV_DV7_MAPPING_TEST
 if(!strcmp(argv[1],"p7-mapping")) {
  dts_playback_next_p7_mapping(1);
  assert(!dts_playback_start(NULL,"",-1,0,0,"window",0,NULL,0,0));
  dts_playback_next_dv(1);p=start(0,1,NULL,0,0);wait_status(p,0,0);
  assert(last_p7_mapping==0);dts_playback_close(p);
  dts_playback_next_p7_mapping(1);dts_playback_next_dv(1);dts_playback_next_seamless(1);
  p=start(0,1,NULL,0,0);wait_status(p,0,0);assert(last_p7_mapping==1);
  dts_playback_seek(p,2);wait_count(&seeks,1);
  dts_playback_audio_at(p,2,2);wait_count(&audio_reconfigs,1);
  assert(last_p7_mapping==1);dts_playback_close(p);
  // Preference is consumed per session; the next movie starts at default.
  dts_playback_next_dv(1);p=start(0,1,NULL,0,0);wait_status(p,0,0);
  assert(last_p7_mapping==0);dts_playback_close(p);
  setenv("DV7_OFF","1",1);dts_playback_next_p7_mapping(1);dts_playback_next_dv(1);
  p=start(0,1,NULL,0,0);wait_status(p,0,0);assert(last_p7_mapping==0);dts_playback_close(p);
  unsetenv("DV7_OFF");dts_playback_next_p7_mapping(1);dts_playback_next_dv(0);
  p=start(0,1,NULL,0,0);wait_status(p,0,0);assert(last_p7_mapping==0);dts_playback_close(p);
  puts("PASS P7 mapping session snapshot: OFF, ON, rejected start, seek/audio retention, no leak, DV/P7 gating");return 0;
 }
#endif
 if(!strncmp(argv[1],"mp4-seek-",9)) {
  int off=!strcmp(argv[1],"mp4-seek-off"),timeout=!strcmp(argv[1],"mp4-seek-timeout");
  int pause=!strcmp(argv[1],"mp4-seek-paused"),cancel=!strcmp(argv[1],"mp4-seek-cancel");
  int audio=!strcmp(argv[1],"mp4-seek-audio");
  mp4_fixture=!off;mp4_seek_case=seamless_fixture=1;scenario=SEEK_PREROLL;
  dts_playback_next_mp4_test(mp4_fixture);dts_playback_next_dv(1);dts_playback_next_seamless(1);
  p=start(0,4,NULL,0,0);wait_count(&plays,1);wait_count(&accepted,4);
  atomic_store(&mp4_no_clock,1);dts_playback_seek(p,2);wait_count(&flushes,1);wait_count(&plays,2);
  dts_playback_seek(p,8);
  if(off) {wait_count(&loads,2);assert(count(&opens)==1);}
  else {
   wait_settled();assert(count(&loads)==1 && count(&flushes)==1);
   dts_playback_seek(p,12);wait_settled();assert(count(&loads)==1 && count(&flushes)==1);
   if(cancel) {dts_playback_close(p);assert(count(&creates)==count(&destroys));return 0;}
   if(timeout) {
    atomic_fetch_add(&hold_wall_offset,INT64_C(13000000000));wait_count(&loads,2);
    assert(last_load_target==12 && count(&opens)==1);
   } else if(pause) {
    dts_playback_pause(p,1);wait_count(&flushes,2);assert(last_seek==12);
   } else if(audio) {
    dts_playback_audio_at(p,9,2);wait_count(&loads,2);
    assert(last_audio_target==12 && last_stream==9);
   } else {
    atomic_store(&mp4_no_clock,0);
    for(int i=0;i<4;i++) {
     char event[96];snprintf(event,sizeof event,"{\"currentTime\":{\"currentTime\":%d}}",2600+i*100);
     emit(event);nap();
    }
    wait_count(&flushes,2);assert(last_seek==12);
   }
   if(!timeout && !audio)assert(count(&loads)==1 && count(&opens)==1);
   if(audio)assert(count(&loads)==2 && count(&opens)==1);
  }
  dts_playback_status(p,&s);assert(!s.failed);dts_playback_close(p);
  assert(count(&creates)==count(&destroys));printf("PASS %s: latest target, bounded fallback and one source cache\n",argv[1]);return 0;
 }
 if(!strncmp(argv[1],"audio-hold-",11)) {
  handoff_fixture=seamless_fixture=hold_fixture=1;scenario=SEEK_PREROLL;
  int gap=strstr(argv[1],"gap")!=NULL,cancel=strstr(argv[1],"cancel")!=NULL;
  int paused=strstr(argv[1],"paused")!=NULL;
  int slow=strstr(argv[1],"readwait")!=NULL,total=strstr(argv[1],"total")!=NULL;
  hold_deadline=strstr(argv[1],"deadline")!=NULL;
  dts_playback_next_seamless(1);dts_playback_next_dv(1);dts_playback_next_dv_audio(1);
  p=start(0,4,NULL,0,0);wait_count(&plays,1);wait_count(&accepted,4);
  if(paused) { dts_playback_pause(p,1);wait_count(&pauses,1); }
  if(gap || cancel || hold_deadline)atomic_store(&buffer_contiguous,4*1048576LL);
  int before=count(&accepted);
  if(slow || total) {
   hold_slow_reads=total?100:4;hold_no_clock=total;
   dts_playback_seek(p,2);wait_count(&flushes,1);
   if(total) {
    wait_count(&loads,2);int old=count(&flushes);
    dts_playback_seek(p,4);wait_count(&loads,3);assert(count(&flushes)==old);
   } else {
    wait_count(&accepted,before+10);assert(count(&loads)==1);
    dts_playback_seek(p,4);wait_count(&flushes,2);
   }
  } else {
   dts_playback_audio_at(p,9,2);wait_count(&flushes,1);
   if(gap || cancel || hold_deadline || paused) {
    wait_count(&accepted,before+(paused?18:24));wait_settled();assert(count(&plays)==1);
    if(cancel) { dts_playback_close(p);assert(count(&creates)==count(&destroys));return 0; }
    if(hold_deadline)atomic_fetch_add(&hold_wall_offset,INT64_C(7000000000));
    else atomic_store(&buffer_contiguous,20*1048576LL);
    if(paused) { assert(count(&plays)==1);dts_playback_pause(p,0); }
   }
   wait_count(&plays,2);assert(count(&loads)==1 && count(&opens)==1);
  }
  dts_playback_close(p);assert(count(&creates)==count(&destroys));
  printf("PASS %s: frame hold, contiguous readiness, pause/cancel or bounded data-aware recovery\n",argv[1]);return 0;
 }
 if(!strncmp(argv[1],"fixed-",6)) {
  handoff_fixture=seamless_fixture=1;scenario=SEEK_PREROLL;
  int mode=strstr(argv[1],"ac3")?2:1;
  int paused=strstr(argv[1],"paused")!=NULL;
  int refused=strstr(argv[1],"refused")!=NULL;
  int invalid=strstr(argv[1],"invalid")!=NULL;
  int nondv=strstr(argv[1],"nondv")!=NULL;
  int rejected=strstr(argv[1],"rejected")!=NULL;
  dts_playback_next_seamless(1);dts_playback_next_dv(!nondv);
  dts_playback_next_dv_audio(invalid?77:mode);
  if(rejected) assert(!dts_playback_start("","",4,0,0,"window",0,NULL,0,0));
  p=start(paused,4,NULL,0,0);wait_count(&accepted,paused?1:4);
  if(paused)wait_count(&pauses,1);else wait_count(&plays,1);
  s=wait_status(p,0,0);
  if(invalid || nondv || rejected) {
   assert(!strcmp(s.info.audio_codec,"aac") && s.info.channels==2);
   // No fixed mode may leak from a rejected start or into SDR playback.
   dts_playback_audio_at(p,9,2);wait_count(&loads,2);
   s=wait_status(p,0,0);assert(!strcmp(s.info.audio_codec,"ac3") && s.info.channels==6);
  } else {
   assert(s.info.channels==(mode==2?6:2));
   flush_refuse=refused;
   dts_playback_audio_at(p,9,2);
   if(refused) wait_count(&loads,2);else wait_count(&flushes,1);
   if(!paused)wait_count(&plays,2);
   s=wait_status(p,0,0);
   assert(s.info.audio_stream==9 && s.info.dovi_profile==8);
   assert(!strcmp(s.info.audio_codec,mode==2?"ac3":"aac") && s.info.channels==(mode==2?6:2));
   assert(s.load_revision==(refused?2:1) && count(&opens)==1);
   if(!refused) {
    dts_playback_audio_at(p,4,3);wait_count(&flushes,2);
    if(!paused)wait_count(&plays,3);
    s=wait_status(p,0,0);assert(s.info.audio_stream==4 && s.load_revision==1);
    assert(count(&loads)==1 && count(&creates)==1 && count(&destroys)==0);
   }
  }
  dts_playback_close(p);assert(count(&creates)==count(&destroys));
  printf("DV %s: fixed output reuses v12 Seek; pause, opt-in and refused-Seek fallback checked.\n",argv[1]);return 0;
 }
 if(!strncmp(argv[1],"handoff-",8)) {
  handoff_fixture=seamless_fixture=1;scenario=SEEK_PREROLL;
  int paused=strstr(argv[1],"paused")!=NULL;
  int cancel=strstr(argv[1],"cancel")!=NULL;
  int refused=strstr(argv[1],"refused")!=NULL;
  int failed_seek=strstr(argv[1],"disabled")!=NULL;
  prime_failure=strstr(argv[1],"prime-error")!=NULL;
  dts_playback_next_seamless(1);dts_playback_next_dv(1);
  p=start(paused,4,NULL,0,0);wait_count(&accepted,paused?1:4);
  if(paused)wait_count(&pauses,1);else wait_count(&plays,1);
  /* Reproduce a previously refused Seek: audio preparation must still hold
   * the old plane, without re-enabling the failed fast path. */
  if(failed_seek) {
   flush_refuse=1;dts_playback_seek(p,0.5);wait_count(&loads,2);wait_count(&plays,2);
   flush_refuse=0;
  }
  int old_loads=count(&loads),old_destroy=count(&destroys),old_audio=count(&audio_reconfigs);
  int old_plays=count(&plays);
  pause_refuse=refused;
  atomic_store(&prime_block,!refused);
  dts_playback_audio_at(p,9,2.0);
  if(!refused) {
   for(int i=0;i<1500 && !atomic_load(&prime_entered);i++)nap();
   assert(atomic_load(&prime_entered));
   assert(count(&loads)==old_loads && count(&destroys)==old_destroy);
   dts_playback_status(p,&s);assert(!s.failed && !s.prepared);
   // Retired-player callbacks must not overwrite the new timeline or fail it.
   emit("{\"currentTime\":{\"currentTime\":999999},\"error\":{\"errorCode\":200}}");
   if(cancel) {
    dts_playback_close(p);assert(count(&loads)==old_loads);
    assert(count(&creates)==count(&destroys));return 0;
   }
   atomic_store(&prime_release,1);
  }
  wait_count(&loads,old_loads+1);wait_count(&accepted,paused?3:8);
  if(paused) { wait_count(&pauses,3);dts_playback_pause(p,0); }
  wait_count(&plays,old_plays+1);
  s=wait_status(p,0,0);
  assert(s.info.audio_stream==9 && s.info.dovi_profile==8);
  assert(s.load_revision==old_loads+1);
  assert(!strcmp(s.info.audio_codec,"ac3") && s.info.channels==6);
  assert(last_audio_target==2.0 && last_load_target==2.0 && count(&opens)==1);
  assert(count(&audio_reconfigs)==old_audio+1);
  int old_flush=count(&flushes);
  dts_playback_seek(p,3.0);
  if(failed_seek || refused) {
   wait_count(&loads,old_loads+2);assert(count(&flushes)==old_flush);
  } else {
   wait_count(&flushes,old_flush+1);wait_count(&plays,old_plays+2);
   assert(count(&loads)==old_loads+1); // Expected audio format changes do not disable Seek.
   dts_playback_status(p,&s);assert(s.load_revision==old_loads+1);
  }
  // Change back from native AC3 5.1 to converted AAC stereo, keeping DV/cache.
  dts_playback_audio_at(p,4,4.0);wait_count(&loads,old_loads+(failed_seek||refused?3:2));
  s=wait_status(p,0,0);assert(s.info.audio_stream==4 && s.info.dovi_profile==8);
  assert(!strcmp(s.info.audio_codec,"aac") && s.info.channels==2 && count(&opens)==1);
  dts_playback_close(p);assert(count(&creates)==count(&destroys));
  printf("DV %s: pause/prepare/prime before Unload, one audio command and safe format Load passed.\n",argv[1]);return 0;
 }
#ifdef NV_DV_TRUEHD_TEST
 if(!strcmp(argv[1],"dv-truehd")) {
  scenario=DV_MAPPING;truehd_fixture=1;dts_playback_next_dv(1);
  DtsTrack selected={.kind=DTS_AUDIO,.channels=6};
  strcpy(selected.codec,"A_TRUEHD");strcpy(selected.language,"rus");
  p=start(0,-1,&selected,2,3);s=wait_status(p,0,0);
  assert(s.info.audio_stream==9 && s.info.dovi_profile==8 && count(&opens)==1);
  int streams[]={4,9};
  for(int j=0;j<2;j++) {
    dts_playback_audio(p,streams[j]);wait_count(&loads,j+2);
    for(int i=0;i<1500;i++) {
      dts_playback_status(p,&s);if(s.revision>=j+2)break;nap();
    }
    assert(!s.failed && s.revision>=j+2 && s.info.audio_stream==streams[j]);
    assert(s.info.dovi_profile==8 && count(&opens)==1);
  }
  dts_playback_close(p);
  puts("DV TrueHD controller: exact selected track and AC3/TrueHD changes retain DV and demux/cache");return 0;
 }
#endif
 if(!strncmp(argv[1],"resume-init-",12)) {
  int seconds=!strcmp(argv[1],"resume-init-seconds"),bad=!strcmp(argv[1],"resume-init-no-duration");
  int rejected=!strcmp(argv[1],"resume-init-rejected"),changed=!strcmp(argv[1],"resume-init-audio");
  int paused=!strcmp(argv[1],"resume-init-paused");
  fixture_duration=bad?0:INT64_C(7200000000000);
  dts_playback_next_resume_pct(67);
  if(rejected) assert(!dts_playback_start(NULL,"",-1,0,0,"",0,NULL,0,0));
  DtsTrack selected={.kind=DTS_AUDIO,.channels=changed?2:6};
  strcpy(selected.language,changed?"ja":"pt");strcpy(selected.codec,"dts");
  double expected=seconds?123.456:rejected?0:4824.0;
  p=dts_playback_start("fixture://movie","",-1,seconds?expected:0,paused,"window",0,&selected,changed?2:1,3);assert(p);
  s=wait_status(p,bad,0);
  if(bad)assert(count(&loads)==0 && strstr(s.error,"duration"));
  else {
   assert(count(&opens)==1 && count(&loads)==1 && s.start_position==expected);
   assert(last_load_target==expected && s.info.audio_stream==(changed?9:4));
   assert(last_open_target==0 && (changed?last_audio_target:last_seek)==expected);
   if(paused)assert(count(&plays)==0);
   // An ordinary later seek must never reapply the initial percentage.
   dts_playback_seek(p,100);wait_count(&loads,2);
   assert(last_load_target==100);
  }
  dts_playback_close(p);puts("DTS initial resume: selected target in first Load, no startup reload and no percent leak passed");return 0;
 }
 if(!strncmp(argv[1],"startup-",8)) {
  int same=!strcmp(argv[1],"startup-same"),fallback=!strcmp(argv[1],"startup-fallback");
  missing_dv=!strcmp(argv[1],"startup-no-dv");audio_fail_once=fallback;
  dts_playback_next_dv(1);dts_playback_next_p7(1);scenario=DV_MAPPING;
  DtsTrack selected={.kind=DTS_AUDIO,.channels=6};strcpy(selected.language,"rus");
  strcpy(selected.codec,same?"A_AC3":"A_DTS/LOSSLESS");
  p=dts_playback_start("fixture://movie","",-1,5157.631,0,"window",0,&selected,same?1:2,3);assert(p);
  s=wait_status(p,missing_dv,0);
  if(missing_dv)assert(count(&loads)==0 && strstr(s.error,"configuration"));
  else {
   assert(s.info.audio_stream==(same?4:9));
   assert(count(&opens)==(fallback?2:1));
   if(fallback)assert(last_open_target==5157.631);
   else assert(last_open_target==0 && (same?last_seek:last_audio_target)==5157.631);
  }
  dts_playback_close(p);puts("DV startup: selected audio/cache reuse, one target seek, safe reopen and missing-DV rejection passed");return 0;
 }
 if(!strncmp(argv[1],"buffered-",9)) {
  seamless_fixture=1;
  int broken=!strcmp(argv[1],"buffered-stall") || !strcmp(argv[1],"buffered-dv-stall"),audio=!strcmp(argv[1],"buffered-audio") || !strcmp(argv[1],"buffered-format");
  int paused=strstr(argv[1],"paused")!=NULL;
  stale_clock=!strcmp(argv[1],"buffered-stale");
  flush_refuse=!strcmp(argv[1],"buffered-refused");audio_format_change=!strcmp(argv[1],"buffered-format");
  scenario=broken?SEEK_SEGMENT:SEEK_PREROLL;
  dts_playback_next_seamless(1);
  if(strstr(argv[1],"buffered-dv"))dts_playback_next_dv(1);
  int prime=!strcmp(argv[1],"buffered-dv-prime"),prime_cancel=!strcmp(argv[1],"buffered-dv-cancel");
  if(prime || prime_cancel)atomic_store(&prime_block,1);
  p=start(paused,4,NULL,0,0);wait_count(&accepted,paused?1:4);
  if(paused)wait_count(&pauses,1);else wait_count(&plays,1);
  int accepted_before=count(&accepted);
  if(audio)dts_playback_audio(p,9);else dts_playback_seek(p,broken?546.05:2.0);
  if(prime || prime_cancel) {
   for(int i=0;i<1500 && !atomic_load(&prime_entered);i++)nap();
   assert(atomic_load(&prime_entered) && count(&flushes)==0 && count(&loads)==1);
   wait_count(&pauses,1);
   if(prime_cancel) {
    dts_playback_close(p);assert(count(&creates)==count(&destroys));return 0;
   }
   atomic_store(&prime_release,1);
  }
  if(broken || stale_clock || flush_refuse || audio_format_change) {
   int i;for(i=0;i<7000 && count(&loads)<2;i++)nap();assert(i<7000);
   assert(count(&creates)==2);
   if(broken || stale_clock) {
    assert(count(&opens)==1); // Failed renderer timeline must not reopen HTTP/demux.
    // The captured old-segment clock/BufferFull failure gets a bounded reload.
    int old=count(&flushes);dts_playback_seek(p,10);wait_count(&loads,3);
    assert(count(&flushes)==old); // Disabled for the rest of this session.
    assert(count(&opens)==1);
   }
  } else {
   wait_count(&flushes,1);wait_count(&accepted,accepted_before+2);
   if(paused) { wait_count(&pauses,2);dts_playback_pause(p,0);wait_count(&plays,1); }
   else wait_count(&plays,2);
   assert(count(&loads)==1 && count(&creates)==1 && count(&opens)==1);
   dts_playback_status(p,&s);assert(!s.failed && s.native_loaded && s.info.audio_stream==(audio?9:4));
   if(strstr(argv[1],"buffered-dv")) {
    assert(prime_serial>0);
    assert(last_dv && last_p7);
    int old_plays=count(&plays);
    dts_playback_audio(p,9);wait_count(&flushes,2);wait_count(&plays,old_plays+1);
    assert(count(&loads)==1 && count(&opens)==1 && last_stream==9);
    old_plays=count(&plays);
    dts_playback_seek(p,0.25);wait_count(&flushes,3);wait_count(&plays,old_plays+1);
    assert(count(&loads)==1 && count(&creates)==1 && count(&opens)==1);
   }
   char event[2048];while(dts_playback_event(p,event,sizeof event))assert(!strstr(event,"1697863"));
  }
  dts_playback_close(p);assert(count(&creates)==count(&destroys));
  printf("DTS %s: native reuse or bounded fresh-Load fallback passed.\n",argv[1]);return 0;
 }
 if(!strcmp(argv[1],"buffer")) {
  assert(getenv("DV7_OFF"));scenario=NORMAL;p=start(1,4,NULL,0,0);
  wait_status(p,0,0);wait_count(&pauses,1);assert(!last_dv && !last_p7);
  int i;
  for(i=0;i<1500;i++) { dts_playback_status(p,&s);if(s.buffer.ready_bytes==21*1048576LL)break;nap(); }
  assert(i<1500 && s.buffer.capacity_bytes==68*1048576LL && s.buffered_until>=0);
  atomic_store(&buffer_ready,23*1048576LL);
  for(i=0;i<1500;i++) { dts_playback_status(p,&s);if(s.buffer.ready_bytes==23*1048576LL)break;nap(); }
  assert(i<1500); // Pausing does not freeze telemetry while prefetch completes.
  atomic_store(&timestamp_block_open,1);atomic_store(&seek_failure,1);
  dts_playback_seek(p,20);
  for(i=0;i<1500 && !atomic_load(&timestamp_open_entered);i++)nap();
  assert(i<1500);dts_playback_status(p,&s);
  assert(!s.prepared && !s.buffer.capacity_bytes && !s.buffered_until);
  dts_playback_close(p);
  puts("DTS buffer snapshots update while paused and clear on seek/reopen with DV/P7 off.");
  return 0;
 }
 if(!strncmp(argv[1],"timestamp",9)) {
  int bounded=!strcmp(argv[1],"timestampbound"), progress=!strcmp(argv[1],"timestampprogress");
  int reopen_fail=!strcmp(argv[1],"timestampopenfail"), cancel=!strcmp(argv[1],"timestampcancel");
  int pause=!strcmp(argv[1],"timestamppause"), dv=!strcmp(argv[1],"timestampdv");
  int manual_seek=!strcmp(argv[1],"timestampseek"), audio_switch=!strcmp(argv[1],"timestampaudio");
  scenario=pause?TIMESTAMP_PAUSED:cancel?TIMESTAMP_CANCEL:bounded||progress||manual_seek||audio_switch?TIMESTAMP_REPEAT:TIMESTAMP_ONCE;
  dts_playback_next_dv(dv);p=start(pause,9,NULL,0,0);
  if(pause) wait_count(&nexts,1);else wait_count(&accepted,2);
  if(!pause) {
   dts_playback_subtitle(p,20);wait_count(&sub_created,1);
   dts_playback_volume(p,25);wait_count(&volumes,2);
  }
  emit("{\"currentTime\":{\"currentTime\":259634}}");
  if(reopen_fail) atomic_store(&timestamp_fail_open,1);
  if(cancel) atomic_store(&timestamp_block_open,1);
  atomic_store(&timestamp_fault,1);
  if(cancel) {
   for(int i=0;i<1500 && !atomic_load(&timestamp_open_entered);i++) nap();
   assert(atomic_load(&timestamp_open_entered));dts_playback_close(p);assert(count(&loads)==1);
  } else if(getenv("RECOVERY_OFF")) {
   s=wait_status(p,1,0);assert(!strcmp(s.error,DTS_ENGINE_TIMESTAMP_DISCONTINUITY));
   wait_count(&destroys,1);
   assert(count(&opens)==1 && count(&loads)==1 && count(&destroys)==1);
   char event[2048];
   while(dts_playback_event(p,event,sizeof event)) assert(!strstr(event,"timestamp-reload"));
   dts_playback_close(p);
  } else if(reopen_fail) {
   s=wait_status(p,1,0);assert(strstr(s.error,"reopen failure"));
   assert(count(&opens)==2 && count(&loads)==1);dts_playback_close(p);
  } else {
   wait_count(&loads,2);
   if(pause) { wait_count(&accepted,1);wait_count(&pauses,2);wait_settled();assert(count(&accepted)==1); }
   else { wait_count(&accepted,4);wait_count(&sub_created,2);wait_count(&volumes,4);assert(last_volume==25); }
   assert(last_stream==9 && last_open_target==259.634 && count(&core_opens)==0 && last_dv==dv);
   if(bounded || progress || manual_seek || audio_switch) {
    emit("{\"currentTime\":{\"currentTime\":260000}}");atomic_store(&timestamp_fault,1);wait_count(&loads,3);
    if(manual_seek) { dts_playback_seek(p,260);wait_count(&loads,4); }
    if(audio_switch) { dts_playback_audio(p,4);wait_count(&loads,4); }
    emit(progress?"{\"currentTime\":{\"currentTime\":291000}}":"{\"currentTime\":{\"currentTime\":260000}}");atomic_store(&timestamp_fault,1);
    if(bounded) { s=wait_status(p,1,0);assert(strstr(s.error,"timestamp recovery exhausted"));assert(count(&opens)==3); }
    else { wait_count(&loads,manual_seek||audio_switch?5:4);dts_playback_status(p,&s);assert(!s.failed); }
   } else { dts_playback_status(p,&s);assert(!s.failed && s.revision==2); }
   char event[2048];int saw_reload=0;
   while(dts_playback_event(p,event,sizeof event)) { assert(!strstr(event,"1697863"));saw_reload|=strstr(event,"timestamp-reload")!=NULL; }
   assert(saw_reload);dts_playback_close(p);assert(count(&sub_created)==count(&sub_destroyed));
  }
 } else if(!strcmp(argv[1],"preparefail")) {
  scenario=OPEN_FAIL;p=start(0,4,NULL,0,0);s=wait_status(p,1,0);assert(strstr(s.error,"engine failure"));assert(count(&loads)==0);dts_playback_close(p);
 } else if(!strcmp(argv[1],"pause")) {
  p=start(1,4,NULL,0,0);wait_status(p,0,0);wait_count(&accepted,1);wait_count(&pauses,1);
  int before=count(&accepted);for(int i=0;i<25;i++)nap();assert(count(&accepted)<=before+1);
  dts_playback_pause(p,0);wait_count(&plays,1);wait_count(&accepted,before+2);dts_playback_pause(p,1);wait_count(&pauses,2);dts_playback_close(p);
 } else if(!strcmp(argv[1],"backpressure")) {
  scenario=BACKPRESSURE;backpressure_left=3;p=start(0,4,NULL,0,0);wait_count(&accepted,1);assert(count(&feeds)>=4);assert(!count(&bad_retry));dts_playback_close(p);
 } else if(!strcmp(argv[1],"delayedid")) {
  scenario=DELAYED_ID;p=start(0,4,NULL,0,0);wait_count(&plays,1);
  dts_playback_status(p,&s);assert(s.prepared && s.revision==1 && !s.failed && !strcmp(s.media_id,"fixture-media"));
  dts_playback_close(p);
 } else if(!strcmp(argv[1],"cancel")) {
  scenario=BLOCK_READ;p=start(0,4,NULL,0,0);wait_count(&nexts,1);dts_playback_close(p);assert(count(&destroys)==1);
 } else if(!strcmp(argv[1],"seek")) {
  p=start(0,4,NULL,0,0);wait_count(&accepted,2);
  dts_playback_seek(p,20);wait_count(&loads,2);dts_playback_seek(p,35);wait_count(&loads,3);
  assert(count(&flushes)==0 && count(&seeks)==2 && count(&opens)==1);assert(count(&creates)==3 && last_seek==35);dts_playback_close(p);
 } else if(!strcmp(argv[1],"seekfallback") || !strcmp(argv[1],"resumeseek")) {
  p=start(0,4,NULL,0,0);wait_count(&accepted,2);
  int fallback=!strcmp(argv[1],"seekfallback");atomic_store(&seek_failure,fallback);
  dts_playback_seek(p,729.501);wait_count(&loads,2);wait_count(&plays,2);
  assert(count(&creates)==2 && count(&seeks)==1 && count(&opens)==(fallback?2:1));
  assert(fallback?last_open_target==729.501:last_seek==729.501);
  dts_playback_status(p,&s);assert(!s.failed && s.revision==2);dts_playback_close(p);
 } else if(!strcmp(argv[1],"seeksegment")) {
  scenario=SEEK_SEGMENT;p=start(0,9,NULL,0,0);wait_count(&accepted,4);
  dts_playback_subtitle(p,20);wait_count(&sub_created,1);
  dts_playback_volume(p,25);wait_count(&volumes,3);
  emit("{\"currentTime\":{\"currentTime\":1697863}}");
  int before=count(&accepted);dts_playback_seek(p,546.05);wait_count(&loads,2);
  wait_count(&accepted,before+4);wait_count(&plays,2);wait_count(&sub_created,2);wait_count(&volumes,5);
  assert(count(&flushes)==0 && count(&seeks)==1 && count(&opens)==1 && count(&creates)==2);
  assert(last_seek==546.05 && last_stream==9 && last_volume==25);
  pthread_mutex_lock(&fixture);assert(frozen_clock>INT64_C(546050000000));pthread_mutex_unlock(&fixture);
  dts_playback_status(p,&s);assert(!s.failed && s.revision==2 && s.native_loaded);
  char queued[2048];while(dts_playback_event(p,queued,sizeof queued)) assert(!strstr(queued,"1697863"));
  dts_playback_close(p);assert(count(&sub_created)==count(&sub_destroyed));
 } else if(!strcmp(argv[1],"pausedseek")) {
  scenario=SEEK_PREROLL;p=start(0,4,NULL,0,0);wait_count(&plays,1);wait_count(&accepted,4);
  dts_playback_pause(p,1);wait_count(&pauses,1);wait_settled();
  emit("{\"currentTime\":{\"currentTime\":1697863}}");
  int before=count(&accepted);dts_playback_seek(p,20);wait_count(&loads,2);
  wait_count(&accepted,before+2);wait_settled();
  pthread_mutex_lock(&fixture);assert(fed_audio_pts>=INT64_C(20000000000));assert(fed_video_pts>=INT64_C(20000000000));pthread_mutex_unlock(&fixture);
  assert(count(&accepted)==before+2);assert(count(&pauses)==2);
  char queued[2048];while(dts_playback_event(p,queued,sizeof queued)) assert(!strstr(queued,"1697863"));
  assert(count(&seek_rendered)==0);
  dts_playback_pause(p,0);wait_count(&plays,2);wait_count(&accepted,before+4);
  wait_count(&seek_rendered,1);
  dts_playback_pause(p,1);wait_count(&pauses,3);wait_settled();
  before=count(&accepted);dts_playback_seek(p,35);dts_playback_pause(p,0);
  wait_count(&loads,3);wait_count(&plays,3);wait_count(&accepted,before+4);
  dts_playback_status(p,&s);assert(!s.failed);dts_playback_close(p);
 } else if(!strcmp(argv[1],"pacing")) {
  scenario=PACING;p=start(0,4,NULL,0,0);wait_count(&accepted,131);
  for(int i=0;i<40;i++)nap();
  int before=count(&accepted);assert(before==131); /* Audio7s/video6.1s, no native clock yet. */
  emit("{\"currentTime\":{\"currentTime\":1000}}");wait_count(&accepted,before+5);
  for(int i=0;i<40;i++) { nap(); }
  assert(count(&accepted)==142);
  dts_playback_pause(p,1);wait_count(&pauses,1);
  emit("{\"currentTime\":{\"currentTime\":2000}}");
  before=count(&accepted);for(int i=0;i<25;i++)nap();assert(count(&accepted)==before);
  dts_playback_pause(p,0);wait_count(&plays,2);wait_count(&accepted,before+5);
  /* A seek must discard the preceding native presentation position. */
  dts_playback_seek(p,0);wait_count(&loads,2);
  before=count(&accepted);for(int i=0;i<100;i++)nap();assert(count(&accepted)<=before+132);
  dts_playback_status(p,&s);assert(!s.failed);dts_playback_close(p);
 } else if(!strcmp(argv[1],"ordinal")) {
  p=start(0,-1,NULL,1,3);s=wait_status(p,0,0);assert(s.info.audio_stream==4);dts_playback_close(p);
 } else if(!strcmp(argv[1],"filtered") || !strcmp(argv[1],"ambiguous")) {
  DtsTrack selected={.kind=DTS_AUDIO,.channels=2};strcpy(selected.language,"jpn");strcpy(selected.codec,"dts");
  scenario=!strcmp(argv[1],"ambiguous")?MAPPING_BAD:MAPPING;p=start(0,-1,&selected,0,1);
  s=wait_status(p,scenario==MAPPING_BAD,0);
  if(scenario==MAPPING_BAD)assert(strstr(s.error,"safely identify"));else assert(last_stream==9 && count(&opens)==1 && count(&audio_reconfigs)==1);
  dts_playback_close(p);
 } else if(!strcmp(argv[1],"dv-codec") || !strcmp(argv[1],"dv-file-ordinal")) {
  scenario=DV_MAPPING;dts_playback_next_dv(1);
  DtsTrack selected={.kind=DTS_AUDIO,.channels=6};strcpy(selected.language,"rus");
  int direct=!strcmp(argv[1],"dv-file-ordinal");
  strcpy(selected.codec,direct?"A_DTS/LOSSLESS":"A_AC3");
  p=start(0,-1,&selected,direct?2:0,direct?3:1);
  s=wait_status(p,0,0);assert(s.info.audio_stream==(direct?9:4));
  assert(last_dv && last_p7);
  assert(count(&opens)==1 && count(&audio_reconfigs)==direct);dts_playback_close(p);
 } else if(!strcmp(argv[1],"audio")) {
  p=start(0,4,NULL,0,0);wait_status(p,0,0);dts_playback_audio(p,9);wait_count(&loads,2);s=wait_status(p,0,0);assert(!s.failed);assert(last_stream==9 && count(&opens)==1);assert(count(&creates)==2);dts_playback_close(p);
 } else if(!strcmp(argv[1],"audioerror") || !strcmp(argv[1],"eoferror")) {
  scenario=!strcmp(argv[1],"eoferror")?EOF_MODE:NORMAL;
  p=start(0,4,NULL,0,0);wait_count(&accepted,2);
  if(scenario==EOF_MODE) wait_status(p,0,1);
  emit("{\"error\":{\"errorCode\":200}}");
  s=wait_status(p,1,0);assert(strstr(s.error,"stereo"));assert(count(&loads)==1);dts_playback_close(p);
 } else if(!strcmp(argv[1],"eof")) {
  scenario=EOF_MODE;p=start(0,4,NULL,0,0);s=wait_status(p,0,1);assert(!s.failed);assert(count(&eoses)==1);assert(count(&destroys)==0);
  dts_playback_seek(p,3);wait_count(&loads,2);wait_count(&eoses,2);dts_playback_close(p);
 } else if(!strcmp(argv[1],"pausedreload")) {
  p=start(1,4,NULL,0,0);wait_count(&pauses,1);wait_count(&accepted,1);
  emit("{\"currentTime\":{\"currentTime\":100000}}");dts_playback_audio(p,9);wait_count(&loads,2);
  assert(last_audio_target==100 && count(&opens)==1);wait_count(&accepted,2);wait_count(&pauses,2);dts_playback_close(p);
 } else if(!strcmp(argv[1],"eofpause")) {
  scenario=EOF_MODE;p=start(0,4,NULL,0,0);wait_status(p,0,1);
  dts_playback_pause(p,1);wait_count(&pauses,1);dts_playback_pause(p,0);wait_count(&plays,2);
  for(int i=0;i<25;i++) { nap(); }
  assert(count(&eoses)==1);dts_playback_close(p);
 } else if(!strcmp(argv[1],"subtitles")) {
  scenario=SUBTITLES;p=start(0,4,NULL,0,0);wait_status(p,0,0);dts_playback_subtitle(p,20);wait_count(&sub_decoded,1);
  dts_playback_pause(p,1);wait_count(&pauses,1);char text[64];double at=last_sub_pts/1e9;
  assert(dts_playback_subtitle_text(p,at,text,sizeof text));assert(!strcmp(text,"fixture cue"));
  DtsSubtitle cue;unsigned revision=0;
  assert(dts_playback_subtitle_bitmap(p,at,0,&cue,&revision));assert(revision && cue.rgba && cue.rgba[0]==3);
  cue.rgba[0]=77;free(cue.rgba);assert(!dts_playback_subtitle_bitmap(p,at,revision,&cue,&revision));
  assert(dts_playback_subtitle_bitmap(p,at,999,&cue,&revision));assert(cue.rgba[0]==3);free(cue.rgba);
  dts_playback_subtitle(p,-1);wait_count(&sub_destroyed,1);assert(!dts_playback_subtitle_text(p,at,text,sizeof text));
  assert(dts_playback_subtitle_bitmap(p,at,revision,&cue,&revision));assert(!revision && !cue.rgba);dts_playback_close(p);
  assert(count(&sub_created)==count(&sub_destroyed));
 } else if(!strcmp(argv[1],"reordered")) {
  DtsTrack selected={.kind=DTS_AUDIO,.channels=2};strcpy(selected.language,"jpn");strcpy(selected.codec,"dts");
  p=start(0,-1,&selected,1,3);s=wait_status(p,0,0);assert(s.info.audio_stream==9);dts_playback_close(p);
 } else if(!strcmp(argv[1],"core") || !strcmp(argv[1],"nocore")) {
  scenario=!strcmp(argv[1],"core")?CORE_RECOVERY:NO_CORE_FAIL;
  p=start(0,4,NULL,0,0);
  if(scenario!=NO_CORE_FAIL) { wait_status(p,0,0);emit("{\"currentTime\":{\"currentTime\":123450}}");atomic_store(&allow_decode_failure,1); }
  if(scenario==NO_CORE_FAIL) { s=wait_status(p,1,0);assert(count(&core_opens)==0);assert(count(&opens)<=2); }
  else { wait_count(&accepted,2);assert(count(&core_opens)==1);assert(count(&opens)==2);assert(last_stream==4);assert(last_open_target==123.45); }
  dts_playback_close(p);
 } else if(!strcmp(argv[1],"pausedcore")) {
  scenario=CORE_RECOVERY;p=start(1,4,NULL,0,0);wait_status(p,0,0);
  emit("{\"currentTime\":{\"currentTime\":123450}}");atomic_store(&allow_decode_failure,1);
  wait_count(&loads,2);wait_count(&accepted,1);wait_count(&pauses,2);assert(last_open_target==123.45);
  int before=count(&accepted);for(int i=0;i<25;i++) { nap(); }assert(count(&accepted)<=before+1);dts_playback_close(p);
 } else if(!strcmp(argv[1],"cpu")) {
  scenario=CPU_BUDGET;p=start(0,4,NULL,0,0);s=wait_status(p,1,0);
  assert(strstr(s.error,"CPU budget"));assert(count(&opens)==2 && count(&core_opens)==1);dts_playback_close(p);
 } else if(!strcmp(argv[1],"volume")) {
  p=start(0,4,NULL,0,0);wait_count(&accepted,2);wait_count(&volumes,2);assert(last_volume==100);
  int before=count(&volumes);dts_playback_volume(p,25);wait_count(&volumes,before+1);assert(last_volume==25);
  before=count(&volumes);dts_playback_audio(p,9);wait_count(&loads,2);wait_count(&volumes,before+2);assert(last_volume==25);
  before=count(&volumes);dts_playback_volume(p,50);wait_count(&volumes,before+1);dts_playback_status(p,&s);assert(!s.failed);dts_playback_close(p);
 } else if(!strcmp(argv[1],"overflow")) {
  p=start(0,4,NULL,0,0);wait_count(&accepted,1);
  void (*cb)(void *,const char *);void *u;
  pthread_mutex_lock(&fixture);cb=native_event;u=native_user;pthread_mutex_unlock(&fixture);
  for(int i=0;i<40;i++)cb(u,"{\"playing\":{}}");
  s=wait_status(p,1,0);assert(strstr(s.error,"queue overflow"));dts_playback_close(p);
 } else if(!strcmp(argv[1],"controls")) {
  p=start(0,4,NULL,0,0);wait_count(&plays,1);wait_count(&accepted,2);
  for(int i=0;i<10;i++) { dts_playback_pause(p,0);emit("{\"loadCompleted\":{}}");nap(); }
  assert(count(&plays)==1);dts_playback_pause(p,1);wait_count(&pauses,1);
  for(int i=0;i<10;i++) { dts_playback_pause(p,1);nap(); }
  assert(count(&pauses)==1);dts_playback_close(p);
 } else if(!strcmp(argv[1],"stallpaced")) {
  scenario=NATIVE_STALL;atomic_store(&stall_wall_ns,INT64_C(100000000000));
  p=start(0,9,NULL,0,0);wait_settled();
  int before=count(&accepted);assert(before>100 && atomic_load(&native_full)==0);
  pthread_mutex_lock(&fixture);
  assert(fed_audio_pts-INT64_C(298381000000)>INT64_C(6000000000));
  assert(fed_video_pts-INT64_C(298381000000)>INT64_C(6000000000));
  pthread_mutex_unlock(&fixture);
  atomic_store(&stall_wall_ns,INT64_C(109000000000));wait_count(&loads,2);
  wait_count(&accepted,before+4);assert(atomic_load(&native_full)==0);
  assert(last_open_target==298.381);
  pthread_mutex_lock(&fixture);assert(frozen_clock>INT64_C(298381000000));pthread_mutex_unlock(&fixture);
  dts_playback_close(p);
 } else if(!strcmp(argv[1],"stallrecover") || !strcmp(argv[1],"stallbound") ||
           !strcmp(argv[1],"stallnoclock") || !strcmp(argv[1],"stalltransient") ||
           !strcmp(argv[1],"stallpause") || !strcmp(argv[1],"stallseek") || !strcmp(argv[1],"stallpausedseek") ||
           !strcmp(argv[1],"stallrecovernoclock") || !strcmp(argv[1],"stallrecovercancel") || !strcmp(argv[1],"stallseeknoclock") || !strcmp(argv[1],"stallrecoverunloaded")) {
  scenario=!strcmp(argv[1],"stallrecoverunloaded")?NATIVE_RECOVERY_UNLOADED:
           (!strcmp(argv[1],"stallrecovernoclock") || !strcmp(argv[1],"stallrecovercancel") || !strcmp(argv[1],"stallseeknoclock"))?NATIVE_RECOVERY_NO_CLOCK:
           !strcmp(argv[1],"stallbound")?NATIVE_REPEAT:
           !strcmp(argv[1],"stallnoclock")?NATIVE_NO_CLOCK:
           !strcmp(argv[1],"stalltransient")?NATIVE_TRANSIENT:NATIVE_STALL;
  atomic_store(&stall_wall_ns,INT64_C(100000000000));
  p=start(0,9,NULL,0,0);wait_count(&accepted,4);
  dts_playback_volume(p,25);wait_count(&volumes,3);
  int user_seek=!strcmp(argv[1],"stallseek") || !strcmp(argv[1],"stallpausedseek") || !strcmp(argv[1],"stallseeknoclock");
  if(user_seek) {
   atomic_store(&jam_native,1);
   for(int i=0;i<1500 && atomic_load(&native_full)<25;i++) { nap(); }assert(atomic_load(&native_full)>=25);
   if(!strcmp(argv[1],"stallpausedseek") || !strcmp(argv[1],"stallseeknoclock")) { dts_playback_pause(p,1);wait_count(&pauses,1); }
   dts_playback_seek(p,42);wait_count(&loads,2);assert(count(&flushes)==0 && (last_seek==42 || last_open_target==42));
   if(!strcmp(argv[1],"stallpausedseek") || !strcmp(argv[1],"stallseeknoclock")) { wait_count(&pauses,2);dts_playback_pause(p,0); }
   wait_count(&plays,2);
   if(!strcmp(argv[1],"stallseeknoclock")) { wait_count(&loads,3);wait_count(&loads,4);s=wait_status(p,1,0);assert(strstr(s.error,"two recovery")); }
  } else if(!strcmp(argv[1],"stallpause")) {
   dts_playback_pause(p,1);wait_count(&pauses,1);wait_settled();
   atomic_store(&jam_native,1);atomic_fetch_add(&stall_wall_ns,INT64_C(60000000000));
   for(int i=0;i<40;i++) { nap(); }assert(count(&loads)==1);
   dts_playback_pause(p,0);wait_count(&loads,2);
  } else {
   atomic_store(&jam_native,1);
   if(scenario==NATIVE_NO_CLOCK || scenario==NATIVE_TRANSIENT) {
    for(int i=0;i<1500 && atomic_load(&native_full)<100;i++)nap();
    assert(atomic_load(&native_full)>=30);assert(count(&loads)==1);
    dts_playback_status(p,&s);assert(!s.failed);
   } else {
    wait_count(&loads,2);
    if(scenario==NATIVE_REPEAT || scenario==NATIVE_RECOVERY_UNLOADED || (scenario==NATIVE_RECOVERY_NO_CLOCK && strcmp(argv[1],"stallrecovercancel"))) {
     wait_count(&loads,3);s=wait_status(p,1,0);assert(strstr(s.error,"two recovery"));
    }
   }
  }
  if(scenario==NATIVE_STALL && !user_seek) {
   wait_count(&plays,2);wait_count(&volumes,5);
   assert(last_stream==9 && last_volume==25 && last_open_target>0);
   /* Reload targets the renderer's final clock, never the blocked packet. */
   pthread_mutex_lock(&fixture);
   assert(original_stall_clock==INT64_C(298381000000));
   assert(stall_audio_pts-original_stall_clock>INT64_C(5000000000));
   assert(stall_video_pts-original_stall_clock>INT64_C(5000000000));
   assert(last_open_target*1e9==original_stall_clock);
   pthread_mutex_unlock(&fixture);
   wait_count(&accepted,20);pthread_mutex_lock(&fixture);assert(frozen_clock>original_stall_clock);pthread_mutex_unlock(&fixture);
  }
  dts_playback_close(p);
 } else if(!strcmp(argv[1],"sustained")) {
  scenario=SUSTAINED;p=start(0,4,NULL,0,0);wait_count(&accepted,1);wait_settled();
  for(int second=1;second<=30;second++) {
   char event[96];snprintf(event,sizeof event,"{\"currentTime\":{\"currentTime\":%d}}",second*1000);emit(event);
   wait_settled();
  }
  atomic_store(&stall_transport,1);
  emit("{\"currentTime\":{\"currentTime\":30100}}");
  for(int i=0;i<1500 && !atomic_load(&transport_stalled);i++) nap();
  assert(atomic_load(&transport_stalled));
  /* A 2.052s HTTP range stall after 30s must leave BOTH native buffers ahead
   * of presentation. The former 1.6s lead cannot survive this observed delay. */
  emit("{\"currentTime\":{\"currentTime\":32152}}");
  pthread_mutex_lock(&fixture);assert(fed_audio_pts>INT64_C(32152000000));assert(fed_video_pts>INT64_C(32152000000));pthread_mutex_unlock(&fixture);
  atomic_store(&stall_transport,0);atomic_store(&release_transport,1);wait_count(&accepted,760);
  dts_playback_status(p,&s);assert(!s.failed && count(&plays)==1);dts_playback_close(p);
 } else assert(0);
 assert(count(&creates)==count(&destroys));printf("DTS playback %s passed\n",argv[1]);return 0;
}

int dts_pipeline_volume(DtsPipeline *p,int pct) {
 assert(p->loaded);pthread_mutex_lock(&fixture);volumes++;last_volume=pct;pthread_mutex_unlock(&fixture);return pct!=50;
}

/* Deterministic CPU-budget check; native waits still use the real wall clock. */
int __real_clock_gettime(clockid_t,struct timespec *);
int __wrap_clock_gettime(clockid_t clock,struct timespec *out) {
 if((hold_fixture || mp4_seek_case) && clock==CLOCK_MONOTONIC) {
  int r=__real_clock_gettime(clock,out);int64_t ns=out->tv_nsec+atomic_load(&hold_wall_offset);
  out->tv_sec+=ns/INT64_C(1000000000);out->tv_nsec=ns%INT64_C(1000000000);return r;
 }
 if(scenario>=NATIVE_STALL && clock==CLOCK_MONOTONIC) {
  int64_t ns=atomic_load(&stall_wall_ns);out->tv_sec=ns/INT64_C(1000000000);out->tv_nsec=ns%INT64_C(1000000000);return 0;
 }
 if(scenario==CPU_BUDGET && (clock==CLOCK_MONOTONIC || clock==CLOCK_THREAD_CPUTIME_ID)) {
  out->tv_sec=clock==CLOCK_MONOTONIC?fake_wall:fake_thread_cpu;
  out->tv_nsec=0;return 0;
 }
 return __real_clock_gettime(clock,out);
}
