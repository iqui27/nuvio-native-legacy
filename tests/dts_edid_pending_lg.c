// Compile the real webOS DTS path and fake only the EDID helper callback.
#define NV_DTS_EDID_PENDING_TEST 1
#include "video.h"
#include "video_escala.h"
#include "video_reconexao.h"
#include "idioma.h"
#include "linguas.h"
#include "marco.h"
#include "mkv.h"
#include "mkvass.h"
#include "js.h"
#include "lsregistro.h"
#include "rede.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <dlfcn.h>
#include <assert.h>
#include "ajustes.h"
#include "legenda.h"
#include "dados.h"
#include "dts/dts_playback.h"
#include "dts/dts_engine.h"
#include "dts/dts_overlay.h"

static int hlCalls;
static const char *hlReply;
static int hlDelayMs;
static int hlAbsent;
static DtsPlayback *dtsStarted;

#define dts_playback_start dubleDtsStart
#define dts_playback_close dubleDtsClose
#define dts_playback_status dubleDtsStatus
#define dts_playback_event dubleDtsEvent
#define dts_playback_pause dubleDtsPause
#define dts_playback_seek dubleDtsSeek
#define dts_playback_audio dubleDtsAudio
#define dts_playback_subtitle dubleDtsSubtitle
#define dts_overlay_draw dubleDtsOverlay
#define dts_playback_enabled dubleDtsEnabled
#define ajustes_dts_original dubleDtsOriginal
static DtsPlayback *dubleDtsStart(const char *url, const char *headers, int audio_stream,
                                  double position, int paused, const char *window,
                                  int webos_major, const DtsTrack *selected, int ordinal,
                                  int count) {
  (void)url; (void)headers; (void)audio_stream; (void)position; (void)paused;
  (void)window; (void)webos_major; (void)selected; (void)ordinal; (void)count;
  dtsStarted = (DtsPlayback *)(uintptr_t)0x1;
  return dtsStarted;
}
static void dubleDtsClose(DtsPlayback *p) { (void)p; dtsStarted = NULL; }
static void dubleDtsStatus(DtsPlayback *p, DtsPlaybackStatus *out) { (void)p; memset(out, 0, sizeof *out); }
static int dubleDtsEvent(DtsPlayback *p,char *out,size_t n) { (void)p;(void)out;(void)n; return 0; }
static void dubleDtsPause(DtsPlayback *p,int paused) { (void)p;(void)paused; }
static void dubleDtsSeek(DtsPlayback *p,double t) { (void)p;(void)t; }
static void dubleDtsAudio(DtsPlayback *p,int stream) { (void)p;(void)stream; }
static void dubleDtsSubtitle(DtsPlayback *p,int stream) { (void)p;(void)stream; }
static int dubleDtsOverlay(DtsPlayback *p,double t,float x,float y,float w,float h,int vw,int vh,float a) {
  (void)p;(void)t;(void)x;(void)y;(void)w;(void)h;(void)vw;(void)vh;(void)a; return 0;
}
static int dubleDtsEnabled(void) { return 1; }
static int dubleDtsOriginal(void) { return 0; }

#undef __APPLE__
#include "../src/video.c"

extern int nv_dts_edid_test_webos_major;

static int webosMajor = 26;
static int dtsTvLiberada = 1;
static int fakeHl(const char *uri, const char *carga, NvHContext *ctx) {
  (void)uri; (void)carga;
  hlCalls++;
  if (hlAbsent) return 0;
  if (hlDelayMs >= 0 && hlReply) {
    usleep((useconds_t)hlDelayMs * 1000u);
    ctx->cb(NULL, (LSMessage *)hlReply, ctx->ud);
  }
  return 1;
}
static int fakeCall(LSHandle *h, const char *uri, const char *carga, Filtro cb,
                    void *ctx, unsigned long *tok, void *erro) {
  (void)h; (void)uri; (void)carga; (void)cb; (void)ctx; (void)tok; (void)erro;
  return 1;
}
static const char *fakePayload(LSMessage *m) { return (const char *)m; }

static void fonte_dts(void) {
  ligado = pronto = 1; dtsSessao = NULL; dtsStarted = NULL; dtsTentou = 0; dvHabilitado = 0;
  bindPendente = 0; __atomic_store_n(&bindVivo, 0, __ATOMIC_RELEASE);
  nAudio = 1; audioAtual = 0; audioNaoSup = 0; dtsHabilitado = 1;
  edidEsperaMs = 2000;
  snprintf(urlAtual, sizeof urlAtual, "http://fixture/dts.mkv");
  snprintf(faixaAudio[0].codec, sizeof faixaAudio[0].codec, "dts");
  faixaAudio[0].canais = 6;
}
static void rodar_ticks(int ms) {
  for (int i = 0; i < ms; ++i) video_bombear();
}
static void ate_edid_resolver(void) {
  int limit = edidEsperaMs + 500;
  for (int t = 0; t < limit; ++t) {
    int edid = __atomic_load_n(&edidDts, __ATOMIC_ACQUIRE);
    if (edid != -1) {
      video_bombear();
      return;
    }
    if (nv_dts_native_agora(-1, 0, SDL_GetTicks(), edidPedidoMs, edidEsperaMs) == 0) {
      video_bombear();
      return;
    }
    video_bombear();
  }
}

int main(void) {
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  nv_dts_edid_test_webos_major = 26;
  lsCall = fakeCall; lsPayload = fakePayload;
  bus = (LSHandle *)(uintptr_t)1;
  pedirEdidHl = fakeHl;
  snprintf(expWin, sizeof expWin, "webos6-window");
  { FILE *f = fopen("/tmp/nuvio-dts-liberar", "w"); assert(f); fclose(f); }

  fonte_dts();
  pedirEdid();
  assert(__atomic_load_n(&edidDts, __ATOMIC_ACQUIRE) == -1);
  video_bombear();
  assert(!dtsTentou && !dtsSessao);

  fonte_dts();
  hlAbsent = 0;
  hlReply = "{\"configs\":{\"tv.model.edidType\":\"TrueHD+dts\"}}";
  hlDelayMs = 5;
  edidEsperaMs = 50;
  pedirEdid();
  rodar_ticks(3);
  assert(!dtsTentou && !dtsSessao);
  while (__atomic_load_n(&edidDts, __ATOMIC_ACQUIRE) == -1 &&
         nv_dts_native_agora(-1, 0, SDL_GetTicks(), edidPedidoMs, edidEsperaMs) == -1)
    video_bombear();
  assert(__atomic_load_n(&edidDts, __ATOMIC_ACQUIRE) == 1);
  assert(!dtsTentou && !dtsSessao);

  fonte_dts();
  hlAbsent = 0;
  hlReply = "{\"configs\":{\"tv.model.edidType\":\"TrueHD\"}}";
  hlDelayMs = 0;
  edidEsperaMs = 2000;
  pedirEdid();
  ate_edid_resolver();
  assert(__atomic_load_n(&edidDts, __ATOMIC_ACQUIRE) == 0);
  assert(dtsTentou && dtsSessao == dtsStarted);
  dtsSessao = NULL; dtsStarted = NULL;

  fonte_dts();
  hlAbsent = 1;
  hlReply = NULL;
  hlDelayMs = -1;
  pedirEdid();
  assert(__atomic_load_n(&edidDts, __ATOMIC_ACQUIRE) == 0);
  video_bombear();
  assert(dtsTentou && dtsSessao == dtsStarted);
  dtsSessao = NULL; dtsStarted = NULL;

  puts("dts_edid_pending_lg: pending EDID never commits early; late/negative fall back");
  return 0;
}
