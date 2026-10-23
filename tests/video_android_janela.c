// JANELA DO ANDROID: o nucleo so entrega ao Kotlin retangulos NOVOS. Dois
// modos de aspecto que coincidem no mesmo filme (Ajustar largura e Original
// num 2.39:1) e a re-leitura do videoInfo mandavam o MESMO retangulo de novo
// — o TCL 2160p DV (11/10) pediu "1920x1037" duas vezes seguidas. O Kotlin
// nada faria (layout identico) e o dvtrace enchia de "changed" sem mudanca.
// O reaplicar do trailer segue reenviando SEMPRE: existe para isso.
#define NV_ANDROID 1
#include <SDL2/SDL.h>
void *SDL_AndroidGetJNIEnv(void);
void *SDL_AndroidGetActivity(void);
#include "../src/video_android.c"
#include "../src/cacheboost.c"
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

int xtream_url_limitada(const char *u) { (void)u; return 0; }
int xtream_conexoes_limitadas(void) { return 0; }
int xtream_conta_ler(XtreamConta *c) { memset(c, 0, sizeof *c); return 0; }
int stream_kbps_atual(void) { return 0; }
static int cap4k[4];
void stream_definir_decoder4k(int h, int a, int v, int av) {
  cap4k[0] = h; cap4k[1] = a; cap4k[2] = v; cap4k[3] = av;
}
static struct JNINativeInterface_ jni;
static const struct JNINativeInterface_ *env = &jni;
static int chamadasJanela, chamadasNormais, chamadasPosicao, recebidoGeracao, excecao, lancar;
static int janelaX, janelaY, janelaW, janelaH, janelaEncaixa;
static int appliedWindow;
static jboolean JNICALL windowReady(JNIEnv *e, jclass c, jmethodID m, ...) {
  (void)e; (void)c; (void)m;
  return appliedWindow == chamadasJanela;
}
static unsigned relogio = 100;
void *SDL_AndroidGetJNIEnv(void) { return &env; }
void *SDL_AndroidGetActivity(void) { return NULL; }
Uint32 SDL_GetTicks(void) { return relogio; }
SDL_mutex *SDL_CreateMutex(void) { return (SDL_mutex *)(uintptr_t)1; }
void marco(const char *s) { (void)s; }
const char *ling_audio(void) { return ""; }
void tex_pressao_reproducao(int l) { (void)l; }
void vel_log(int cent, const char *estado) { (void)cent; (void)estado; }

static jobject JNICALL global(JNIEnv *e, jobject o) { (void)e; return o; }
static jmethodID JNICALL metodo(JNIEnv *e, jclass c, const char *nome, const char *sig) {
  (void)e; (void)c;
  if (!strcmp(nome, "janela")) {
    assert(!strcmp(sig, "(IIIII)V"));
    return (jmethodID)(uintptr_t)9;   // id proprio: chamar() grava a janela
  }
  if (!strcmp(nome, "abrirPosicao")) return (jmethodID)(uintptr_t)2;
  if (!strcmp(nome, "abrirRetomada")) return (jmethodID)(uintptr_t)5;
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
  if (m == (jmethodID)(uintptr_t)9) {
    va_list ap; va_start(ap, m);
    janelaX = va_arg(ap, jint); janelaY = va_arg(ap, jint);
    janelaW = va_arg(ap, jint); janelaH = va_arg(ap, jint);
    janelaEncaixa = va_arg(ap, jint);
    va_end(ap);
    chamadasJanela++;
    return;
  }
  if (m == mAbrirRetomada || m == mAbrirPosicao) {
    va_list ap; va_start(ap, m);
    (void)va_arg(ap, jstring); (void)va_arg(ap, jstring);
    (void)va_arg(ap, jint);                    // inicioMs
    if (m == mAbrirRetomada) (void)va_arg(ap, jint);   // fracao
    recebidoGeracao = va_arg(ap, jint);
    if (m == mAbrirPosicao) chamadasPosicao++;
    if (lancar) excecao = 1;
    va_end(ap);
    return;
  }
  chamadasNormais++;
}

