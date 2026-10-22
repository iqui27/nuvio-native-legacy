// Ponte do nucleo com o Android. Ver android.h.
#ifdef NV_ANDROID
#include "android.h"
#include "queda.h"
#include <SDL2/SDL.h>
#include <android/log.h>
#include <jni.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <android/api-level.h>

#define AND_TAG "nuvio"

static void interfaceBool(const char *nome, int ativa) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  jobject act;
  jclass cls;
  jmethodID metodo;
  if (!env) return;
  act = (jobject)SDL_AndroidGetActivity();
  if (!act) return;
  cls = (*env)->GetObjectClass(env, act);
  if (cls) {
    metodo = (*env)->GetMethodID(env, cls, nome, "(Z)V");
    if (metodo) (*env)->CallVoidMethod(env, act, metodo, ativa ? JNI_TRUE : JNI_FALSE);
    (*env)->DeleteLocalRef(env, cls);
  }
  if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
  (*env)->DeleteLocalRef(env, act);
}

void android_interface_modo(int mobile) { interfaceBool("interfaceModo", mobile); }

#ifdef NV_TOUCH_UI
static SDL_atomic_t toqueCancelEvento;

void android_player_tela_cheia(int ativa) { interfaceBool("orientarPlayer", ativa); }

int android_toque_cancelado(const SDL_Event *e) {
  int tipo = SDL_AtomicGet(&toqueCancelEvento);
  return tipo > 0 && e->type == (Uint32)tipo;
}

JNIEXPORT void JNICALL
Java_space_nuvio_nativelegacy_NuvioActivity_nativeToqueCancelou(JNIEnv *env, jobject act) {
  SDL_Event e;
  int tipo;
  (void)env; (void)act;
  if (!SDL_WasInit(SDL_INIT_EVENTS)) return;
  // Chamado apenas pelo fio da interface Android; o fio SDL so le o tipo.
  tipo = SDL_AtomicGet(&toqueCancelEvento);
  if (!tipo) {
    Uint32 novo = SDL_RegisterEvents(1);
    tipo = novo == (Uint32)-1 ? -1 : (int)novo;
    SDL_AtomicSet(&toqueCancelEvento, tipo);
  }
  if (tipo <= 0) return;
  SDL_zero(e); e.type = (Uint32)tipo;
  SDL_PushEvent(&e);
}
#endif

// A linha de modelo que o host .tpk escreve (tizen-tpk/Program.cs, LogaTv):
//   [tv] modelo=<modelo> host=<host> dotnet=<...> tela=<WxH>
// Aqui o "host" e o Android e nao ha .NET. NUVIO_TV_INFO vem do NuvioActivity:
//   "<MANUFACTURER> <MODEL>|<SDK_INT>|<RELEASE>|<versionName>"
// A tela ainda nao existe neste ponto (o SDL nem iniciou); a linha `janela=` do
// main.c ja diz o drawable.
static void logaTv(void) {
  char info[256], *campo[4] = { "", "", "", "" }, *p = info;
  const char *e = getenv("NUVIO_TV_INFO");
  int n = 0;
  snprintf(info, sizeof info, "%s", e ? e : "");
  campo[n++] = p;
  while (*p && n < 4) { if (*p == '|') { *p = 0; campo[n++] = p + 1; } p++; }
  printf("[tv] modelo=%s host=android-%s dotnet=- tela=- sdk=%s app=%s\n",
         campo[0][0] ? campo[0] : "?", campo[2][0] ? campo[2] : "?",
         campo[1][0] ? campo[1] : "?", campo[3][0] ? campo[3] : "?");
  e = getenv("NUVIO_DECODERS");
  if (e && e[0]) printf("[tv] decoders de video: %s\n", e);
  // Motivo da morte do processo anterior (ApplicationExitInfo, NuvioActivity).
  e = getenv("NUVIO_SAIDA_ANTERIOR");
  if (e && e[0]) printf("[android] saida anterior: %s\n", e);
  // E, se foi crash nativo no Android 12+, onde: o resumo do tombstone, ja em
  // linhas "[queda] ..." (Tombstone.kt).
  e = getenv("NUVIO_QUEDA_ANDROID");
  if (e && e[0]) printf("%s\n", e);
  fflush(stdout);
}

// ESPELHO NO LOGCAT. stdout e stderr (o mesmo descritor do arquivo de log, ver
// main.c) passam a entrar num pipe; um fio le o pipe e grava os mesmos bytes no
// arquivo (descritor guardado antes) e, linha a linha, no logcat. O arquivo
// continua inteiro: e ele que o painel de log e o "Enviar registro" leem.
static int fdArquivo = -1, fdLeitura = -1;

