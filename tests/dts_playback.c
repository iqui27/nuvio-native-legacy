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
static double last_seek, last_open_target;
static int core_opens, volumes, last_volume, sub_decoded, sub_created, sub_destroyed, seek_rendered;
static int64_t last_sub_pts;
static atomic_int allow_decode_failure, transport_stalled, release_transport, stall_transport;
static atomic_int jam_native, native_full;
static atomic_llong stall_wall_ns;
static int64_t frozen_clock, original_stall_clock;
static int64_t stall_audio_pts, stall_video_pts;
static int64_t fed_audio_pts, fed_video_pts;
static long fake_wall=100, fake_thread_cpu;
static void (*native_event)(void *,const char *);
static void *native_user;
enum { NORMAL, OPEN_FAIL, BACKPRESSURE, BLOCK_READ, EOF_MODE, MAPPING, MAPPING_BAD, SUBTITLES, CORE_RECOVERY, NO_CORE_FAIL, CPU_BUDGET, DELAYED_ID, PACING, SUSTAINED, SEEK_PREROLL, SEEK_SEGMENT, NATIVE_STALL, NATIVE_REPEAT, NATIVE_NO_CLOCK, NATIVE_TRANSIENT, NATIVE_RECOVERY_NO_CLOCK, NATIVE_RECOVERY_UNLOADED };
struct DtsEngine { DtsMediaInfo info; atomic_int cancelled; unsigned char packet[98304]; int serial, core; double base; };
struct DtsPipeline { void (*event)(void *,const char *); void *user; int loaded, completed, running, flushed, audio_ready, video_ready, generation, packets, broken_segment; double target; };
static void nap(void) { struct timespec t={0,1000000};nanosleep(&t,NULL); }
static void emit(const char *json) {
 void (*cb)(void *,const char *);void *u;
 pthread_mutex_lock(&fixture);cb=native_event;u=native_user;pthread_mutex_unlock(&fixture);
 assert(cb);cb(u,json);
}
int dts_engine_available(void) { return 1; }
void dts_engine_dv(DtsEngine *e,int on) { (void)e; (void)on; }
DtsEngine *dts_engine_create(void) { return calloc(1,sizeof(DtsEngine)); }
int dts_engine_open(DtsEngine *e,const char *url,const char *headers,int stream,int core,double target) {
 (void)headers;assert(!strcmp(url,"fixture://movie"));
 pthread_mutex_lock(&fixture);opens++;last_stream=stream;last_open_target=target;if(core)core_opens++;pthread_mutex_unlock(&fixture);
 if(scenario==OPEN_FAIL) return -1;
 memset(&e->info,0,sizeof e->info);strcpy(e->info.video_codec,"hevc");strcpy(e->info.audio_codec,"aac");
 e->info.width=1920;e->info.height=1080;e->info.audio_stream=stream<0?4:stream;e->info.channels=2;e->info.sample_rate=48000;
 e->info.n_tracks=4;
 e->info.tracks[0]=(DtsTrack){.stream_index=0,.kind=DTS_VIDEO};
 e->info.tracks[1]=(DtsTrack){.stream_index=1,.kind=DTS_AUDIO,.channels=2};strcpy(e->info.tracks[1].codec,"aac");strcpy(e->info.tracks[1].language,"en");
 e->info.tracks[2]=(DtsTrack){.stream_index=4,.kind=DTS_AUDIO,.channels=6};strcpy(e->info.tracks[2].codec,"dts");strcpy(e->info.tracks[2].language,"pt");
 e->info.tracks[3]=(DtsTrack){.stream_index=9,.kind=DTS_AUDIO,.channels=2};strcpy(e->info.tracks[3].codec,"dts");strcpy(e->info.tracks[3].language,"ja");
 if(scenario==MAPPING_BAD) { e->info.tracks[4]=e->info.tracks[3];e->info.tracks[4].stream_index=10;e->info.n_tracks=5; }
 if(scenario==SUBTITLES || scenario==SEEK_SEGMENT) {
  e->info.tracks[4]=(DtsTrack){.stream_index=20,.kind=DTS_SUBTITLE};strcpy(e->info.tracks[4].codec,"srt");e->info.n_tracks=5;
 }
 e->core=core;e->base=target;e->serial=0;return 0;
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
 if(scenario==CPU_BUDGET) { fake_wall++;fake_thread_cpu++; }
 pthread_mutex_lock(&fixture);nexts++;pthread_mutex_unlock(&fixture);
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
 e->base=target;e->serial=0;
 pthread_mutex_lock(&fixture);seeks++;last_seek=target;pthread_mutex_unlock(&fixture);return 0;
}
void dts_engine_cancel(DtsEngine *e) { atomic_store(&e->cancelled,1); }
void dts_engine_destroy(DtsEngine *e) { free(e); }
const char *dts_engine_error(DtsEngine *e) {
 (void)e;return scenario==CORE_RECOVERY || scenario==NO_CORE_FAIL ? "DTS decode fixture failure" : "fixture engine failure";
}
int dts_pipeline_available(int major) { (void)major;return 1; }
DtsPipeline *dts_pipeline_create(const char *a,const char *w,int m,void (*cb)(void *,const char *),void *u) {
 (void)a;(void)w;(void)m;DtsPipeline *p=calloc(1,sizeof *p);p->event=cb;p->user=u;
 pthread_mutex_lock(&fixture);creates++;p->generation=creates;p->flushed=creates>1;native_event=cb;native_user=u;pthread_mutex_unlock(&fixture);return p;
}
int dts_pipeline_load(DtsPipeline *p,const DtsMediaInfo *m,double t) {
 (void)m; if(p->loaded) return 0;p->loaded=1;p->target=t;
 pthread_mutex_lock(&fixture);loads++;pthread_mutex_unlock(&fixture);return 1;
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
 if(scenario==SEEK_SEGMENT) {
  char clock[96];snprintf(clock,sizeof clock,"{\"currentTime\":{\"currentTime\":%lld}}",(long long)(f->pts_ns/1000000));
  pthread_mutex_lock(&fixture);frozen_clock=f->pts_ns;pthread_mutex_unlock(&fixture);
  p->event(p->user,clock);
 }
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
 p->running=1;pthread_mutex_lock(&fixture);plays++;
 if(scenario==SEEK_PREROLL && p->flushed && p->audio_ready && p->video_ready) seek_rendered++;
 pthread_mutex_unlock(&fixture);return 1;
}
int dts_pipeline_pause(DtsPipeline *p) { p->running=0;pthread_mutex_lock(&fixture);pauses++;pthread_mutex_unlock(&fixture);return 1; }
int dts_pipeline_flush(DtsPipeline *p,double t) {
 if(scenario==SEEK_SEGMENT) {
  /* Captured failure: flush emits one old-segment keyframe clock, then a
   * normal 95KB video packet fills the decoder forever. Fresh Load clears it. */
  p->broken_segment=1;p->event(p->user,"{\"currentTime\":{\"currentTime\":545503}}");
 }
 if(scenario==NATIVE_STALL && atomic_load(&jam_native) && p->generation==1) return 0;
 (void)t;p->running=0;p->flushed=1;p->audio_ready=p->video_ready=0;
 /* Firmware keeps the Load complete state but flushes its stopped decoder
  * queues. Presentation requires fresh A/V preroll and a new Play. */
 if(scenario==SEEK_PREROLL) p->event(p->user,"{\"currentTime\":{\"currentTime\":1697863}}");
 pthread_mutex_lock(&fixture);flushes++;pthread_mutex_unlock(&fixture);return 1;
}
int dts_pipeline_eos(DtsPipeline *p) { (void)p;pthread_mutex_lock(&fixture);eoses++;pthread_mutex_unlock(&fixture);return 1; }
void dts_pipeline_destroy(DtsPipeline *p) {
 if(!p)return;
 if(scenario==SEEK_PREROLL || scenario==SEEK_SEGMENT) p->event(p->user,"{\"currentTime\":{\"currentTime\":1697863}}");
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
 DtsPlayback *p=dts_playback_start("fixture://movie","X-Token: fixture",stream,0,paused,"window",0,selected,ordinal,total);assert(p);return p;
}
int main(int argc,char **argv) {
 assert(argc==2);assert(!dts_playback_enabled());DtsPlayback *p;DtsPlaybackStatus s;
 if(!strcmp(argv[1],"preparefail")) {
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
  assert(count(&flushes)==0 && count(&seeks)==0);assert(count(&creates)==3 && last_open_target==35);dts_playback_close(p);
 } else if(!strcmp(argv[1],"seeksegment")) {
  scenario=SEEK_SEGMENT;p=start(0,9,NULL,0,0);wait_count(&accepted,4);
  dts_playback_subtitle(p,20);wait_count(&sub_created,1);
  dts_playback_volume(p,25);wait_count(&volumes,3);
  emit("{\"currentTime\":{\"currentTime\":1697863}}");
  int before=count(&accepted);dts_playback_seek(p,546.05);wait_count(&loads,2);
  wait_count(&accepted,before+4);wait_count(&plays,2);wait_count(&sub_created,2);wait_count(&volumes,5);
  assert(count(&flushes)==0 && count(&seeks)==0 && count(&creates)==2);
  assert(last_open_target==546.05 && last_stream==9 && last_volume==25);
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
  if(scenario==MAPPING_BAD)assert(strstr(s.error,"safely identify"));else assert(last_stream==9);
  dts_playback_close(p);
 } else if(!strcmp(argv[1],"audio")) {
  p=start(0,4,NULL,0,0);wait_status(p,0,0);dts_playback_audio(p,9);wait_count(&loads,2);s=wait_status(p,0,0);assert(!s.failed);assert(last_stream==9);assert(count(&creates)==2);dts_playback_close(p);
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
  assert(last_open_target==100);wait_count(&accepted,2);wait_count(&pauses,2);dts_playback_close(p);
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
   dts_playback_seek(p,42);wait_count(&loads,2);assert(count(&flushes)==0 && last_open_target==42);
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
 if(scenario>=NATIVE_STALL && clock==CLOCK_MONOTONIC) {
  int64_t ns=atomic_load(&stall_wall_ns);out->tv_sec=ns/INT64_C(1000000000);out->tv_nsec=ns%INT64_C(1000000000);return 0;
 }
 if(scenario==CPU_BUDGET && (clock==CLOCK_MONOTONIC || clock==CLOCK_THREAD_CPUTIME_ID)) {
  out->tv_sec=clock==CLOCK_MONOTONIC?fake_wall:fake_thread_cpu;
  out->tv_nsec=0;return 0;
 }
 return __real_clock_gettime(clock,out);
}
