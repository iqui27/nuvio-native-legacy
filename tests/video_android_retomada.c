// JNI falsa sobre o backend real: aceita/rejeita o novo prepare e entrega
// acknowledgements atrasados. Nao depende da JVM, SDL dinamico ou TV.
#define NV_ANDROID 1
#include <SDL2/SDL.h>
void *SDL_AndroidGetJNIEnv(void);
void *SDL_AndroidGetActivity(void);
#include "../src/video_android.c"
#include "../src/cacheboost.c"   // F07: video_android reads the session volume
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

static struct JNINativeInterface_ jni;
static const struct JNINativeInterface_ *env = &jni;
static int chamadasNormais, chamadasPosicao, recebidoMs, recebidoGeracao, excecao, lancar;
static int ausente;
static unsigned relogio = 100;
void *SDL_AndroidGetJNIEnv(void) { return &env; }
void *SDL_AndroidGetActivity(void) { return NULL; }
Uint32 SDL_GetTicks(void) { return relogio; }
Uint64 SDL_GetTicks64(void) { return relogio; }
SDL_mutex *SDL_CreateMutex(void) { return (SDL_mutex *)(uintptr_t)1; }
void marco(const char *s) { (void)s; }

static jobject JNICALL global(JNIEnv *e, jobject o) { (void)e; return o; }
static jmethodID JNICALL metodo(JNIEnv *e, jclass c, const char *nome, const char *sig) {
  (void)e; (void)c;
  if (!strcmp(nome, "abrirPosicao")) {
    assert(!strcmp(sig, "(Ljava/lang/String;Ljava/lang/String;II)V"));
    if (ausente) { excecao = 1; return NULL; }
    return (jmethodID)(uintptr_t)2;
  }
  return (jmethodID)(uintptr_t)1;
}
static jstring JNICALL texto(JNIEnv *e, const jchar *u, jsize n) {
  (void)e; (void)u; (void)n; return (jstring)malloc(1);
}
static void JNICALL apagar(JNIEnv *e, jobject o) { (void)e; free(o); }
static jboolean JNICALL temExcecao(JNIEnv *e) { (void)e; return excecao; }
static void JNICALL limparExcecao(JNIEnv *e) { (void)e; excecao = 0; }
static void JNICALL chamar(JNIEnv *e, jclass c, jmethodID m, ...) {
  (void)e; (void)c;
  if (m == mParar) return;
  va_list ap; va_start(ap, m);
  (void)va_arg(ap, jstring); (void)va_arg(ap, jstring);
  if (m == mAbrirPosicao) {
    chamadasPosicao++;
    recebidoMs = va_arg(ap, jint); recebidoGeracao = va_arg(ap, jint);
    if (lancar) excecao = 1;
  } else chamadasNormais++;
  va_end(ap);
}
int main(void) {
  jni.NewGlobalRef = global; jni.GetStaticMethodID = metodo;
  jni.NewString = texto; jni.DeleteLocalRef = apagar;
  jni.ExceptionCheck = temExcecao; jni.ExceptionClear = limparExcecao;
  jni.CallStaticVoidMethod = chamar;
  // IDs distintos so para o falso separar parar de abrir.
  Java_space_nuvio_nativelegacy_NvPlayer_nativeIniciar(&env, (jclass)(uintptr_t)3);
  mParar = (jmethodID)(uintptr_t)4;
  assert(video_tocar_posicao("https://example.invalid/um.mp4", 612.345));
  assert(chamadasPosicao == 1 && recebidoMs == 612345 && chamadasNormais == 0);
  assert(video_retomada_inicial_estado() == 0);
  int antigo = recebidoGeracao;
  Java_space_nuvio_nativelegacy_NvPlayer_nativeRetomada(&env, NULL, antigo, 1);
  assert(video_retomada_inicial_estado() == 1);

  assert(video_tocar_posicao("https://example.invalid/dois.mp4", 120));
  assert(recebidoGeracao != antigo && video_retomada_inicial_estado() == 0);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeRetomada(&env, NULL, antigo, 1);
  assert(video_retomada_inicial_estado() == 0);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeRetomada(&env, NULL, recebidoGeracao, 0);
  assert(video_retomada_inicial_estado() == -1);
  int atual = recebidoGeracao;
  video_parar();
  Java_space_nuvio_nativelegacy_NvPlayer_nativeRetomada(&env, NULL, atual, 1);
  assert(video_retomada_inicial_estado() == -1);
  puts("ok JNI posicao imutavel, ack da geracao e cancelamento");

  lancar = 1;
  int normais = chamadasNormais;
  assert(video_tocar_posicao("https://example.invalid/fallback.mp4", 50));
  assert(chamadasNormais == normais + 1 && video_retomada_inicial_estado() == -1 && !excecao);
  lancar = 0; ausente = 1;
  assert(resolverMetodos(&env)); mParar = (jmethodID)(uintptr_t)4;
  normais = chamadasNormais;
  assert(video_tocar_posicao("https://example.invalid/legado.mp4", 50));
  assert(chamadasNormais == normais + 1 && video_retomada_inicial_estado() == -1);
  puts("ok excecao e ponte anterior recuam ao abrir normal");

  int posicoes = chamadasPosicao;
  assert(video_tocar_posicao("https://example.invalid/invalido.mp4", NAN));
  assert(video_tocar_posicao("https://example.invalid/invalido.mp4", INFINITY));
  assert(video_tocar_posicao("https://example.invalid/invalido.mp4", 2147484));
  assert(video_tocar("https://example.invalid/inicio.mp4"));
  assert(chamadasPosicao == posicoes);
  // StreamFit (F03): with the current shell, a start at 0 also goes through
  // abrirPosicao so Kotlin learns the generation; resume state stays -1.
  ausente = 0; assert(resolverMetodos(&env)); mParar = (jmethodID)(uintptr_t)4;
  normais = chamadasNormais;
  assert(video_tocar("https://example.invalid/zero.mp4"));
  assert(chamadasPosicao == posicoes + 1 && chamadasNormais == normais && recebidoMs == 0);
  assert((unsigned)recebidoGeracao == video_android_sessao() && video_retomada_inicial_estado() == -1);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeRetomada(&env, NULL, recebidoGeracao, 0);
  assert(video_retomada_inicial_estado() == -1);
  puts("ok inicio em 0 entrega a geracao sem mudar a retomada");
  puts("video Android retomada: PASS (JNI, geracoes, fallback e limites)");
}