static void *espelho(void *arg) {
  char buf[2048], linha[1100];
  size_t nl = 0;
  ssize_t n;
  (void)arg;
  while ((n = read(fdLeitura, buf, sizeof buf)) > 0) {
    ssize_t i;
    if (fdArquivo >= 0) { ssize_t off = 0; while (off < n) { ssize_t w = write(fdArquivo, buf + off, (size_t)(n - off)); if (w <= 0) break; off += w; } }
    for (i = 0; i < n; i++) {
      if (buf[i] == '\n' || nl + 1 >= sizeof linha) {
        linha[nl] = 0;
        if (nl) __android_log_write(ANDROID_LOG_INFO, AND_TAG, linha);
        nl = 0;
        if (buf[i] != '\n') linha[nl++] = buf[i];
      } else linha[nl++] = buf[i];
    }
  }
  return NULL;
}

static void espelharNoLogcat(void) {
  int p[2];
  pthread_t t;
  fflush(stdout); fflush(stderr);
  if (pipe(p) != 0) return;
  fdArquivo = dup(STDOUT_FILENO);   // o arquivo de log (ou /dev/null)
  fdLeitura = p[0];
  if (pthread_create(&t, NULL, espelho, NULL) != 0) {
    close(p[0]); close(p[1]); if (fdArquivo >= 0) close(fdArquivo); fdArquivo = -1;
    return;
  }
  pthread_detach(t);
  dup2(p[1], STDOUT_FILENO);
  dup2(p[1], STDERR_FILENO);
  close(p[1]);
  setvbuf(stdout, NULL, _IOLBF, 0);
}

// RELATOR DE QUEDA NATIVA para Android < 12 (#318, queda.h). Changhong AI
// PONT (Android 11, MStar): "crash-nativo status=11" ao tocar, e o log acaba
// sem dizer onde; o tombstone do ApplicationExitInfo (Tombstone.kt) so existe
// do 12 em diante. No 12+ fica desligado: o tombstone ja diz, e melhor.
// No tratador, depois do relato: o que ainda esta no pipe do espelho vai para
// o arquivo de log (as ultimas linhas antes da queda se perdiam ali) e uma
// linha marca o momento. So poll/read/write.
static void drenarNaQueda(void) {
  static char b[4096];
  static const char marca[] = "[queda] sinal fatal: relato gravado, sai no log da proxima abertura\n";
  int k;
  if (fdLeitura < 0 || fdArquivo < 0) return;
  for (k = 0; k < 64; k++) {
    struct pollfd p;
    ssize_t n, off = 0;
    p.fd = fdLeitura; p.events = POLLIN; p.revents = 0;
    if (poll(&p, 1, 0) <= 0 || !(p.revents & POLLIN)) break;
    n = read(fdLeitura, b, sizeof b);
    if (n <= 0) break;
    while (off < n) { ssize_t w = write(fdArquivo, b + off, (size_t)(n - off)); if (w <= 0) break; off += w; }
  }
  if (write(fdArquivo, marca, sizeof marca - 1) < 0) { /* sem rastro e so isso */ }
}

static void armarRelatorDeQueda(void) {
  const char *d = getenv("NUVIO_DADOS");
  char arq[600];
  int api = android_get_device_api_level();
  if (!d || !d[0]) return;
  snprintf(arq, sizeof arq, "%s/queda-nativa.txt", d);
  queda_relatar(arq);   // da sessao anterior, se ela caiu com o relator armado
  if (api > 0 && api < 31) {
    queda_armar_encadeado(arq, drenarNaQueda);
    printf("[queda] relator nativo armado (Android api %d, encadeado a ART/debuggerd)\n", api);
  }
  fflush(stdout);
}

void android_iniciar(void) {
  // Mesma pilha para os fios criados pelo SDL (SDL_CreateThread).
  SDL_SetHint(SDL_HINT_THREAD_STACK_SIZE, "8388608");
  __android_log_write(ANDROID_LOG_INFO, AND_TAG, "nucleo C iniciando");
  // O Voltar chega como SDLK_AC_BACK ao app; sem isto o SDL fecha a Activity.
  SDL_SetHint("SDL_ANDROID_TRAP_BACK_BUTTON", "1");
  espelharNoLogcat();
  logaTv();
  armarRelatorDeQueda();
}

