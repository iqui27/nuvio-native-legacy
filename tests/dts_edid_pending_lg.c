/* Real webOS video.c path. Host fakes only libhelpers dlopen and the clock. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <dlfcn.h>
#include <SDL2/SDL.h>
#include "video.h"
#include "dts/dts_playback.h"
#include "dts/dts_engine.h"
#include "dts/dts_overlay.h"

static Uint32 edid_now = 1000;
Uint32 edid_test_ticks(void) { return edid_now; }

static int edid_dlopen_ok = 1;
static int edid_sym_ok = 1;
static void *edid_hl_sym;
static int edid_original;

void *edid_dlopen(const char *name, int mode) {
  if (name && strcmp(name, "libhelpers.so.2") == 0)
    return edid_dlopen_ok ? (void *)(uintptr_t)1 : NULL;
  return dlopen(name, mode);
}
void *edid_dlsym(void *h, const char *sym) {
  if (h == (void *)(uintptr_t)1 && sym && strcmp(sym, "HLunaServiceCall") == 0)
    return edid_sym_ok ? edid_hl_sym : NULL;
  return dlsym(h, sym);
}
int edid_ajustes_original(void) { return edid_original; }

#define SDL_GetTicks edid_test_ticks
#define dlopen edid_dlopen
#define dlsym edid_dlsym
#define ajustes_dts_original edid_ajustes_original
#ifndef NV_VIDEO_C
#define NV_VIDEO_C "../src/video.c"
#endif
#include NV_VIDEO_C
#undef SDL_GetTicks
#undef dlopen
#undef dlsym
#undef ajustes_dts_original

static int fails;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; return fails; } } while (0)

static const char *positive =
  "{\"configs\":{\"tv.model.edidType\":\"TrueHD+dts\"},\"returnValue\":true}";
static const char *negative =
  "{\"configs\":{\"tv.model.edidType\":\"TrueHD\"},\"returnValue\":true}";

static int hl_return = 1;
static int hl_sync;
static const char *hl_sync_reply;
typedef struct { NvHContext *ctx; } Held;
static Held held[8];
static int nheld;

static int fake_hl(const char *uri, const char *payload, NvHContext *ctx) {
  (void)payload;
  if (!uri || !strstr(uri, "com.webos.service.config/getConfigs")) return 0;
  if (nheld < 8) held[nheld++].ctx = ctx;
  if (hl_sync && ctx->cb) ctx->cb(NULL, (LSMessage *)hl_sync_reply, ctx->ud);
  return hl_return;
}
static void fire(int i, const char *payload) {
  NvHContext *ctx = held[i].ctx;
  ctx->cb(NULL, (LSMessage *)payload, ctx->ud);
}
static const char *fake_payload(LSMessage *m) { return (const char *)m; }
static int fake_call(LSHandle *h, const char *uri, const char *payload, Filtro cb,
                     void *ctx, unsigned long *tok, void *err) {
  (void)h; (void)uri; (void)payload; (void)cb; (void)ctx; (void)tok; (void)err;
  return 1;
}

static DtsPlayback *dts_started;
DtsPlayback *dts_playback_start(const char *url, const char *headers, int audio_stream,
                                double position, int paused, const char *window,
                                int webos_major, const DtsTrack *selected, int ordinal,
                                int count) {
  (void)url; (void)headers; (void)audio_stream; (void)position; (void)paused;
  (void)window; (void)webos_major; (void)selected; (void)ordinal; (void)count;
  dts_started = (DtsPlayback *)(uintptr_t)1;
  return dts_started;
}
void dts_playback_close(DtsPlayback *p) { (void)p; dts_started = NULL; }
void dts_playback_status(DtsPlayback *p, DtsPlaybackStatus *out) {
  (void)p; memset(out, 0, sizeof *out);
}
int dts_playback_event(DtsPlayback *p, char *out, size_t n) {
  (void)p; (void)out; (void)n; return 0;
}
void dts_playback_pause(DtsPlayback *p, int paused) { (void)p; (void)paused; }
void dts_playback_seek(DtsPlayback *p, double t) { (void)p; (void)t; }
void dts_playback_audio(DtsPlayback *p, int stream) { (void)p; (void)stream; }
void dts_playback_subtitle(DtsPlayback *p, int stream) { (void)p; (void)stream; }
int dts_overlay_draw(DtsPlayback *p, double t, float x, float y, float w, float h,
                     int vw, int vh, float a) {
  (void)p; (void)t; (void)x; (void)y; (void)w; (void)h; (void)vw; (void)vh; (void)a;
  return 0;
}
int dts_playback_enabled(void) { return 1; }
void dts_playback_next_dv(int enabled) { (void)enabled; }
void dts_playback_next_p7(int enabled) { (void)enabled; }
void dts_engine_set_ac3(int on) { (void)on; }
void dts_engine_set_ahead_mib(int mib) { (void)mib; }
int ajustes_dts_ac3(void) { return 1; }
int ajustes_dv_mkv(void) { return 0; }
int ajustes_dv7(void) { return 0; }
int cacheboost_backend_buffer_mb(void) { return 0; }
int esmaecer_segura_protetor_tv(void) { return 0; }
void marco(const char *name) { (void)name; }
char *dados_ler(const char *nome) { (void)nome; return NULL; }
int dados_gravar(const char *nome, const char *conteudo) {
  (void)nome; (void)conteudo; return 1;
}
const char *i18n(const char *s) { return s ? s : ""; }
void esmaecer_protetor_publicador(void (*publicar)(void)) { (void)publicar; }
int mkvass_cabecalho(const char *url, unsigned char **buf, long *n) {
  (void)url; (void)buf; (void)n; return 0;
}
int ajustes_buffer_preset(void) { return 0; }
long memlog_livre_mb(void) { return -1; }
int moviebuffer_url(const char *source, const char *headers, int mib, char *out, size_t n) {
  (void)source; (void)headers; (void)mib; (void)out; (void)n; return 0;
}
void moviebuffer_stop(void) {}
const char *rede_url_publica(const char *url, char *dst, unsigned tam) {
  (void)dst; (void)tam; return url;
}
void vel_log(int cent, const char *estado) { (void)cent; (void)estado; }
void legenda_carregar(const char *url) { (void)url; }
void capmkv_aplicar(const MkvCap *caps, int n) { (void)caps; (void)n; }
int faixasmkv_acrescentar_imagem(VideoFaixa *leg, int nLeg, int max, const MkvFaixa *fx, int n) {
  (void)leg; (void)nLeg; (void)max; (void)fx; (void)n; return nLeg;
}
int faixasmkv_acrescentar_texto(VideoFaixa *leg, int nLeg, int max, const MkvFaixa *fx, int n) {
  (void)leg; (void)nLeg; (void)max; (void)fx; (void)n; return nLeg;
}
int faixasmkv_idiomas_coerentes(const VideoFaixa *leg, int nLeg, const MkvFaixa *fx, const int *idx) {
  (void)leg; (void)nLeg; (void)fx; (void)idx; return 0;
}
int faixasmkv_legendas_por_idioma(const VideoFaixa *leg, int nLeg, const MkvFaixa *fx, int n, int *mapa) {
  (void)leg; (void)nLeg; (void)fx; (void)n; (void)mapa; return 0;
}
int mkv_capitulos_alem(const char *url, const unsigned char *cab, long cabN,
                       MkvCap *caps, int maxCaps, int *status) {
  (void)url; (void)cab; (void)cabN; (void)caps; (void)maxCaps; (void)status; return 0;
}
int mkv_casar_legendas(const MkvFaixa *fx, int n, const int *tvNum, int nTv, int *idxFx) {
  (void)fx; (void)n; (void)tvNum; (void)nTv; (void)idxFx; return 0;
}
int mkv_contentor_definitivo(int c) { (void)c; return 0; }
const char *mkv_contentor_nome(int c) { (void)c; return ""; }
double mkv_creditos_nomeados(const MkvCap *caps, int n) { (void)caps; (void)n; return 0; }
double mkv_creditos_ultimo(const MkvCap *caps, int n) { (void)caps; (void)n; return 0; }
const MkvDiag *mkv_diag(void) { return NULL; }
int mkv_faixas_do_trecho(const unsigned char *buf, long n, MkvFaixa *saida, int max,
                         MkvCap *caps, int maxCaps, int *nCaps) {
  (void)buf; (void)n; (void)saida; (void)max; (void)caps; (void)maxCaps; (void)nCaps; return 0;
}
int mkv_faixas_e_caps(const char *url, MkvFaixa *saida, int max,
                      MkvCap *caps, int maxCaps, int *nCaps) {
  (void)url; (void)saida; (void)max; (void)caps; (void)maxCaps; (void)nCaps; return 0;
}
int mkv_rpu_alem(const char *url, int nalTam, long teto) {
  (void)url; (void)nalTam; (void)teto; return 0;
}
int mkv_tracks_alem(const char *url, MkvFaixa *saida, int max, int *status) {
  (void)url; (void)saida; (void)max; (void)status; return 0;
}
int mkv_ultimo_contentor(void) { return 0; }
void mkvass_aceitar_imagem(int sim) { (void)sim; }

static void fonte(const char *codec) {
  ligado = pronto = 1; dtsSessao = NULL; dts_started = NULL; dtsTentou = 0;
  dvHabilitado = 0; dtsNativePending = 0; dtsEstado = 0; audioNaoSup = 0;
  dtsHabilitado = 1; modoLoad = 0; edid_original = 0; nAudio = 1; audioAtual = 0;
  nheld = 0; hl_sync = 0; hl_return = 1; hl_sync_reply = NULL;
  snprintf(urlAtual, sizeof urlAtual, "http://fixture/a.mkv");
  memset(&faixaAudio[0], 0, sizeof faixaAudio[0]);
  snprintf(faixaAudio[0].codec, sizeof faixaAudio[0].codec, "%s", codec);
  faixaAudio[0].canais = 6;
}
static void pump(void) { video_bombear(); }

int main(void) {
  edid_hl_sym = (void *)fake_hl;
  lsCall = fake_call;
  lsPayload = fake_payload;
  bus = (LSHandle *)(uintptr_t)1;
#ifndef NV_EDID_ORIGINAL
  nv_dts_edid_test_webos_major = 26;
#else
  snprintf(expWin, sizeof expWin, "webos6-window");
#endif

  fonte("dts");
  edid_dlopen_ok = 0; edid_sym_ok = 0;
  pedirEdid();
  pump();
  CHECK(__atomic_load_n(&edidDts, __ATOMIC_ACQUIRE) == 0);
  CHECK(dtsTentou && dtsSessao == dts_started);
  CHECK(nheld == 0);

  fonte("dts");
  edid_dlopen_ok = 1; edid_sym_ok = 0;
  pedirEdid();
  pump();
  CHECK(__atomic_load_n(&edidDts, __ATOMIC_ACQUIRE) == 0);
  CHECK(dtsTentou && nheld == 0);

  fonte("dts");
  lsPayload = NULL;
  pedirEdid();
  CHECK(__atomic_load_n(&edidDts, __ATOMIC_ACQUIRE) == 0);
  CHECK(nheld == 0);
  lsPayload = fake_payload;

  /* Reply is held until after the pump. Original stores 0 and converts here. */
  fonte("dts");
  edid_sym_ok = 1;
  edid_now = 1000;
  pedirEdid();
  pump();
  CHECK(!dtsTentou && !dtsSessao);
  CHECK(__atomic_load_n(&edidDts, __ATOMIC_ACQUIRE) == -1);
  CHECK(nheld == 1);
  fire(0, positive);
  pump();
  CHECK(__atomic_load_n(&edidDts, __ATOMIC_ACQUIRE) == 1);
  CHECK(!dtsTentou && !dtsSessao);

  fonte("dts");
  hl_return = 0;
  edid_now = 1000;
  pedirEdid();
  pump();
  CHECK(__atomic_load_n(&edidDts, __ATOMIC_ACQUIRE) == -1);
  CHECK(!dtsTentou);

  fonte("dts");
  hl_return = 0;
  hl_sync = 1;
  hl_sync_reply = positive;
  pedirEdid();
  CHECK(__atomic_load_n(&edidDts, __ATOMIC_ACQUIRE) == 1);
  pump();
  CHECK(!dtsTentou);

  fonte("dts");
  edid_now = 1000;
  pedirEdid();
  pump();
  CHECK(!dtsTentou);
  edid_now = 2999;
  pump();
  CHECK(!dtsTentou && __atomic_load_n(&edidDts, __ATOMIC_ACQUIRE) == -1);
  edid_now = 3000;
  pump();
  CHECK(dtsTentou && dtsSessao == dts_started);

  fonte("dts");
  edid_now = 0xfffffff0u;
  pedirEdid();
  pump();
  CHECK(!dtsTentou);
  edid_now = 0xfffffff0u + 1999u;
  pump();
  CHECK(!dtsTentou);
  edid_now = 0xfffffff0u + 2000u;
  pump();
  CHECK(dtsTentou);

  fonte("dts");
  edid_now = 0;
  pedirEdid();
  pump();
  CHECK(!dtsTentou);
  edid_now = 1999;
  pump();
  CHECK(!dtsTentou);
  edid_now = 2000;
  pump();
  CHECK(dtsTentou);

  fonte("dts");
  edid_now = 1000;
  pedirEdid();
  fire(0, negative);
  pump();
  CHECK(__atomic_load_n(&edidDts, __ATOMIC_ACQUIRE) == 0);
  CHECK(dtsTentou);

  fonte("dts");
  pedirEdid();
  fire(0, "{");
  pump();
  CHECK(__atomic_load_n(&edidDts, __ATOMIC_ACQUIRE) == 0);
  CHECK(dtsTentou);

  fonte("dts");
  pedirEdid();
  fire(0, "{\"errorText\":\"Not permitted to send to com.webos.service.config\"}");
  pump();
  CHECK(__atomic_load_n(&edidDts, __ATOMIC_ACQUIRE) == 0);
  CHECK(dtsTentou);

  fonte("dts");
  edid_original = 1;
  edid_now = 1000;
  pedirEdid();
  pump();
  CHECK(__atomic_load_n(&edidDts, __ATOMIC_ACQUIRE) == -1);
  CHECK(!dtsTentou);

  fonte("dts");
  audioNaoSup = 1;
  edid_now = 1000;
  pedirEdid();
  pump();
  CHECK(__atomic_load_n(&edidDts, __ATOMIC_ACQUIRE) == -1);
  CHECK(dtsTentou && dtsSessao == dts_started);

  fonte("truehd");
  edid_now = 1000;
  pedirEdid();
  fire(0, negative);
  pump();
  CHECK(__atomic_load_n(&edidDts, __ATOMIC_ACQUIRE) == 0);
  CHECK(!dtsTentou && !dtsSessao);

  fonte("dts");
  edid_now = 1000;
  pedirEdid();
  pedirEdid();
  fire(0, positive);
  CHECK(__atomic_load_n(&edidDts, __ATOMIC_ACQUIRE) == -1);
  fire(1, positive);
  CHECK(__atomic_load_n(&edidDts, __ATOMIC_ACQUIRE) == 1);
  fire(0, negative);
  CHECK(__atomic_load_n(&edidDts, __ATOMIC_ACQUIRE) == 1);
  pump();
  CHECK(!dtsTentou);

#ifndef NV_EDID_ORIGINAL
  fonte("dts");
  nv_dts_edid_test_webos_major = 4;
  edid_now = 1000;
  pedirEdid();
  fire(0, negative);
  pump();
  CHECK(__atomic_load_n(&edidDts, __ATOMIC_ACQUIRE) == 0);
  CHECK(!dtsTentou);

  fonte("dts");
  nv_dts_edid_test_webos_major = 4;
  dtsHabilitado = 0;
  pedirEdid();
  fire(0, negative);
  pump();
  CHECK(dtsEstado == 0);
  CHECK(!dtsTentou);
  nv_dts_edid_test_webos_major = 26;
#endif

  puts("dts_edid_pending_lg ok");
  return 0;
}
