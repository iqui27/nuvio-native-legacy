// F07 over the REAL Android backend (src/video_android.c) with a fake JNI:
// the player arms the next open with its cache limit, the limit crosses JNI
// only when it changes and BEFORE the open, a trailer (never armed) goes out
// with the cache off and at 100%, a player session re-applies its volume
// after each Kotlin open (source change / reconnection), the gain is not
// re-sent when unchanged, and the Kotlin reports land in cacheboost.
#define NV_ANDROID 1
#include <SDL2/SDL.h>
void *SDL_AndroidGetJNIEnv(void);
void *SDL_AndroidGetActivity(void);
#include "../src/video_android.c"
// capmkv.c (capitulos do MKV) nao faz parte deste teste.
int juntos_precarregando(void) { return 0; }
int stream_kbps_atual(void) { return 0; }
int xtream_url_limitada(const char *u) { (void)u; return 0; }
int xtream_conexoes_limitadas(void) { return 0; }
int xtream_conta_ler(XtreamConta *c) { memset(c, 0, sizeof *c); return 0; }
void capmkv_iniciar(const char *u) { (void)u; }
void capmkv_zerar(void) {}
double capmkv_creditos(double d) { (void)d; return 0.0; }
#include "../src/cacheboost.c"
#include "../src/audsync.c"   // F06 nativeAudioEstado feeds the boost state too
#include "../src/velocidade.c" // #202 speed
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

static struct JNINativeInterface_ jni;
static const struct JNINativeInterface_ *env = &jni;
static unsigned relogio = 100;
static int semEnv;
void *SDL_AndroidGetJNIEnv(void) { return semEnv ? NULL : &env; }
void *SDL_AndroidGetActivity(void) { return NULL; }
Uint32 SDL_GetTicks(void) { return relogio; }
Uint64 SDL_GetTicks64(void) { return relogio; }
SDL_mutex *SDL_CreateMutex(void) { return (SDL_mutex *)(uintptr_t)1; }
void marco(const char *s) { (void)s; }
const char *ling_audio(void) { return ""; }
static int pressao = -1;   // last tex_pressao_reproducao() value
void tex_pressao_reproducao(int l) { pressao = l; }

// Call log: 'A' open, 'C<n>' cache(n), 'G<n>' ganho(n), 'V<n>' velocidade(n), 'P' stop,
// 'T<id>;' titulo(id) (warm resume #492), 'D' descartarGuardado().
static char registro[512];
static void anota(const char *s) { strncat(registro, s, sizeof registro - strlen(registro) - 1); }
static int semCache;