// SUPERFICIE 4K. A TCL Smart TV Pro (Android 14) tem painel 3840x2160 mas poe
// os apps numa tela LOGICA de 1920x1080 (`wm size` override): a interface de
// todo app e desenhada em 1080p e ampliada. O plano de video nao passa por isso
// (o 4K do filme sai nitido). Uma superficie com buffer fixo de 3840x2160 e
// composta pelo SurfaceFlinger no espaco FISICO; se o plano de graficos da TV
// aceitar, a UI sai em 4K de verdade. O que a TV concedeu aparece na linha
// `janela=... drawable=...` do main.c.
int android_pedir_superficie(int w, int h) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  jobject act = (jobject)SDL_AndroidGetActivity();
  jclass cls;
  jmethodID m;
  int ok = 0;
  if (!env || !act) return 0;
  // Classe pela propria Activity: FindClass deste fio (o do SDL) usa o
  // carregador do sistema e nao acha classe do app.
  cls = (*env)->GetObjectClass(env, act);
  m = cls ? (*env)->GetMethodID(env, cls, "pedirSuperficie", "(II)Z") : NULL;
  if (m) ok = (*env)->CallBooleanMethod(env, act, m, (jint)w, (jint)h) ? 1 : 0;
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); ok = 0; }
  if (cls) (*env)->DeleteLocalRef(env, cls);
  (*env)->DeleteLocalRef(env, act);
  printf("[4k] android: superficie %dx%d %s\n", w, h, ok ? "concedida" : "NAO veio (segue o tamanho da tela)");
  fflush(stdout);
  return ok;
}

int android_instalar_apk(const char *caminho) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  jobject act = (jobject)SDL_AndroidGetActivity();
  jclass cls;
  jmethodID m;
  jstring js;
  int r = 0;
  if (!env || !act || !caminho) return 0;
  cls = (*env)->GetObjectClass(env, act);
  m = cls ? (*env)->GetMethodID(env, cls, "instalarApk", "(Ljava/lang/String;)I") : NULL;
  js = (*env)->NewStringUTF(env, caminho);
  if (m && js) r = (int)(*env)->CallIntMethod(env, act, m, js);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); r = 0; }
  if (js) (*env)->DeleteLocalRef(env, js);
  if (cls) (*env)->DeleteLocalRef(env, cls);
  (*env)->DeleteLocalRef(env, act);
  printf("[atualizacao] android: instalador %s\n", r == 1 ? "aberto" : r == 2 ? "pede permissao" : "falhou");
  fflush(stdout);
  return r;
}

// TEXTO DO SISTEMA (sistexto.h): teclado do sistema e voz. Tudo chamado do fio
// do SDL; o NuvioActivity faz o trabalho no fio da interface e devolve o que
// aconteceu numa fila de strings que android_st_evento drena.
static int chamaBool(const char *nome, const char *assin, const char *s, int i, int temInt) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  jobject act = (jobject)SDL_AndroidGetActivity();
  jclass cls;
  jmethodID m;
  jstring js = NULL;
  int ok = 0;
  if (!env || !act) return 0;
  cls = (*env)->GetObjectClass(env, act);
  m = cls ? (*env)->GetMethodID(env, cls, nome, assin) : NULL;
  if (s) js = (*env)->NewStringUTF(env, s);
  if (m) {
    if (s && temInt) ok = (*env)->CallBooleanMethod(env, act, m, js, (jint)i) ? 1 : 0;
    else if (s) ok = (*env)->CallBooleanMethod(env, act, m, js) ? 1 : 0;
    else ok = (*env)->CallBooleanMethod(env, act, m) ? 1 : 0;
  }
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); ok = 0; }
  if (js) (*env)->DeleteLocalRef(env, js);
  if (cls) (*env)->DeleteLocalRef(env, cls);
  (*env)->DeleteLocalRef(env, act);
  return ok;
}

int android_st_teclado(const char *inicial, int max) {
  return chamaBool("abrirTeclado", "(Ljava/lang/String;I)Z", inicial ? inicial : "", max, 1);
}
int android_st_ditar(const char *idioma) {
  return chamaBool("ditar", "(Ljava/lang/String;)Z", idioma ? idioma : "", 0, 0);
}
void android_st_fechar(void) { chamaBool("fecharEntrada", "()Z", NULL, 0, 0); }

int android_st_evento(char *dst, size_t n) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  jobject act = (jobject)SDL_AndroidGetActivity();
  jclass cls;
  jmethodID m;
  jstring js = NULL;
  int r = 0;
  if (n) dst[0] = 0;
  if (!env || !act) return 0;
  cls = (*env)->GetObjectClass(env, act);
  m = cls ? (*env)->GetMethodID(env, cls, "proximoEvento", "()Ljava/lang/String;") : NULL;
  if (m) js = (jstring)(*env)->CallObjectMethod(env, act, m);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); js = NULL; }
  if (js) {
    const char *c = (*env)->GetStringUTFChars(env, js, NULL);
    if (c) { snprintf(dst, n, "%s", c); r = 1; (*env)->ReleaseStringUTFChars(env, js, c); }
    (*env)->DeleteLocalRef(env, js);
  }
  if (cls) (*env)->DeleteLocalRef(env, cls);
  (*env)->DeleteLocalRef(env, act);
  return r;
}

