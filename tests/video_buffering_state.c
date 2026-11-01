/* Exercise the actual webOS event dispatcher and converter pump. */
#define main video_acb_fixture_main
#include "video_acb_lg.c"
#undef main

static void reset_buffering(void) {
  dtsSessao=NULL;dtsModoDv=dvHabilitado=0;
  acb=0;bus=NULL;ligado=0;midia[0]=0;
  pronto=tocando=1;pausaPedida=falhou=terminou=0;
  seekEm=seekEnvEm=seekRetryEm=0;seekView=(NvSeekView){0};
  posSeg=posNativa=10;bufferSeg=30;
  bufferandoDesde=dtsRefillMs=dtsRefillEm=0;
  pump_status=(DtsPlaybackStatus){0};pump_count=pump_read=0;
  nv_recon_zerar(&recon);
}
static void event(const char *payload) { eventoPayload(payload,sessao); }
static void clock_at(int ms) {
  char payload[96];
  snprintf(payload,sizeof payload,"{\"currentTime\":{\"currentTime\":%d}}",ms);
  event(payload);
}
static void start(void) {
  event("{\"bufferingStart\":{}}");assert(video_bufferando_ms());
}
static void test_playing(void) {
  reset_buffering();tocando=0;start();
  event("{\"playing\":{}}");assert(!video_bufferando_ms());
}
static void test_late_start(void) {
  reset_buffering();event("{\"playing\":{}}");start();
  clock_at(10200);assert(video_bufferando_ms());
  Uint32 since=bufferandoDesde;
  start();assert(bufferandoDesde==since); // Repeated starts keep the original stall.
  clock_at(10400);assert(!video_bufferando_ms());
  start();clock_at(10400);clock_at(10400);
  assert(video_bufferando_ms()); // A frozen clock is a real stall.
  bufferSeg=10.4;clock_at(10600);clock_at(10800);
  assert(video_bufferando_ms()); // Residual audio with an empty buffer.
  bufferSeg=30;clock_at(11000);assert(video_bufferando_ms());
  clock_at(11200);assert(!video_bufferando_ms());
}
static void test_startup_seek(void) {
  reset_buffering();pronto=0;bufferSeg=200;start();
  seekEm=1;clock_at(10200);clock_at(10400);assert(video_bufferando_ms());
  seekEm=0;seekEnvEm=1;
  nv_seekview_begin(&seekView,120,SDL_GetTicks());
  nv_seekview_sent(&seekView,SDL_GetTicks());
  event("{\"playing\":{}}");assert(video_bufferando_ms());
  clock_at(120000);assert(video_bufferando_ms()); // Seek jump is not playback.
  assert(!seekView.active);
  event("{\"seekDone\":{}}");
  pronto=1;
  clock_at(120200);assert(video_bufferando_ms());
  clock_at(120400);assert(!video_bufferando_ms());
  start();seekRetryEm=1;clock_at(120600);clock_at(120800);
  assert(video_bufferando_ms());
  seekRetryEm=0;clock_at(10000);assert(video_bufferando_ms()); // Backward jump.
  clock_at(10200);assert(video_bufferando_ms());
  clock_at(10400);assert(!video_bufferando_ms());
}
static void test_pause_and_generation(void) {
  reset_buffering();start();pausaPedida=1;
  event("{\"playing\":{}}");clock_at(10200);clock_at(10400);
  assert(bufferandoDesde);pausaPedida=0;
  eventoPayload("{\"playing\":{}}",sessao-1);
  eventoPayload("{\"bufferingEnd\":{}}",sessao-1);
  assert(video_bufferando_ms());
  event("{\"bufferingEnd\":{}}");assert(!video_bufferando_ms());
}
static void test_managed_refill(void) {
  reset_buffering();dtsSessao=(DtsPlayback *)(uintptr_t)1;
  pump_status=(DtsPlaybackStatus){.buffering_ms=7300};
  bombearDts();assert(video_bufferando_ms()>=7300); // Refill during preroll.
  pump_status.prepared=pump_status.native_loaded=1;
  bombearDts();start();event("{\"playing\":{}}");
  assert(!bufferandoDesde && video_bufferando_ms()>=7300);
  start();clock_at(10200);clock_at(10400);
  assert(!bufferandoDesde);
  assert(video_bufferando_ms()>=7300); // Managed refill remains authoritative.
  pausaPedida=1;assert(!video_bufferando_ms());pausaPedida=0;
  pump_status.prepared=0;pump_status.buffering_ms=0;
  bombearDts();assert(!dtsRefillMs && !video_bufferando_ms());

  pump_status.prepared=1;pump_status.buffering_ms=7300;bombearDts();
  pump_status.buffering_ms=0;
  pump_events[0]="{\"bufferingStart\":{}}";pump_count=1;pump_read=0;
  bombearDts();assert(!dtsRefillMs && video_bufferando_ms());
  // Fresh native starvation queued after the old managed refill must survive.
  dtsSessao=NULL;video_parar();
  assert(!video_bufferando_ms() && !bufferandoDesde && !dtsRefillMs);
}
int main(int argc,char **argv) {
  assert(SDL_Init(SDL_INIT_TIMER)==0);SDL_Delay(2);
  if(argc>1 && !strcmp(argv[1],"refill"))test_managed_refill();
  else {
    test_playing();test_late_start();test_startup_seek();
    test_pause_and_generation();test_managed_refill();
  }
  SDL_Quit();
  puts("video_buffering_state: startup, late events, seek, pause, session and managed refill passed");
  return 0;
}