static jobject JNICALL global(JNIEnv *e, jobject o) { (void)e; return o; }
static jmethodID JNICALL metodo(JNIEnv *e, jclass c, const char *nome, const char *sig) {
  (void)e; (void)c; (void)sig;
  if (!strcmp(nome, "abrir")) return (jmethodID)(uintptr_t)10;
  if (!strcmp(nome, "abrirPosicao")) return (jmethodID)(uintptr_t)11;
  if (!strcmp(nome, "parar")) return (jmethodID)(uintptr_t)12;
  if (!strcmp(nome, "cache")) return semCache ? NULL : (jmethodID)(uintptr_t)13;
  if (!strcmp(nome, "ganho")) return semCache ? NULL : (jmethodID)(uintptr_t)14;
  if (!strcmp(nome, "buffer")) return semCache ? NULL : (jmethodID)(uintptr_t)16;
  if (!strcmp(nome, "cacheTitle")) return semCache ? NULL : (jmethodID)(uintptr_t)17;
  if (!strcmp(nome, "descartarGuardado")) return semCache ? NULL : (jmethodID)(uintptr_t)18;
  if (!strcmp(nome, "velocidade")) return semCache ? NULL : (jmethodID)(uintptr_t)15;
  return (jmethodID)(uintptr_t)1;
}
// The fake jstring is the ASCII text itself, so the call log can print it.
static jstring JNICALL texto(JNIEnv *e, const jchar *u, jsize n) {
  char *t = malloc((size_t)n + 1);
  (void)e;
  for (jsize i = 0; i < n; i++) t[i] = (char)u[i];
  t[n] = 0;
  return (jstring)t;
}
static void JNICALL apagar(JNIEnv *e, jobject o) { (void)e; free(o); }
static jboolean JNICALL temExcecao(JNIEnv *e) { (void)e; return 0; }
static void JNICALL limparExcecao(JNIEnv *e) { (void)e; }
static void JNICALL chamar(JNIEnv *e, jclass c, jmethodID m, ...) {
  char b[24];
  va_list ap;
  (void)e; (void)c;
  va_start(ap, m);
  switch ((uintptr_t)m) {
    case 10: case 11: anota("A"); break;
    case 12: anota("P"); break;
    case 13: snprintf(b, sizeof b, "C%d", va_arg(ap, jint)); anota(b); break;
    case 14: snprintf(b, sizeof b, "G%d", va_arg(ap, jint)); anota(b); break;
    case 15: snprintf(b, sizeof b, "V%d", va_arg(ap, jint)); anota(b); break;
    case 16: snprintf(b, sizeof b, "B%d", va_arg(ap, jint)); anota(b); break;
    case 17: anota("T"); anota((const char *)va_arg(ap, jstring)); anota(";"); break;
    case 18: anota("D"); break;
    default: break;
  }
  va_end(ap);
}