// Metodo da Activity com ate dois textos; devolve o jobject/valor pelo tipo.
static jmethodID metodo(JNIEnv *env, jobject act, jclass *cls, const char *nome, const char *assin) {
  *cls = (*env)->GetObjectClass(env, act);
  return *cls ? (*env)->GetMethodID(env, *cls, nome, assin) : NULL;
}

char *android_listar_apps(void) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  jobject act = (jobject)SDL_AndroidGetActivity();
  jclass cls = NULL;
  jmethodID m;
  char *r = NULL;
  if (!env || !act) return NULL;
  m = metodo(env, act, &cls, "listarApps", "()Ljava/lang/String;");
  if (m) {
    jstring js = (jstring)(*env)->CallObjectMethod(env, act, m);
    if (!(*env)->ExceptionCheck(env) && js) {
      const char *c = (*env)->GetStringUTFChars(env, js, NULL);
      if (c) { r = strdup(c); (*env)->ReleaseStringUTFChars(env, js, c); }
    }
    if (js) (*env)->DeleteLocalRef(env, js);
  }
  if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
  if (cls) (*env)->DeleteLocalRef(env, cls);
  (*env)->DeleteLocalRef(env, act);
  return r;
}

static int chamarBool(const char *nome, const char *assin, const char *a, const char *b) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  jobject act = (jobject)SDL_AndroidGetActivity();
  jclass cls = NULL;
  jmethodID m;
  jstring ja = NULL, jb = NULL;
  int ok = 0;
  if (!env || !act) return 0;
  m = metodo(env, act, &cls, nome, assin);
  if (m && !(*env)->ExceptionCheck(env)) {
    ja = (*env)->NewStringUTF(env, a ? a : "");
    if (ja && !(*env)->ExceptionCheck(env) && b) jb = (*env)->NewStringUTF(env, b);
    if (ja && (!b || jb) && !(*env)->ExceptionCheck(env))
      ok = (b ? (*env)->CallBooleanMethod(env, act, m, ja, jb)
              : (*env)->CallBooleanMethod(env, act, m, ja)) ? 1 : 0;
  }
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); ok = 0; }
  if (ja) (*env)->DeleteLocalRef(env, ja);
  if (jb) (*env)->DeleteLocalRef(env, jb);
  if (cls) (*env)->DeleteLocalRef(env, cls);
  (*env)->DeleteLocalRef(env, act);
  return ok;
}
int android_abrir_app(const char *pacote) {
  return chamarBool("abrirApp", "(Ljava/lang/String;)Z", pacote, NULL);
}
int android_abrir_loja(const char *pacote, const char *nome) {
  return chamarBool("abrirLoja", "(Ljava/lang/String;Ljava/lang/String;)Z", pacote, nome ? nome : "");
}

