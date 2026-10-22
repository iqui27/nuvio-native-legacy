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
void *SDL_AndroidGetJNIEnv(void) { return &env; }
void *SDL_AndroidGetActivity(void) { return NULL; }
Uint32 SDL_GetTicks(void) { return relogio; }
Uint64 SDL_GetTicks64(void) { return relogio; }
SDL_mutex *SDL_CreateMutex(void) { return (SDL_mutex *)(uintptr_t)1; }
void marco(const char *s) { (void)s; }

// Call log: 'A' open, 'C<n>' cache(n), 'G<n>' ganho(n), 'V<n>' velocidade(n), 'P' stop.
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
  if (!strcmp(nome, "velocidade")) return semCache ? NULL : (jmethodID)(uintptr_t)15;
  return (jmethodID)(uintptr_t)1;
}
static jstring JNICALL texto(JNIEnv *e, const jchar *u, jsize n) { (void)e; (void)u; (void)n; return (jstring)malloc(1); }
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

  // Player session, cache 512: the limit goes BEFORE the open; volume 100 is
  // not sent.
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
  assert(abrirSessao(0));
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
  Java_space_nuvio_nativelegacy_NvPlayer_nativePos(&env, NULL, 12500);
  relogio = 1200;
  assert(video_pos_amostra(&stamp) == 12.5 && stamp == 1000);
  Java_space_nuvio_nativelegacy_NvPlayer_nativePos(&env, NULL, 12500);
  assert(video_pos_amostra(&stamp) == 12.5 && stamp == 1200);
  registro[0] = 0;
  assert(abrirSessao(0));                     // reconnection / same playback
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

  // Passthrough (TCL 06/10): the row is blocked with a reason; PCM frees it.
  Java_space_nuvio_nativelegacy_NvPlayer_nativeAudioEstado(&env, NULL, 2);
  assert(video_velocidade_bloqueada());
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

  // An older Kotlin shell without cache()/ganho(): nothing crosses, open works.
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