// Zoom leve do filme 3836x2072 da TCL: recorte 316,134 3202x1802 no destino
// visivel 0,0 1920x1080 vira a janela virtual -189,-80 2300x1242 (encaixa 0).
static void zoomLeve(void) {
  video_janela_fonte(316, 134, 3202, 1802, 0, 0, 1920, 1080);
}
int main(void) {
  jni.NewGlobalRef = global; jni.GetStaticMethodID = metodo;
  jni.NewString = texto; jni.DeleteLocalRef = apagar;
  jni.ExceptionCheck = temExcecao; jni.ExceptionClear = limparExcecao;
  jni.CallStaticVoidMethod = chamar;
  jni.CallStaticBooleanMethod = windowReady;
  Java_space_nuvio_nativelegacy_NvPlayer_nativeIniciar(&env, (jclass)(uintptr_t)3);
  mParar = (jmethodID)(uintptr_t)4;
  assert(video_tocar_posicao("https://example.invalid/filme.mp4", 0));
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_TAMANHO, 3836, 2072);
  assert(video_largura() == 3836 && video_altura() == 2072);

  // 1. O primeiro pedido de janela chega: e so um.
  zoomLeve();
  assert(chamadasJanela == 1);
  assert(janelaX == -189 && janelaY == -80 && janelaW == 2300 && janelaH == 1242);
  assert(janelaEncaixa == 0);
  assert(!video_janela_pronta());
  appliedWindow = chamadasJanela;
  assert(video_janela_pronta());
  // 2. O MESMO retangulo de novo (dois modos que coincidem, videoInfo de
  // novo): nada via JNI — o log da TCL pedia "1920x1037" duas vezes.
  zoomLeve();
  assert(chamadasJanela == 1);
  // 3. O reaplicar do trailer reenvia SEMPRE, mesmo identico: e o contrato
  // dele (a superficie pode ter sido montada de novo la fora).
  video_recorte_reaplicar();
  assert(chamadasJanela == 2 && janelaX == -189 && janelaW == 2300);
  assert(!video_janela_pronta()); // Previous layout acknowledgement cannot release a new request.
  video_recorte_reaplicar();
  assert(chamadasJanela == 3);
  puts("ok dedupe do reenvio; reaplicar fora dele de proposito");

  // 4. Retangulo DIFERENTE entrega; encaixa diferente tambem entrega (a
  // semantica muda: tarja contra recorte exato).
  video_janela_fonte(0, 0, 3836, 2072, 0, 21, 1920, 1037);   // quadro inteiro
  assert(chamadasJanela == 4 && janelaX == 0 && janelaY == 21 &&
         janelaW == 1920 && janelaH == 1037 && janelaEncaixa == 0);
  video_janela_fonte(0, 0, 3836, 2072, 0, 21, 1920, 1037);
  assert(chamadasJanela == 4);
  video_janela(0, 21, 1920, 1037);                            // encaixa=1
  assert(chamadasJanela == 5 && janelaEncaixa == 1);
  video_janela(0, 21, 1920, 1037);
  assert(chamadasJanela == 5);
  // 5. Sem dimensoes do quadro cai na janela lisa (encaixa), dedupe incluido.
  video_parar();
  video_janela_fonte(0, 0, 0, 0, 5, 5, 100, 50);
  assert(chamadasJanela == 6 && janelaX == 5 && janelaY == 5 &&
         janelaW == 100 && janelaH == 50 && janelaEncaixa == 1);
  video_janela_fonte(0, 0, 0, 0, 5, 5, 100, 50);
  assert(chamadasJanela == 6);
  puts("ok retangulo novo e encaixa diferente entregam; repetidos nao");

  // 6. Abertura nova zera a memoria: a SurfaceView de cada abertura nasce sem
  // janela e precisa receber ate o retangulo igual ao da sessao anterior.
  assert(video_tocar_posicao("https://example.invalid/segunda.mp4", 0));
  assert(chamadasPosicao == 2 && recebidoGeracao == (int)video_android_sessao());
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_TAMANHO, 3836, 2072);
  zoomLeve();
  assert(chamadasJanela == 7 && janelaX == -189 && janelaW == 2300);
  zoomLeve();
  assert(chamadasJanela == 7);
  puts("ok abertura nova reentrega; repetidos seguem dedupados");

  puts("video Android janela: PASS (dedupe do reenvio, reaplicar intacto, reset por sessao)");
}
