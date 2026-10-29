/* Compile the actual webOS path; no bus, decoder or TV is contacted. */
#include <time.h>
#include <assert.h>
#include <unistd.h>
// Antes do #undef __APPLE__: gl_compat.h (via ajustes.h) escolhe o GL do Mac.
#include "ajustes.h"
#undef __APPLE__
static int fake_sleep(const struct timespec *, struct timespec *);
#define nanosleep fake_sleep
#include "../src/video.c"
#undef nanosleep

/* The official native HTTP payload reads device buffering preferences. */
int ajustes_buffer_preset(void) { return 0; }
long memlog_livre_mb(void) { return 4096; }
/* No HTTP or decoder is started by native dispatcher fixtures. */
static DtsBufferInfo movie_snapshot;
static int movie_active, movie_stops;
static unsigned movie_stop_session;
int moviebuffer_url(const char *s,const char *h,int m,char *o,size_t n) {
  (void)s;(void)h;(void)m;(void)o;(void)n;return 0;
}
int moviebuffer_info(DtsBufferInfo *out) { *out=movie_snapshot;return movie_active; }
void moviebuffer_stop(void) { movie_stops++;movie_stop_session=sessao;movie_active=0; }
static DtsPlaybackStatus pump_status;
static const char *pump_events[8];
static int pump_read, pump_count;
void dts_playback_status(DtsPlayback *p, DtsPlaybackStatus *out) { (void)p;*out=pump_status; }
int dts_playback_event(DtsPlayback *p,char *out,size_t n) { (void)p;if (pump_read==pump_count)return 0;snprintf(out,n,"%s",pump_events[pump_read++]);return 1; }
/* Unused source-changing paths remain linked by the real event dispatcher. */
void dts_playback_close(DtsPlayback *p) { (void)p;assert(0); }
void dts_playback_pause(DtsPlayback *p,int paused) { (void)p;(void)paused;assert(0); }
void mkvass_aceitar_imagem(int sim) { (void)sim; }
void dts_playback_seek(DtsPlayback *p,double t) { (void)p;(void)t;assert(0); }
void dts_playback_audio(DtsPlayback *p,int stream) { (void)p;(void)stream;assert(0); }
void dts_playback_subtitle(DtsPlayback *p,int stream) { (void)p;(void)stream;assert(0); }
int dts_overlay_draw(DtsPlayback *p,double t,float x,float y,float w,float h,int vw,int vh,float a) {
  (void)p;(void)t;(void)x;(void)y;(void)w;(void)h;(void)vw;(void)vh;(void)a;assert(0);return 0;
}
void legenda_carregar(const char *url) { (void)url;assert(0); }
int esmaecer_segura_protetor_tv(void) { return 0; }
void esmaecer_protetor_publicador(void (*publicar)(void)) { publicar(); }
char *dados_ler(const char *nome) { (void)nome; return NULL; }
int dados_gravar(const char *nome, const char *conteudo) { (void)nome; (void)conteudo; return 1; }
void marco(const char *name) { (void)name; }
const char *i18n(const char *s) { return s; }
const char *rede_url_publica(const char *url,char *dst,unsigned n) { snprintf(dst,n,"%s",url);return dst; }
static int exported_windows;
static int fake_exported_window(const char *id,SDL_Rect *src,SDL_Rect *dst) {
  assert(!strcmp(id,"webos6-window") && src->w==1920 && src->h==1080 && dst->w && dst->h);
  exported_windows++;return 1;
}
static int creates, destroys, finalizes, inits, media_calls, state_calls, windows, connects;
static int init_type, init_ok = 1, change_at_sleep, sleeps;
static long next_handle = 100;
static char bound_media[64];
static int fake_sleep(const struct timespec *a, struct timespec *b) {
  (void)a;(void)b;sleeps++;
  if (change_at_sleep && sleeps==change_at_sleep) {
    __atomic_add_fetch(&sessao,1,__ATOMIC_ACQ_REL);
    snprintf(midia,sizeof midia,"new-media");
  }
  return 0;
}
static long fake_create(void) { creates++;return next_handle++; }
static int fake_init(long h,int type,const char *app,void *cb) {
  assert(h && app && cb);inits++;init_type=type;return init_ok;
}
static void fake_destroy(long h) { assert(h);assert(!bindAtivo());destroys++; }
static int fake_finalize(long h) { assert(h);assert(!bindAtivo());finalizes++;return 1; }
static int fake_sink(long h,int type) { assert(h && type==0);return 1; }
static int fake_media(long h,const char *id) { assert(h==acb);media_calls++;snprintf(bound_media,sizeof bound_media,"%s",id);return 1; }
static int fake_state(long h,int app,int play,long *task) { assert(h==acb && app==1 && play>=1 && task);state_calls++;return 1; }
static int fake_window(long h,long x,long y,long w,long height,int full,long *task) {
  (void)x;(void)y;(void)full;assert(h==acb && w && height && task);windows++;return 1;
}
static int fake_connect(long h,int sink,long *task) { assert(h==acb && !sink && task);connects++;return 1; }
static int fake_data(long h,const char *data,long *task) { assert(h==acb && data && task);return 1; }
static AcbBind *snapshot(void) {
  AcbBind *b=malloc(sizeof *b);assert(b);b->sessao=sessao;b->acb=acb;b->tipo=acbTipoAtual;snprintf(b->midia,sizeof b->midia,"%s",midia);return b;
}
static void reset_calls(void) { media_calls=state_calls=windows=connects=sleeps=0;change_at_sleep=0; }
static int native_audio_calls;
static int fake_native_audio(LSHandle *h,const char *uri,const char *payload,
                             Filtro cb,void *ctx,unsigned long *token,void *error) {
  (void)h;(void)cb;(void)ctx;(void)token;(void)error;
  assert(!strcmp(uri,"luna://com.webos.media/selectTrack"));
  assert(strstr(payload,"\"type\":\"audio\""));native_audio_calls++;return 1;
}
static int startup_selects, startup_plays, startup_subscribes;
static const char *fake_load_payload(LSMessage *m) { (void)m;return "{\"returnValue\":true,\"mediaId\":\"live-test\"}"; }
static int fake_startup_call(LSHandle *h,const char *uri,const char *payload,
                             Filtro cb,void *ctx,unsigned long *token,void *error) {
  (void)h;(void)payload;(void)cb;(void)ctx;(void)token;(void)error;
  if (strstr(uri,"/selectTrack")) startup_selects++;
  if (strstr(uri,"/subscribe")) startup_subscribes++;
  if (strstr(uri,"/play")) {
    assert(startup_subscribes == 1);
    startup_plays++;
  }
  return 1;
}
int main(void) {
  /* #625: successful load must start playback before track discovery.
   * TS and HLS both use this callback; native default tracks need no selection. */
  lsPayload=fake_load_payload;lsCall=fake_startup_call;
  bus=(LSHandle *)(uintptr_t)1;protetorLigado=1;
  for (int mode=0;mode<=2;mode++) {
    modoLoad=mode;midia[0]=0;
    startup_selects=startup_plays=startup_subscribes=0;
    aoCarregar(NULL,NULL,(void *)(uintptr_t)sessao);
    assert(startup_selects == 0);
    assert(startup_plays == 1 && startup_subscribes == 1);
  }
  midia[0]=0;
  /* OLED A1: LG rejects DTS and returns an empty string, followed by a video
   * array. That video must never become a fake H264 audio track. */
  eventoPayload("{\"sourceInfo\":{\"programInfo\":[{\"numAudioTracks\":0,\"audioTrackInfo\":\"\",\"videoTrackInfo\":[{\"codec\":\"H264\"}],\"subtitleTrackInfo\":\"\"}]}}",sessao);
  assert(nAudio == 0 && nLeg == 0);
  MkvFaixa tracks[3] = {
    {.numero=1,.tipo=1,.codec="V_MPEG4/ISO/AVC"},
    {.numero=2,.tipo=2,.codec="A_DTS",.nome="DTS-HD Master Audio",.canais=6},
    {.numero=3,.tipo=17,.codec="S_HDMV/PGS"}
  };
  eventoPayload("{\"error\":{\"errorCode\":200,\"errorText\":\"Audio Codec Not Supported\"}}",sessao);
  assert(audioNaoSup && !video_faixa_dts(video_audio(audioAtual)));
  completarAudioMkv(tracks,3);
  assert(nAudio == 1 && audioAtual == 0 && video_faixa_dts(video_audio(0)) > 0);
  assert(audioNaoSup && video_faixa_dts(video_audio(audioAtual))); /* fallback trigger */
  assert(!strcmp(faixaAudio[0].codec,"dts") && faixaAudio[0].canais == 6);
  assert(faixaAudio[0].stream_index == -1 && faixaAudio[0].stream_id == -1);
  /* Empty arrays also stay empty; multiple hidden tracks are ambiguous. */
  eventoPayload("{\"sourceInfo\":{\"audioTrackInfo\":[],\"videoTrackInfo\":[{\"codec\":\"H264\"}],\"subtitleTrackInfo\":[]}}",sessao);
  assert(nAudio == 0 && nLeg == 0);
  tracks[2] = (MkvFaixa){.tipo=2,.codec="A_DTS"};
  completarAudioMkv(tracks,3);assert(nAudio == 0);
  /* Error 200 also covers TrueHD; it must not trigger a DTS conversion. */
  snprintf(tracks[1].codec,sizeof tracks[1].codec,"A_TRUEHD");
  snprintf(tracks[1].nome,sizeof tracks[1].nome,"TrueHD");
  completarAudioMkv(tracks,2);assert(nAudio == 1 && !video_faixa_dts(video_audio(0)));
  /* Real native audio remains authoritative; iteration stops at the array. */
  eventoPayload("{\"sourceInfo\":{\"audioTrackInfo\":[{\"codec\":\"aac\",\"channels\":2}],\"videoTrackInfo\":[{\"codec\":\"H264\"}],\"subtitleTrackInfo\":[]}}",sessao);
  assert(nAudio == 1 && !strcmp(faixaAudio[0].codec,"aac"));
  completarAudioMkv(tracks,2);assert(!strcmp(faixaAudio[0].codec,"aac"));
  nAudio = nLeg = 0; audioNaoSup = 0;

  /* Native DTS is preferred even with fallback enabled. An unsupported error
   * from another track must not follow the user onto a compatible DTS track. */
  ligado=1;snprintf(midia,sizeof midia,"native-media");nAudio=2;audioAtual=0;
  snprintf(faixaAudio[0].codec,sizeof faixaAudio[0].codec,"eac3");
  snprintf(faixaAudio[1].codec,sizeof faixaAudio[1].codec,"dts");
  faixaAudio[0].numero=0;faixaAudio[1].numero=1;dtsHabilitado=1;
  lsCall=fake_native_audio;
  eventoPayload("{\"errorCode\":200,\"errorText\":\"Audio Codec Not Supported\"}",sessao);
  assert(audioNaoSup && !dtsSessao);
  video_escolher_audio(0);assert(audioNaoSup); /* same rejected track */
  video_escolher_audio(1);
  assert(audioAtual==1 && !audioNaoSup && !dtsSessao && native_audio_calls==2);
  eventoPayload("{\"playing\":{},\"currentTime\":1000,\"errorCode\":0,\"errorText\":\"No Error\"}",sessao);
  assert(video_tocando() && !audioNaoSup && !dtsSessao);
  eventoPayload("{\"errorCode\":200,\"errorText\":\"Audio Codec Not Supported\"}",sessao);
  assert(audioNaoSup); /* a fresh DTS failure still permits fallback */
  audioNaoSup=0;dtsHabilitado=0;ligado=0;nAudio=0;midia[0]=0;posSeg=0;tocando=0;lsCall=NULL;

  /* Source stages have no detail field. Logging them must not read or print
   * uninitialized stack data, as seen in the full TV startup log. */
  FILE *stage_log=tmpfile();assert(stage_log);
  fflush(stdout);int saved_stdout=dup(fileno(stdout));assert(saved_stdout>=0);
  assert(dup2(fileno(stage_log),fileno(stdout))>=0);
  assert(logDtsStage("{\"dtsStage\":{\"name\":\"worker-rate\",\"detail\":\"ready=1\"}}"));
  assert(logDtsStage("{\"dtsStage\":{\"name\":\"source-open-requested\"}}"));
  assert(logDtsStage("{\"dtsStage\":{\"name\":\"source-opened\"}}"));
  fflush(stdout);rewind(stage_log);char stage_text[256]={0};
  assert(fread(stage_text,1,sizeof stage_text-1,stage_log)>0);
  assert(dup2(saved_stdout,fileno(stdout))>=0);close(saved_stdout);fclose(stage_log);
  assert(!strcmp(stage_text,"[dts] stage=worker-rate ready=1\n[dts] stage=source-open-requested \n[dts] stage=source-opened \n"));
  acbCriar=fake_create;acbIniciar=fake_init;acbDestruir=fake_destroy;acbFinalizar=fake_finalize;
  acbSink=fake_sink;acbMidia=fake_media;acbEstado=fake_state;acbJanela=fake_window;acbConectar=fake_connect;
  acbVideoData=acbAudioData=fake_data;
  assert(acbConfigurarTipo(dtsSessao != NULL) && init_type==0 && creates==1);long original=acb;
  dtsSessao=(DtsPlayback *)(uintptr_t)1;bindVivo=1;
  assert(!acbConfigurarTipo(dtsSessao != NULL) && acb==original && creates==1 && destroys==0);
  bindVivo=0;assert(acbConfigurarTipo(dtsSessao != NULL) && init_type==10 && creates==2 && finalizes==1 && destroys==1);
  assert(acbConfigurarTipo(dtsSessao != NULL) && creates==2); // stable playback must not recreate each frame
  dtsSessao=NULL;assert(acbConfigurarTipo(dtsSessao != NULL) && init_type==0 && creates==3);
  tipoJogadorManual=1;tipoJogador=4;dtsSessao=(DtsPlayback *)(uintptr_t)1;
  assert(acbConfigurarTipo(dtsSessao != NULL) && init_type==4);dtsSessao=NULL;assert(acbConfigurarTipo(dtsSessao != NULL) && init_type==4);
  tipoJogadorManual=0;tipoJogador=0;assert(acbConfigurarTipo(dtsSessao != NULL));
  init_ok=0;dtsSessao=(DtsPlayback *)(uintptr_t)1;
  assert(!acbConfigurarTipo(dtsSessao != NULL) && !acb);int before=creates;
  assert(!acbConfigurarTipo(dtsSessao != NULL) && creates==before); // no 60Hz failure/recreation loop
  init_ok=1;dtsSessao=NULL;assert(acbConfigurarTipo(dtsSessao != NULL));

  sessao=7;snprintf(midia,sizeof midia,"old-media");AcbBind *b=snapshot();
  sessao=8;snprintf(midia,sizeof midia,"new-media");bindVivo=1;prenderPlano(b);
  assert(!bindAtivo() && !media_calls && !state_calls && !windows && !connects);
  // A new native ID can also appear within the same source session.
  reset_calls();b=snapshot();snprintf(midia,sizeof midia,"replacement-id");bindVivo=1;prenderPlano(b);
  assert(!media_calls && !state_calls);
  // Changing generation during a sleep must stop before the next native stage.
  reset_calls();b=snapshot();change_at_sleep=1;bindVivo=1;prenderPlano(b);
  assert(media_calls==1 && !state_calls && !connects && !windows);
  // A current snapshot binds normally and retains the captured identity.
  reset_calls();b=snapshot();bindVivo=1;prenderPlano(b);
  assert(media_calls==1 && !strcmp(bound_media,"new-media") && state_calls==2 && connects==1 && windows==1);
  assert(!bindAtivo());

  /* loadCompleted may be drained in a pump which sampled prepared=false.
   * The next pump resets readiness for revision 1; there is no second event.
   * The durable native-loaded snapshot must still schedule the video bind. */
  DtsPlaybackStatus st = {.prepared=1, .revision=1, .native_loaded=1};
  snprintf(st.media_id,sizeof st.media_id,"dts-media");
  snprintf(midia,sizeof midia,"dts-media");
  pronto=0;bindPendente=0;
  sincronizarPlanoDts(&st);
  assert(pronto && bindPendente);
  reset_calls();b=snapshot();bindPendente=0;bindVivo=1;prenderPlano(b);
  assert(media_calls==1 && connects==1 && windows==1);
  sincronizarPlanoDts(&st);
  assert(pronto && !bindPendente); // stable pumps do not continually rebind

  /* Late native ID, after load readiness, must also bind exactly once. */
  midia[0]=st.media_id[0]=0;pronto=0;bindPendente=0;
  sincronizarPlanoDts(&st);assert(pronto && !bindPendente);
  snprintf(st.media_id,sizeof st.media_id,"late-dts-media");
  sincronizarPlanoDts(&st);assert(pronto && bindPendente);
  assert(!strcmp(midia,"late-dts-media"));

  /* A replacement Load must not bind until its own completion is observed. */
  st.native_loaded=0;
  snprintf(st.media_id,sizeof st.media_id,"replacement-dts-media");
  sincronizarPlanoDts(&st);assert(!pronto && !bindPendente);
  st.native_loaded=1;
  sincronizarPlanoDts(&st);assert(pronto && bindPendente);
  /* webOS 6 uses an exported window without ACB. Native callbacks may all
   * arrive while Load is executing, before prepared metadata is published. */
  acb=0;snprintf(expWin,sizeof expWin,"webos6-window");sdlExpJanela=fake_exported_window;
  dtsSessao=(DtsPlayback *)(uintptr_t)1;dtsRevisao=0;dtsLegAntes=-1;
  pronto=tocando=viuVideo=0;midia[0]=0;bindPendente=0;
  memset(&pump_status,0,sizeof pump_status);
  pump_events[0]="{\"videoInfo\":{\"width\":1920,\"height\":1080,\"hdrType\":\"HDR10\"}}";
  pump_events[1]="{\"loadCompleted\":{}}";
  pump_events[2]="{\"playing\":{}}";
  pump_count=3;pump_read=0;
  bombearDts();assert(pump_read==0 && !video_pronto() && !video_decoder_anunciou());
  pump_status.prepared=pump_status.native_loaded=pump_status.revision=1;
  pump_status.info.width=1920;pump_status.info.height=1080;
  snprintf(pump_status.media_id,sizeof pump_status.media_id,"exported-dts-media");
  bombearDts();
  assert(pump_read==3 && video_pronto() && video_tocando() && video_decoder_anunciou());
  assert(!strcmp(vidHdr,"HDR10") && exported_windows>0 && !bindPendente);
  bombearDts();
  assert(video_pronto() && video_tocando() && video_decoder_anunciou());
  assert(!strcmp(vidHdr,"HDR10"));
  /* A stall recovery publishes a new native generation in the same source
   * session. Its early events must survive the metadata reset too. */
  pump_status.prepared=pump_status.native_loaded=0;
  pump_status.media_id[0]=0;pump_read=0;
  int old_windows=exported_windows;
  bombearDts();assert(pump_read==0 && exported_windows==old_windows);
  pump_status.prepared=pump_status.native_loaded=1;pump_status.revision=2;
  snprintf(pump_status.media_id,sizeof pump_status.media_id,"recovered-dts-media");
  bombearDts();
  assert(pump_read==3 && dtsRevisao==2 && !strcmp(midia,"recovered-dts-media"));
  assert(video_pronto() && video_tocando() && video_decoder_anunciou());
  assert(!strcmp(vidHdr,"HDR10") && exported_windows>old_windows);
  old_windows=exported_windows;bombearDts();assert(exported_windows==old_windows);
  puts("webOS ACB dispatch snapshot, stale generations, deferred VIDEO/MSE transitions, overrides and failed-init retry, early webOS 6 exported-window events and replacement pipeline tests passed");
  return 0;
}
