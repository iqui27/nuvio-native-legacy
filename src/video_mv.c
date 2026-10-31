// MULTIVIEW — decodificadores extra (ver video.h). Android: ponte JNI para o
// objeto Kotlin NvExtra (ExoPlayer secundario, SurfaceView propria atras da
// SDL, sem audio). Demais plataformas: um pipeline so, video_mv_max() = 0 e
// os slots extras caem no estado "limite de decodificadores" da interface.
#include "video.h"
#include <string.h>
#include <stdio.h>

#ifdef NV_ANDROID
#include <SDL2/SDL.h>
#include <jni.h>

#define NV_CLASSE_MV "space/nuvio/nativelegacy/NvExtra"
#define MV_MAX 3

static jclass mvCls;
static jmethodID mMvAbrir, mMvJanela, mMvParar, mMvPararTodos, mMvEstado, mMvMax;
static int mvTentou, mvOk;

static int mvResolver(JNIEnv *env) {
  mMvAbrir = (*env)->GetStaticMethodID(env, mvCls, "abrir", "(ILjava/lang/String;)V");
  mMvJanela = (*env)->GetStaticMethodID(env, mvCls, "janela", "(IIIII)V");
  mMvParar = (*env)->GetStaticMethodID(env, mvCls, "parar", "(I)V");
  mMvPararTodos = (*env)->GetStaticMethodID(env, mvCls, "pararTodos", "()V");
  mMvEstado = (*env)->GetStaticMethodID(env, mvCls, "estado", "(I)I");
  mMvMax = (*env)->GetStaticMethodID(env, mvCls, "maximo", "()I");
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return 0; }
  return mMvAbrir && mMvJanela && mMvParar && mMvPararTodos && mMvEstado && mMvMax;
}

// Chamado pelo Kotlin (NvExtra.iniciar) no fio principal; sem ele, a primeira
// chamada tenta o ClassLoader da Activity (mesmo plano B do NvPlayer).
JNIEXPORT void JNICALL Java_space_nuvio_nativelegacy_NvExtra_nativeIniciar(JNIEnv *env, jclass cls) {
  if (!mvCls) mvCls = (*env)->NewGlobalRef(env, cls);
  mvOk = mvResolver(env);
  if (!mvOk) { mvCls = NULL; printf("[video] mv: metodos do NvExtra nao achados\n"); }
  else printf("[video] mv: ponte do NvExtra pronta\n");
  fflush(stdout);
}

static JNIEnv *mvAmbiente(void) {
  if (mvTentou && !mvCls) return NULL;
  mvTentou = 1;
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  if (!env) return NULL;
  if (!mvCls) {
    jobject act = (jobject)SDL_AndroidGetActivity();
    if (!act) return NULL;
    jclass cAct = (*env)->GetObjectClass(env, act);
    jmethodID mGet = (*env)->GetMethodID(env, cAct, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject loader = mGet ? (*env)->CallObjectMethod(env, act, mGet) : NULL;
    jclass cLoader = loader ? (*env)->GetObjectClass(env, loader) : NULL;
    jmethodID mLoad = cLoader ? (*env)->GetMethodID(env, cLoader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;") : NULL;
    jstring nome = (*env)->NewStringUTF(env, NV_CLASSE_MV);
    jclass achada = (mLoad && nome) ? (jclass)(*env)->CallObjectMethod(env, loader, mLoad, nome) : NULL;
    if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); achada = NULL; }
    if (achada) { mvCls = (*env)->NewGlobalRef(env, achada); mvOk = mvResolver(env); if (!mvOk) mvCls = NULL; }
    if (nome) (*env)->DeleteLocalRef(env, nome);
    if (achada) (*env)->DeleteLocalRef(env, achada);
    if (cLoader) (*env)->DeleteLocalRef(env, cLoader);
    if (loader) (*env)->DeleteLocalRef(env, loader);
    (*env)->DeleteLocalRef(env, cAct);
  }
  return mvCls ? env : NULL;
}

int video_mv_max(void) {
  JNIEnv *env = mvAmbiente();
  if (!env) return 0;
  int n = (int)(*env)->CallStaticIntMethod(env, mvCls, mMvMax);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return 0; }
  return n < 0 ? 0 : n > MV_MAX ? MV_MAX : n;
}

int video_mv_abrir(int slot, const char *url) {
  JNIEnv *env = mvAmbiente();
  if (!env || !url || !url[0] || slot < 0 || slot >= MV_MAX) return -1;
  jstring u = (*env)->NewStringUTF(env, url);
  (*env)->CallStaticVoidMethod(env, mvCls, mMvAbrir, slot, u);
  (*env)->DeleteLocalRef(env, u);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return -1; }
  return 0;
}

void video_mv_janela(int slot, int x, int y, int w, int h) {
  JNIEnv *env = mvAmbiente();
  if (!env || slot < 0 || slot >= MV_MAX) return;
  (*env)->CallStaticVoidMethod(env, mvCls, mMvJanela, slot, x, y, w, h);
  if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
}

void video_mv_parar(int slot) {
  JNIEnv *env = mvAmbiente();
  if (!env || slot < 0 || slot >= MV_MAX) return;
  (*env)->CallStaticVoidMethod(env, mvCls, mMvParar, slot);
  if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
}

void video_mv_parar_todos(void) {
  JNIEnv *env = mvAmbiente();
  if (!env) return;
  (*env)->CallStaticVoidMethod(env, mvCls, mMvPararTodos);
  if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
}

int video_mv_estado(int slot) {
  JNIEnv *env = mvAmbiente();
  if (!env || slot < 0 || slot >= MV_MAX) return 0;
  int r = (int)(*env)->CallStaticIntMethod(env, mvCls, mMvEstado, slot);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return 0; }
  return r;
}

#else  /* !NV_ANDROID: plataformas de um pipeline so */

int  video_mv_max(void) { return 0; }
int  video_mv_abrir(int slot, const char *url) { (void)slot; (void)url; return -1; }
void video_mv_janela(int slot, int x, int y, int w, int h) { (void)slot; (void)x; (void)y; (void)w; (void)h; }
void video_mv_parar(int slot) { (void)slot; }
void video_mv_parar_todos(void) {}
int  video_mv_estado(int slot) { (void)slot; return 0; }

#endif
