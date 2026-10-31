/* Exercise the real controller against the existing engine/native fixture. */
#define main dts_existing_fixture_main
#define DTS_REFILL_FIXTURE 1
#include "dts_playback.c"
#undef main
static DtsPlaybackStatus refill_status(DtsPlayback *p,int active) {
  DtsPlaybackStatus st={0};
  for(int i=0;i<1500;i++) {
    dts_playback_status(p,&st);
    if(!!st.buffering_ms==active)return st;
    assert(!st.failed);nap();
  }
  assert(!"refill state not published");return st;
}
int main(int argc,char **argv) {
  assert(argc==2);const char *mode=argv[1];
  int dv=strcmp(mode,"sdr")!=0;
  scenario=SEEK_PREROLL;hold_fixture=1;
  dts_playback_next_dv(dv);dts_playback_next_seamless(1);
  DtsPlayback *p=start(0,4,NULL,0,0);
  wait_count(&plays,1);wait_count(&accepted,10);wait_settled();
  pthread_mutex_lock(&fixture);
  int64_t end=fed_audio_pts<fed_video_pts?fed_audio_pts:fed_video_pts;
  pthread_mutex_unlock(&fixture);
  /* 160 MiB elsewhere cannot substitute for a contiguous current interval. */
  atomic_store(&buffer_ready,160*1048576LL);
  atomic_store(&buffer_contiguous,4*1048576LL);
  char event[128];snprintf(event,sizeof event,"{\"currentTime\":{\"currentTime\":%lld}}",(long long)(end/1000000-100));
  emit(event);refill_status(p,1);wait_count(&pauses,1);
  wait_settled();assert(count(&plays)==1 && count(&loads)==1 && count(&opens)==1);
  if(!strcmp(mode,"cancel")) {
    dts_playback_close(p);assert(count(&creates)==count(&destroys));return 0;
  }
  if(!strcmp(mode,"timeout")) {
    atomic_fetch_add(&hold_wall_offset,INT64_C(21000000000));
    DtsPlaybackStatus st=wait_status(p,1,0);
    assert(!strcmp(st.error,DTS_PLAYBACK_STARVED));
    assert(count(&loads)==1 && count(&plays)==1);
    dts_playback_close(p);return 0;
  }
  if(!strcmp(mode,"eof")) {
    atomic_store(&refill_eof,1);atomic_store(&refill_tail,1);
    wait_count(&plays,2);
    pthread_mutex_lock(&fixture);
    int64_t tail=fed_audio_pts<fed_video_pts?fed_audio_pts:fed_video_pts;
    pthread_mutex_unlock(&fixture);
    snprintf(event,sizeof event,"{\"currentTime\":{\"currentTime\":%lld}}",(long long)(tail/1000000-100));
    emit(event);
    DtsPlaybackStatus st=wait_status(p,0,1);
    assert(!st.failed && !st.buffering_ms);
    assert(count(&loads)==1);
    dts_playback_close(p);return 0;
  }
  if(!strcmp(mode,"paused")) {
    dts_playback_pause(p,1);wait_settled();
    atomic_store(&buffer_contiguous,20*1048576LL);
    atomic_fetch_add(&hold_wall_offset,INT64_C(60000000000));
    wait_settled();DtsPlaybackStatus st;dts_playback_status(p,&st);
    assert(!st.failed && count(&plays)==1);
    dts_playback_pause(p,0);
  } else if(!strcmp(mode,"seek")) {
    dts_playback_seek(p,50);wait_count(&flushes,1);refill_status(p,1);
    wait_settled();assert(count(&plays)==1 && last_seek==50);
    atomic_store(&buffer_contiguous,20*1048576LL);
  } else if(!strcmp(mode,"audio")) {
    atomic_store(&buffer_contiguous,20*1048576LL);
    dts_playback_audio_at(p,9,8);wait_count(&audio_reconfigs,1);
  } else {
    atomic_fetch_add(&hold_wall_offset,INT64_C(9000000000));
    wait_settled();assert(count(&plays)==1);
    atomic_store(&buffer_contiguous,20*1048576LL);
  }
  wait_count(&plays,2);DtsPlaybackStatus st=refill_status(p,0);
  assert(!st.failed && count(&opens)==1 && count(&loads)==1);
  dts_playback_close(p);assert(count(&creates)==count(&destroys));
  printf("PASS refill %s: continuous bytes, preserved pipeline/cache, user controls and cleared loading snapshot\n",mode);
}
