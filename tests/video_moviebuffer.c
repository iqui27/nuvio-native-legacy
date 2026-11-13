/* Real native buffer reporting and shutdown, using the existing bus fixture. */
#define main video_acb_fixture_main
#include "video_acb_lg.c"
#undef main
static int loads;
static char loaded[NV_MEDIA_URL_MAX+4096];
static int fake_buffer_load(LSHandle *h,const char *uri,const char *payload,
                            Filtro cb,void *ctx,unsigned long *token,void *error) {
  (void)h;(void)cb;(void)ctx;(void)token;(void)error;
  assert(strstr(uri,"/load"));loads++;
  snprintf(loaded,sizeof loaded,"%s",payload);return 1;
}
static void pending(unsigned id) {
  bufferLoad.session=id;
  snprintf(bufferLoad.source,sizeof bufferLoad.source,"https://fixture.invalid/movie.mkv");
  snprintf(bufferLoad.proxy,sizeof bufferLoad.proxy,"http://127.0.0.1:1234/movie.mkv");
  bufferLoad.dolby[0]=0;
}
int main(void) {
  ligado=1;bus=(LSHandle *)(uintptr_t)1;lsCall=fake_buffer_load;
  sessao=9;modoLoad=0;test_buffer_preset=3;
  pending(sessao);movie_ready=0;
  for(int i=0;i<100;i++)bufferLoadBombear();
  assert(!loads && bufferLoad.session==sessao); // No Load while still probing.
  movie_ready=1;bufferLoadBombear();bufferLoadBombear();
  assert(loads==1 && !bufferLoad.session && strstr(loaded,"127.0.0.1:1234"));
  assert(strstr(loaded,"\"userBufferCtrl\":false"));
  pending(sessao);movie_ready=-1;movie_active=1;bufferLoadBombear();
  assert(loads==2 && !movie_active && !bufferLoad.session);
  assert(strstr(loaded,"fixture.invalid") && strstr(loaded,"\"userBufferCtrl\":true"));
  pending(sessao-1);movie_ready=1;bufferLoadBombear();assert(loads==2);
  pending(sessao);movie_ready=0;video_parar();
  movie_ready=1;bufferLoadBombear();assert(loads==2 && !bufferLoad.session);
  ligado=0;

  // Keep upstream's complete URL allowance in the asynchronous fallback loader.
  char longUrl[NV_MEDIA_URL_MAX];memset(longUrl,'x',sizeof longUrl-1);longUrl[sizeof longUrl-1]=0;
  memcpy(longUrl,"https://fixture.invalid/",24);
  ligado=1;pending(sessao);snprintf(bufferLoad.source,sizeof bufferLoad.source,"%s",longUrl);
  movie_ready=-1;bufferLoadBombear();assert(strstr(loaded,longUrl));
  ligado=0;

  dtsSessao=NULL;posSeg=10;durSeg=100;bufferSeg=20;
  movie_snapshot=(DtsBufferInfo){.ready_bytes=200,.capacity_bytes=256,.total=1000};
  movie_active=1;
  assert(fabs(video_buffer_fim()-40)<.001);
  movie_snapshot.ready_bytes=950;
  assert(fabs(video_buffer_fim()-100)<.001);
  movie_snapshot.ready_bytes=0;
  assert(video_buffer_fim()==20);
  movie_snapshot.ready_bytes=200;movie_active=-1;
  assert(video_buffer_fim()==20);
  movie_active=1;dtsSessao=(DtsPlayback *)(uintptr_t)1;
  assert(video_buffer_fim()==20); // Inactive converter ignores the native proxy.
  pump_status=(DtsPlaybackStatus){.prepared=1,
    .buffer={.ready_bytes=12*1048576LL,.capacity_bytes=132*1048576LL,
             .total=100*1048576LL,.contiguous_bytes=6*1048576LL},.buffered_until=14};
  bufferSeg=0;
  assert(fabs(video_buffer_fim()-16)<.001); // Stop at a gap, not all 12 MiB ready.
  pump_status.buffered_until=25;
  assert(fabs(video_buffer_fim()-25)<.001); // Include frames already fed to LG.
  pump_status.buffer.contiguous_bytes=100*1048576LL;
  assert(video_buffer_fim()==100); // Never extend past the file duration.
  pump_status.failed=1;
  assert(video_buffer_fim()==0);
  pump_status=(DtsPlaybackStatus){0};dtsSessao=NULL;
  movie_active=1;sessao=41;ligado=0;midia[0]=0;acb=0;
  int stops=movie_stops;
  pararSessao();
  assert(movie_stops==stops+1 && !movie_active && movie_stop_session==42);
  puts("video_moviebuffer: native/DTS rail, range gaps, duration clamp, fallback and shutdown passed");
}