// Ver android.h. A resposta vem como byte[]: 4 bytes de status (big-endian) e
// o corpo; status 0 = falha de transporte, e o corpo e o motivo.
char *android_http(const char *verbo, const char *url, const char *cabs,
                   const char *corpo, int prazoMs, int *status, long *n,
                   char *erro, size_t nErro) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  jobject act = env ? (jobject)SDL_AndroidGetActivity() : NULL;
  jclass cls = NULL;
  jmethodID m;
  jstring jm = NULL, ju = NULL, jc = NULL;
  jbyteArray jcorpo = NULL, jr = NULL;
  char *r = NULL;
  if (status) *status = 0;
  if (n) *n = 0;
  if (erro && nErro) snprintf(erro, nErro, "jni indisponivel");
  if (!env || !act) return NULL;
  m = metodo(env, act, &cls, "httpPedir", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;[BI)[B");
  if (m && !(*env)->ExceptionCheck(env)) {
    jm = (*env)->NewStringUTF(env, verbo ? verbo : "GET");
    ju = (*env)->NewStringUTF(env, url ? url : "");
    jc = (*env)->NewStringUTF(env, cabs ? cabs : "");
    if (corpo) {
      jsize k = (jsize)strlen(corpo);
      jcorpo = (*env)->NewByteArray(env, k);
      if (jcorpo) (*env)->SetByteArrayRegion(env, jcorpo, 0, k, (const jbyte *)corpo);
    }
    if (jm && ju && jc && (!corpo || jcorpo) && !(*env)->ExceptionCheck(env))
      jr = (jbyteArray)(*env)->CallObjectMethod(env, act, m, jm, ju, jc, jcorpo, (jint)prazoMs);
  }
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); jr = NULL; }
  if (jr) {
    jsize k = (*env)->GetArrayLength(env, jr);
    if (k >= 4) {
      unsigned char s4[4];
      int st;
      (*env)->GetByteArrayRegion(env, jr, 0, 4, (jbyte *)s4);
      st = (s4[0] << 24) | (s4[1] << 16) | (s4[2] << 8) | s4[3];
      r = (char *)malloc((size_t)(k - 4) + 1);
      if (r) {
        (*env)->GetByteArrayRegion(env, jr, 4, k - 4, (jbyte *)r);
        r[k - 4] = 0;
        if (st <= 0) {   // falha: o corpo e o motivo
          if (erro && nErro) snprintf(erro, nErro, "%s", r);
          free(r); r = NULL;
        } else {
          if (status) *status = st;
          if (n) *n = (long)(k - 4);
        }
      }
    }
    (*env)->DeleteLocalRef(env, jr);
  }
  if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
  if (jm) (*env)->DeleteLocalRef(env, jm);
  if (ju) (*env)->DeleteLocalRef(env, ju);
  if (jc) (*env)->DeleteLocalRef(env, jc);
  if (jcorpo) (*env)->DeleteLocalRef(env, jcorpo);
  if (cls) (*env)->DeleteLocalRef(env, cls);
  (*env)->DeleteLocalRef(env, act);
  return r;
}

// PILHA DOS FIOS. O bionic da ~1 MB a um pthread criado sem atributo; o glibc
// da LG e do Tizen da 8 MB, e o nucleo foi escrito contando com isso (vetores
// de CatItem, DiagAddon, VazCand... na pilha dos fios de descoberta e
// diagnostico). O CMake liga com --wrap=pthread_create: so as chamadas desta
// biblioteca passam por aqui. Quem ja pede tamanho (descoberta.c, mapa.c) fica
// como esta.
#define AND_PILHA_FIO (8u << 20)
int __real_pthread_create(pthread_t *t, const pthread_attr_t *a, void *(*f)(void *), void *arg);
int __wrap_pthread_create(pthread_t *t, const pthread_attr_t *a, void *(*f)(void *), void *arg) {
  pthread_attr_t at;
  int r;
  if (a) return __real_pthread_create(t, a, f, arg);
  pthread_attr_init(&at);
  pthread_attr_setstacksize(&at, AND_PILHA_FIO);
  r = __real_pthread_create(t, &at, f, arg);
  pthread_attr_destroy(&at);
  return r;
}

// VIGIA DO ARRANQUE (#266). TVs Android 11 (TCL 43P745, Shield, uma caixa)
// ficam com a tela preta ao abrir, e sem login nao chega log nenhum ao D1. O
// NuvioActivity (ArranqueVigia.kt) le daqui, do fio da interface, em que etapa
// o main() esta e quantos quadros ja sairam: sem quadro em N s, ou com o laco
// parado, ele mostra uma tela Android (dialogo) com a etapa, o fim do log, o
// envio do registro sem conta e a saida. Escrito so pelo fio do SDL.
static const char *volatile etapaAtual = "main";
static volatile long long quadrosFeitos;

void android_etapa(const char *nome) {
  if (nome) __atomic_store_n(&etapaAtual, nome, __ATOMIC_RELEASE);
}
void android_quadro(void) { __atomic_add_fetch(&quadrosFeitos, 1, __ATOMIC_RELAXED); }

JNIEXPORT jstring JNICALL
Java_space_nuvio_nativelegacy_NuvioActivity_nativeEtapa(JNIEnv *env, jobject act) {
  (void)act;
  return (*env)->NewStringUTF(env, __atomic_load_n(&etapaAtual, __ATOMIC_ACQUIRE));
}
JNIEXPORT jlong JNICALL
Java_space_nuvio_nativelegacy_NuvioActivity_nativeQuadros(JNIEnv *env, jobject act) {
  (void)env; (void)act;
  return (jlong)__atomic_load_n(&quadrosFeitos, __ATOMIC_RELAXED);
}
#ifndef NV_REC_URL
#define NV_REC_URL ""
#endif
// Base do worker de registro (env.sh), para o envio do registro sem conta.
JNIEXPORT jstring JNICALL
Java_space_nuvio_nativelegacy_NuvioActivity_nativeRecUrl(JNIEnv *env, jobject act) {
  (void)act;
  return (*env)->NewStringUTF(env, NV_REC_URL);
}

#endif