int main(void) {
  jni.NewGlobalRef = global; jni.GetStaticMethodID = metodo;
  jni.NewString = texto; jni.DeleteLocalRef = apagar;
  jni.ExceptionCheck = temExcecao; jni.ExceptionClear = limparExcecao;
  jni.CallStaticVoidMethod = chamar;
  Java_space_nuvio_nativelegacy_NvPlayer_nativeIniciar(&env, (jclass)(uintptr_t)3);

  // A trailer before any player: no cache call (0 is Kotlin's default), no gain.
  registro[0] = 0;
  assert(video_tocar("https://example.invalid/trailer.mp4"));
  assert(!strcmp(registro, "A"));
  assert(pressao == 0);   // trailer: home art stays, texture cap untouched

  // Failed open does not leave the texture cap on.
  pressao = -1; semEnv = 1;
  assert(!video_tocar("https://example.invalid/trailer.mp4"));
  semEnv = 0;
  assert(pressao == -1);

  // Player session, cache 512: the limit goes BEFORE the open; volume 100 is
  // not sent.
  cacheboost_sessao();
  registro[0] = 0;
  cacheboost_backend_cache(512);
  assert(video_tocar_posicao("https://example.invalid/filme.mkv", 0));
  assert(!strcmp(registro, "C512A"));
  assert(pressao == 1);   // full-screen player caps the texture cache

  // WARM RESUME (#492): the title armed with the cache crosses JNI before the
  // open, once per change; the next open of the same title does not resend it.
  registro[0] = 0;
  cacheboost_backend_cache(512);
  cacheboost_backend_titulo("tt1:2:3");
  assert(video_tocar_posicao("https://example.invalid/filme.mkv", 0));
  assert(!strcmp(registro, "Ttt1:2:3;A"));
  registro[0] = 0;
  cacheboost_backend_cache(512);
  cacheboost_backend_titulo("tt1:2:3");
  assert(video_tocar_posicao("https://example.invalid/filme.mkv", 120));
  assert(!strcmp(registro, "A"));
  // Reconnection reopen keeps the title; a title armed only for a trailer
  // (never armed with cache) is ignored: the trailer sends the empty id.
  registro[0] = 0;
  assert(abrirSessao(0, 0));
  assert(!strcmp(registro, "A"));
  registro[0] = 0;
  cacheboost_backend_titulo("tt9");   // armed title without arming the cache
  assert(video_tocar("https://example.invalid/trailer-x.mp4"));
  assert(!strcmp(registro, "C0T;A"));
  assert(!tituloArmado[0] && !tituloSessao[0]);
  // Profile switch / setting off: the drop order reaches Kotlin.
  registro[0] = 0;
  cacheboost_backend_descartar();
  assert(!strcmp(registro, "D"));
  // Back to the player session the next blocks expect.
  cacheboost_sessao();
  registro[0] = 0;
  cacheboost_backend_cache(512);
  assert(video_tocar_posicao("https://example.invalid/filme.mkv", 0));
  assert(!strcmp(registro, "C512A"));

  // The person raises to 150%: sent once per change, not when unchanged.
  registro[0] = 0;
  for (int i = 0; i < 5; i++) cacheboost_backend_ganho(cacheboost_volume_passo(1));
  assert(cacheboost_volume() == 150);
  cacheboost_backend_ganho(150);
  assert(!strcmp(registro, "G110G120G130G140G150"));
  cacheboost_backend_ganho(999);   // clamped to 200
  assert(strstr(registro, "G200"));
  cacheboost_backend_ganho(150);

  // Reconnection reopen of the SAME video: no new arm, same cache (not re-sent),
  // and the volume is re-applied after the open (Kotlin starts each open at 100).
  registro[0] = 0;
  assert(abrirSessao(0, 0));
  assert(!strcmp(registro, "AG150"));

  // Source change inside the title: the player arms again (same limit, not
  // re-sent) and the volume follows the new open.
  registro[0] = 0;
  cacheboost_backend_cache(512);
  assert(video_tocar_posicao("https://example.invalid/outra-fonte.mkv", 30));
  assert(!strcmp(registro, "AG150"));

  // Live channel (player arms 0): cache off crosses JNI before the open.
  registro[0] = 0;
  cacheboost_backend_cache(0);
  assert(video_tocar("https://example.invalid/canal.ts"));
  assert(!strncmp(registro, "C0A", 3));

  // A trailer after the player: unarmed -> cache stays off, NO boost although
  // the C session volume is still 150.
  cacheboost_backend_cache(1024);
  assert(video_tocar_posicao("https://example.invalid/filme2.mkv", 0));
  registro[0] = 0;
  assert(video_tocar("https://example.invalid/trailer2.mp4"));
  assert(!strcmp(registro, "C0A"));

  // Stopped player: a late gain change does not reach Kotlin.
  video_parar();
  registro[0] = 0;
  cacheboost_backend_ganho(180);
  assert(!registro[0]);

  // Kotlin reports -> cacheboost.
  Java_space_nuvio_nativelegacy_NvPlayer_nativeCache(&env, NULL, CB_CACHE_ATIVO, 1024, 512, 40);
  assert(cacheboost_cache().estado == CB_CACHE_ATIVO && cacheboost_cache().limiteMb == 512);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeCache(&env, NULL, CB_CACHE_DISCO_CHEIO, 1024, 0, 0);
  assert(cacheboost_cache_aviso() != NULL);
  cacheboost_sessao(); cacheboost_volume_passo(1); cacheboost_volume_passo(1);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeAudioEstado(&env, NULL, 2);   // F06 tap: bitstream
  assert(cacheboost_volume() == 100 && cacheboost_volume_teto() == 100);

  // #202 SPEED: sent once the open is ready, never twice for the same value,
  // and again after a reopen of the same playback (the new ExoPlayer is 1x).
  cacheboost_sessao();
  assert(video_velocidade_suportada());
  assert(video_tocar_posicao("https://example.invalid/rapido.mkv", 0));
  registro[0] = 0;
  video_velocidade(150);
  video_bombear();
  assert(!registro[0]);                       // not ready yet: waits
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(&env, NULL, 1, 60000, 0);
  video_bombear(); video_bombear();
  assert(!strcmp(registro, "V150"));
  assert(video_velocidade_atual() == 150);
  registro[0] = 0;
  video_taxa_sync(950); video_bombear();
  assert(!strcmp(registro, "V143") && video_velocidade_atual() == 150);
  registro[0] = 0;
  video_taxa_sync(1000); video_bombear();
  assert(!strcmp(registro, "V150"));
  Uint64 stamp;
  relogio = 1000;
  Java_space_nuvio_nativelegacy_NvPlayer_nativePos(&env, NULL, 12500, 15000, 0, 0, sessao);
  relogio = 1200;
  assert(video_pos_amostra(&stamp) == 12.5 && stamp == 1000);
  Java_space_nuvio_nativelegacy_NvPlayer_nativePos(&env, NULL, 12500, 15000, 0, 0, sessao);
  assert(video_pos_amostra(&stamp) == 12.5 && stamp == 1200);
  registro[0] = 0;
  assert(abrirSessao(0, 0));                     // reconnection / same playback
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(&env, NULL, 1, 60000, 0);
  video_bombear();
  assert(!strcmp(registro, "AV150"));
  registro[0] = 0;
  video_velocidade(100);                      // player closes: back to 1x
  video_bombear();
  assert(!strcmp(registro, "V100"));
  registro[0] = 0;
  assert(video_tocar("https://example.invalid/trailer3.mp4"));
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(&env, NULL, 1, 60000, 0);
  video_bombear();
  assert(!strcmp(registro, "A"));            // the trailer is never sped up

  // Personal speed remains available: Kotlin switches passthrough to PCM.
  Java_space_nuvio_nativelegacy_NvPlayer_nativeAudioEstado(&env, NULL, 2);
  assert(!video_velocidade_bloqueada());
  Java_space_nuvio_nativelegacy_NvPlayer_nativeAudioEstado(&env, NULL, 1);
  assert(!video_velocidade_bloqueada());
  // The player MEASURED 1x after "ok": refused -> back to 1x in the pipeline,
  // the row gone, later requests ignored.
  video_velocidade(150); video_bombear();
  registro[0] = 0;
  video_velocidade_recusada(); video_bombear();
  assert(!strcmp(registro, "V100"));
  assert(!video_velocidade_suportada() && video_velocidade_atual() == 100);
  video_velocidade(200); video_bombear();
  assert(!strcmp(registro, "V100"));

  // #338: buffer MiB crosses before open, once per change, with invalid sizes reset.
  registro[0] = 0;
  cacheboost_backend_buffer(128);
  assert(video_tocar("https://example.invalid/buffer.mkv"));
  assert(strstr(registro, "B128") && strstr(registro, "B128") < strchr(registro, 'A'));
  registro[0] = 0;
  assert(abrirSessao(0, 0));
  assert(!strchr(registro, 'B'));
  registro[0] = 0;
  cacheboost_backend_buffer(999);
  assert(abrirSessao(0, 0));
  assert(strstr(registro, "B0"));
  { VideoStats st;
    Java_space_nuvio_nativelegacy_NvPlayer_nativeStats(&env, NULL, 1000, 25000, 150000, 2, 4, 12, 3, 100, NULL, 50000, 80, 1);
    video_estatisticas(&st);
    assert(st.bytes == 1000 && st.bufferSeg == 25 && st.bufferAlvoSeg == 150 && st.quadrosPerdidos == 3);
    assert(st.conexoes == 2 && st.poolConexoes == 4);
    assert(abrirSessao(0, 0));
    video_estatisticas(&st);
    assert(st.bytes == -1 && st.bufferSeg < 0);
  }
  // An older Kotlin shell without cache()/ganho()/buffer(): open still works.
  semCache = 1;
  assert(resolverMetodos(&env));
  registro[0] = 0;
  cacheboost_backend_cache(256);
  assert(video_tocar_posicao("https://example.invalid/legado.mkv", 0));
  cacheboost_backend_ganho(130);
  assert(!strcmp(registro, "A"));
  assert(!video_velocidade_suportada());     // no velocidade() either: no row
  puts("video Android F07: cache armed per player open, sent on change before the open, gain and speed re-applied per open, trailers untouched: PASS");
  return 0;
}
