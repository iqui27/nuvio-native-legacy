#define _GNU_SOURCE   // strcasestr
// SEEK-BAR THUMBNAILS MADE ON THIS DEVICE (Android, test): see miniatura_android.h.
#include "miniatura_android.h"

#ifndef NV_ANDROID
const SeekrProvedor *miniatura_provedor(void) { return NULL; }
int miniatura_prontas_agora(void) { return 0; }
int miniatura_status_agora(void) { return 0; }
int miniatura_direto(void) { return 0; }
void miniatura_preaquecer(const char *url, const char *cabecalhos) { (void)url; (void)cabecalhos; }
#else
#include "dados.h"
#include "ajustes.h"
#include "gfx.h"
#include "tex_cache.h"
#include "streams.h"
#include "video.h"
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <utime.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <ctype.h>
#include <jni.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/system_properties.h>
#include "miniplat.h"
#include "miniatura_pixels.h"
#include "miniatura_lifecycle.h"
#include "miniatura_progress.h"
#include "miniatura_coverage.h"
#include <stdatomic.h>
#ifdef NV_MINI_FFMPEG
#include "miniatura_motor.h"   // CPU route: Namer03's FFmpeg engine (packet mode)
#endif

#define GRADE_MS 10000L
#define TOL_MS 5000L
#define LOTE 32                      // most keyframes per multi-range request (the engine sizes each batch to ~4 MB)
// Copy choice and the optimistic start, named so a test can tune them (22/10, converged with the review):
#define RAZAO_1080 0.80              // a 1080p copy only when <= 80 % of the best 720p's size
#define RAZAO_MESMA_RES 0.70         // a copy at the playing file's resolution only when <= 70 % of its size
#define PISO_MBPS_1080 4.0           // bitrate floors: a tiny bad encode must not win on size alone
#define PISO_MBPS_720 2.0
#define PISO_MBPS_SD 1.0
#define OFFSET_MANTER_MS 3000.0      // optimistic thumbnails are kept (relabelled) up to this offset, redone above
#define TOL_COPIA_MS 8000L          // a copy keyframe this close stands in for its slot (labelled with its own time)
#define PISO_KB (300L * 1024)        // MemAvailable kept free for the player and the app
#define SIG_N (32 * 18)
#define CAB 9                        // result header: 8 bytes pts + 1 flag
#define NTEX 8
#define VIZINHO_SLOTS 30             // the bar shows the nearest made tile up to 5 min away
// timeline check (OpenSeek timeline.py)
#define N_ANCORAS 20
#define MIN_CORR 0.60
#define MIN_MARGEM 0.05
#define MIN_INLIERS 8
#define MAX_RESIDUO_MS 8000.0
#define MAX_ERRO_MS 3500.0
#define MAX_RAZAO_ERRO 0.10
#define JANELA_LARGA_MS 12000
#define PASSO_LARGO_MS 3000
#define JANELA_ESTREITA_MS 3000
#define PASSO_ESTREITO_MS 1000
#define MIN_DESVIO 4.0
#define MIN_POR_QUARTO 2
#define TOMADA_MS 8000               // candidates this far from the best are another shot
#define TENTATIVAS 12                // copies tried per title, 1080p backups included (a dead link or wrong runtime costs ~1-5 s)
#define PESADA_MBPS 20.0             // a 1080p copy above this (a disc remux) is not worth reading for thumbnails

enum { M_PARADO = 0, M_BUSCANDO, M_PRONTO, M_FALHOU };

typedef struct { double a, b, erro; int inliers; } Mapa;
// prov = made at offset 0 while the copy's check was still running (optimistic start): relabelled or discarded
// when the check decides.
typedef struct { unsigned char *jpg; int n; long pts; unsigned char prov, salvo; unsigned revision; } Tile;
// What the playing file showed at each check anchor: decoded ONCE per title and reused for every copy checked
// (the 4K decode is what the player feels; a second copy must not pay it again).
typedef struct { int estado; long pA; int ok; float sig[SIG_N]; } Ancora;   // estado: 0 not yet, 1 read, -1 failed


typedef struct {
  char url[4096], cab[512];
  int altura;
  unsigned long long tamanho;
  char nome[160];
  char fonte[FONTEFALHA_CHAVE];   // the add-on it came from (copy failures are remembered per source)
} Copia;

typedef struct {
  unsigned g;
  char urlA[4096], cabA[512];
  int alturaA, larguraSaida, avisoCompleto, cacheChecado, cacheTipo, auditarRede;
  long dur;
  Copia copias[TENTATIVAS];
  int nCopias;
  Tile *tiles; unsigned char *tentado; int n;
  MiniCoverage coverage;                   // nominal targets reference immutable frame-pool entries
  int *slotFrame; unsigned char *slotState;
  int64_t *framePts; long *slotKey;
  Ancora anc[N_ANCORAS];
  char arq[640];             // the saved set on this device ("" = cannot save)
  int feitos;
  Uint32 lacunaDesde;        // when the exact-frame gap pass started (0 = not yet)
  Mapa mapa;                 // copy time = a x play time + b, read by the lanes (under mu)
  int otimista;              // lanes are running at offset 0 before the check decided
  volatile int pararFaixas;  // stop the lanes only (a rejected copy), the job goes on
  // per-title summary (one SUMMARY line at the end: the acceptance record of a test)
  char titulo[48], motivos[320], escolhida[200];
  double offset;
  int refeito;
  int nRecheck, falhasRecheck, resumido;   // re-check while watching; summary printed once
  long caiuEm;                             // play time where the copy stopped matching (0 = never)
  long ultimoBom;                          // last play time the copy was CONFIRMED to match (check anchors, re-checks)
  int reancoras;                           // times a new offset was found after the copy stopped matching
  long long cont0[12];
  long long perdidos0;                     // player's dropped frames when the title started
  long long audio0;                        // player's audio underruns when the title started
  long memMin;                             // lowest MemAvailable seen (kB)
  long heapMin;                            // lowest free Java heap seen (MB)
  int esperasHeap;                         // times thumbnail work waited for heap room
  char arqDireto[640];                     // the direct-mode set (its own identity: never mixed with a copy's)
  Uint32 t0, primeiro, cob60, cob30;       // start; first thumbnail; whole film at 60 s / 30 s spacing (ticks)
  int direto, contencoes;                  // thumbnails from the playing file; playback-contention back-offs
  int sharedHeadroom, headerBridge, coverageAudit; // source-scoped policies/diagnostics
  double folgaTopo; Uint32 folgaTopoEm;    // never carry another title's buffer ceiling
  int appendPronto;                       // direct tiles are immutable: subsequent checkpoints append only new ones
  int cpuFeitos; long cpuMs, cpuDecMs;     // CPU route: thumbnails made, average total / decode ms
  // COPY-FIRST (06/11/2026): the direct engine run on a smaller copy that passed the check. Its keyframes are asked for
  // at copy time = dA x play time + dB and every thumbnail is stored at play time; dUrl/dCab name the copy.
  int dCopia; double dA, dB; char dUrl[4096], dCab[512]; int dAltura;
  // Optimistic copy-first: thumbnails start at offset 0 while the screen check runs on its own thread.
  // dCheck 0 none, 1 running, 2 accepted (tiles relabelled to the measured offset), 3 rejected (all discarded).
  volatile int dCheck; pthread_t dCheckT; int dCheckRodando;
  char dFonte[FONTEFALHA_CHAVE];           // the chosen copy's source (its verdict is remembered)
  volatile int parar;
  int vivo;
} Job;

static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t muFolga = PTHREAD_MUTEX_INITIALIZER;
static Job *job;
static int estado = M_PARADO;
int miniatura_direto(void) {
  pthread_mutex_lock(&mu);
  int direct = job && job->direto;
  pthread_mutex_unlock(&mu);
  return direct;
}
static unsigned geracao;
static char urlAtual[4096];
static volatile long posMs, playheadMs;
static volatile int scrubbing, bufferando;
static char origem[48] = "";      // what the thumbnails came from (log + label)
static volatile unsigned avisoPronto;
static volatile int evRebuffer, evErro;   // playback events during the title (summary)
static MiniProgress progresso;
static int motivoSem;            // M_FALHOU because: 0 the video would not open, 1 no smaller copy usable
   // generation whose set just completed (pPedir shows the notice)

// Copy-first mapping (identity when the direct engine reads the playing file itself).
static long dParaCopia(const Job *j, long t) { return j->dCopia ? (long)(j->dA * t + j->dB) : t; }
static long dParaPlay(const Job *j, long p) { return j->dCopia ? (long)((p - j->dB) / j->dA) : p; }
static void dMapear(const Job *j, long *t, int n) { if (j->dCopia) for (int i = 0; i < n; i++) t[i] = dParaCopia(j, t[i]); }
// Called with mu held. Direct image ownership is independent of nominal grid positions.
static int diretoCoverageComplete(const Job *j) { return j->direto && mini_coverage_complete(&j->coverage); }
static void diretoRebuildCoverage(Job *j) {
  for (int i = 0; i < j->n; i++) j->framePts[i] = j->tiles[i].jpg ? j->tiles[i].pts : -1;
  mini_coverage_rebuild(&j->coverage, j->framePts, j->n);
}
static int diretoGuardarFrame(Job *j, long pts, unsigned char **jpg, int n) {
  if (!*jpg || pts < 0 || pts > j->dur) return -1;
  int owner = mini_coverage_find_frame(j->framePts, j->n, pts);
  if (owner < 0) {
    for (int i = 0; i < j->n; i++) if (!j->tiles[i].jpg) { owner = i; break; }
    if (owner < 0) return -1;
    Tile *t = &j->tiles[owner];
    t->jpg = *jpg; t->n = n; t->pts = pts; t->revision++;
    *jpg = NULL; j->framePts[owner] = pts; j->feitos++;
    if (!j->primeiro) j->primeiro = SDL_GetTicks();
    if (job == j) estado = M_PRONTO;
  }
  mini_coverage_offer_frame(&j->coverage, j->framePts, j->n, owner);
  return owner;
}
static void diretoCoberturaRelatorio(Job *j) {
  pthread_mutex_lock(&mu);
  int covered = mini_coverage_covered_count(&j->coverage), pending = 0, failed = 0, unavailable = 0;
  for (int k = 0; k < j->n; k++) {
    pending += j->slotState[k] == MINI_COVER_PENDING;
    failed += j->slotState[k] == MINI_COVER_FAILED;
    unavailable += j->slotState[k] == MINI_COVER_UNAVAILABLE;
  }
  int complete = diretoCoverageComplete(j);
  pthread_mutex_unlock(&mu); // workers are joined; do not hold the UI mutex while printing a diagnostic ledger
  printf("[miniatura] coverage: targets=%d covered=%d images=%d pending=%d failed=%d unavailable=%d complete=%d\n",
         j->n, covered, j->feitos, pending, failed, unavailable, complete);
  if (j->auditarRede || j->coverageAudit) for (int k = 0; k < j->n; k++) {
    int owner = j->slotFrame[k];
    long pts = owner >= 0 && owner < j->n && j->tiles[owner].jpg ? j->tiles[owner].pts : -1;
    long target = (long)mini_coverage_target_ms(&j->coverage, k);
    printf("[miniatura] coverage-slot: targetMs=%ld keyMs=%ld frame=%d ptsMs=%ld driftMs=%ld state=%d\n",
           target, j->slotKey[k], owner, pts, pts >= 0 ? pts - target : -1, j->slotState[k]);
  }
  fflush(stdout);
}

// ---- JNI (MiniaturaMotor.kt) ----
static jclass cls;
static jmethodID jDolbyGpu,jDolbyGpuClose;
static jmethodID jLanesProprias;   // optional: storage host with its own thumbnail slots (MediaHttpBudget.EXTRA)
static jmethodID jAbrir, jInfo, jQuadro, jExato, jSoltar, jFechar, jLidos, jRitmo, jFalhaRede, jTela, jPreparar, jCont, jAlvo, jLote,
                 jDiag, jConfig, jPerdidos, jAbrirDireto, jQuadroDireto, jKeyframe, jDiagDireto, jHeap, jAudio, jCodecCpu, jPacoteCpu, jPrepDec,
                 jPixelsDireto, jPixelsDimensoes, jDecPronto, jPrepararFuturo, jPrepararSondas, jPipelineStart, jPipelineSummary, jPipelineState, jPipelineStop, jHeadroom, jSlotKeyframe, jSeekRecente;
static void prepararPixelsDireto(Job *j, long long h, int k, long pts);

static int jniPreparar(void) {
  if (cls) return 1;
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  jobject act = (jobject)SDL_AndroidGetActivity();
  if (!env || !act) return 0;
  jclass ac = (*env)->GetObjectClass(env, act);
  jmethodID gl = (*env)->GetMethodID(env, ac, "getClassLoader", "()Ljava/lang/ClassLoader;");
  jobject loader = (*env)->CallObjectMethod(env, act, gl);
  jclass lc = (*env)->FindClass(env, "java/lang/ClassLoader");
  jmethodID lcm = (*env)->GetMethodID(env, lc, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
  jstring nome = (*env)->NewStringUTF(env, "space.nuvio.nativelegacy.MiniaturaMotor");
  jclass c = (jclass)(*env)->CallObjectMethod(env, loader, lcm, nome);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); c = NULL; }
  if (c) {
    jDolbyGpu=(*env)->GetStaticMethodID(env,c,"converterDolbyGpu","(IILjava/nio/ByteBuffer;Ljava/nio/ByteBuffer;Ljava/nio/ByteBuffer;)I");
    jDolbyGpuClose=(*env)->GetStaticMethodID(env,c,"fecharDolbyGpu","()V");
    jAbrir = (*env)->GetStaticMethodID(env, c, "abrir", "(Ljava/lang/String;Ljava/lang/String;Z)J");
    jInfo = (*env)->GetStaticMethodID(env, c, "info", "(J)[J");
    jQuadro = (*env)->GetStaticMethodID(env, c, "quadro", "(JJZZ)[B");
    jExato = (*env)->GetStaticMethodID(env, c, "quadroExato", "(JJZ)[B");
    jSoltar = (*env)->GetStaticMethodID(env, c, "soltarDecoder", "(J)V");
    jFechar = (*env)->GetStaticMethodID(env, c, "fechar", "(J)V");
    jLidos = (*env)->GetStaticMethodID(env, c, "lidos", "(J)J");
    jRitmo = (*env)->GetStaticMethodID(env, c, "ritmo", "()J");
    jFalhaRede = (*env)->GetStaticMethodID(env, c, "falhaRede", "(J)I");
    jTela = (*env)->GetStaticMethodID(env, c, "tela", "()[B");
    jPreparar = (*env)->GetStaticMethodID(env, c, "preparar", "(J[J)I");
    jPrepararFuturo = (*env)->GetStaticMethodID(env, c, "preparar", "(J[J[J)I");
    jPrepararSondas = (*env)->GetStaticMethodID(env, c, "prepararComSondas", "(J[J[J[J)I");
    jCont = (*env)->GetStaticMethodID(env, c, "contadores", "()[J");
    jAlvo = (*env)->GetStaticMethodID(env, c, "alvo", "(J)Ljava/lang/String;");
    jLote = (*env)->GetStaticMethodID(env, c, "lote", "(J)I");
    jDiag = (*env)->GetStaticMethodID(env, c, "diag", "(Z)Ljava/lang/String;");
    jConfig = (*env)->GetStaticMethodID(env, c, "configurar", "(II)V");
    jPipelineStop = (*env)->GetStaticMethodID(env, c, "pipelineStop", "()V");
    jPipelineStart = (*env)->GetStaticMethodID(env, c, "pipelineStart", "(J)V");
    jPipelineSummary = (*env)->GetStaticMethodID(env, c, "pipelineSummary", "(J)Ljava/lang/String;");
    jPipelineState = (*env)->GetStaticMethodID(env, c, "pipelineState", "(IZ)V");
    jHeadroom = (*env)->GetStaticMethodID(env, c, "playbackHeadroom", "(D)I");
    jPerdidos = (*env)->GetStaticMethodID(env, c, "perdidos", "()J");
    jSeekRecente = (*env)->GetStaticMethodID(env, c, "seekRecente", "()I");
    jAbrirDireto = (*env)->GetStaticMethodID(env, c, "abrirDireto", "(Ljava/lang/String;Ljava/lang/String;)J");
    jQuadroDireto = (*env)->GetStaticMethodID(env, c, "quadroDireto", "(JJ)[B");
    jKeyframe = (*env)->GetStaticMethodID(env, c, "keyframeMs", "(JJ)J");
    jSlotKeyframe = (*env)->GetStaticMethodID(env, c, "slotKeyframeMs", "(JJ)J");
    jDiagDireto = (*env)->GetStaticMethodID(env, c, "diagDireto", "()Ljava/lang/String;");
    jHeap = (*env)->GetStaticMethodID(env, c, "heapLivreMb", "()J");
    jAudio = (*env)->GetStaticMethodID(env, c, "audioFalhas", "()J");
    jCodecCpu = (*env)->GetStaticMethodID(env, c, "codecCpu", "(J)I");
    jPacoteCpu = (*env)->GetStaticMethodID(env, c, "pacoteCpu", "(JJ)[B");
    jPrepDec = (*env)->GetStaticMethodID(env, c, "prepararDecoder", "(J)I");
    jDecPronto = (*env)->GetStaticMethodID(env, c, "decoderPronto", "(J)I");
    jPixelsDireto = (*env)->GetStaticMethodID(env, c, "pixelsDireto", "(J)Ljava/nio/ByteBuffer;");
    jPixelsDimensoes = (*env)->GetStaticMethodID(env, c, "pixelsDimensoes", "(J)I");
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);   // earlier lookups failed: the check below still sees the NULLs
    jLanesProprias = (*env)->GetStaticMethodID(env, c, "lanesProprias", "(J)I");
    if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); jLanesProprias = NULL; }
    if (!jAbrir || !jInfo || !jQuadro || !jExato || !jSoltar || !jFechar || !jLidos || !jRitmo || !jFalhaRede || !jTela || !jPreparar || !jPrepararFuturo || !jPrepararSondas || !jCont || !jAlvo || !jLote ||
        !jDiag || !jConfig || !jPipelineStart || !jPipelineStop || !jPipelineSummary || !jPipelineState || !jHeadroom || !jPerdidos || !jSeekRecente || !jAbrirDireto || !jQuadroDireto || !jKeyframe || !jSlotKeyframe || !jDiagDireto || !jHeap || !jAudio || !jCodecCpu || !jPacoteCpu || !jPrepDec || !jDecPronto || !jPixelsDireto || !jPixelsDimensoes) {
      (*env)->ExceptionClear(env);
    } else {
      cls = (jclass)(*env)->NewGlobalRef(env, c);
    }
    (*env)->DeleteLocalRef(env, c);
  }
  (*env)->DeleteLocalRef(env, nome);
  (*env)->DeleteLocalRef(env, lc);
  (*env)->DeleteLocalRef(env, loader);
  (*env)->DeleteLocalRef(env, ac);
  (*env)->DeleteLocalRef(env, act);
  if (!cls) { printf("[miniatura] engine class missing\n"); fflush(stdout); }
  return cls != NULL;
}

// `manter`: keep the socket between requests (the copy). The playing file closes it after every request.
// Synchronous: native buffers remain owned by this CPU worker throughout the GL call.
int mini_dovi_gpu_convert(int w,int h,const float *samples,const float *metadata,unsigned char *rgba) {
  if(!jniPreparar() || !jDolbyGpu)return -1;
  JNIEnv *env=(JNIEnv *)SDL_AndroidGetJNIEnv();
  jobject a=(*env)->NewDirectByteBuffer(env,(void *)samples,(jlong)w*h*12);
  jobject b=(*env)->NewDirectByteBuffer(env,(void *)metadata,4096);
  jobject c=(*env)->NewDirectByteBuffer(env,rgba,(jlong)w*h*4);
  int result=-1;
  if(a && b && c)result=(*env)->CallStaticIntMethod(env,cls,jDolbyGpu,w,h,a,b,c);
  if((*env)->ExceptionCheck(env)) {(*env)->ExceptionClear(env);result=-1;}
  if(a)(*env)->DeleteLocalRef(env,a);if(b)(*env)->DeleteLocalRef(env,b);if(c)(*env)->DeleteLocalRef(env,c);
  return result;
}
void mini_dovi_gpu_close(void) {
  if(!cls || !jDolbyGpuClose)return;
  JNIEnv *env=(JNIEnv *)SDL_AndroidGetJNIEnv();
  (*env)->CallStaticVoidMethod(env,cls,jDolbyGpuClose);
  if((*env)->ExceptionCheck(env))(*env)->ExceptionClear(env);
}
static long long mAbrir(const char *url, const char *cab, int manter) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  jstring u = (*env)->NewStringUTF(env, url), c = (*env)->NewStringUTF(env, cab ? cab : "");
  jlong h = (*env)->CallStaticLongMethod(env, cls, jAbrir, u, c, manter ? JNI_TRUE : JNI_FALSE);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); h = 0; }
  (*env)->DeleteLocalRef(env, u); (*env)->DeleteLocalRef(env, c);
  return h;
}
// [duration ms, width, height, hdr, bytes read, codec usable (H.264/HEVC/DV)]
static void mInfo(long long h, long long out[6]) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  memset(out, 0, 6 * sizeof *out);
  jlongArray a = (jlongArray)(*env)->CallStaticObjectMethod(env, cls, jInfo, (jlong)h);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return; }
  if (a) { (*env)->GetLongArrayRegion(env, a, 0, 6, (jlong *)out); (*env)->DeleteLocalRef(env, a); }
}
// Unpacks MiniaturaMotor's reply: pts (ms), signature (unit-norm, mean-removed; 0 when flat), JPEG (malloc'd).
static int desempacotar(JNIEnv *env, jbyteArray a, long *pts, float sig[SIG_N], int *temSig, unsigned char **jpg, int *nJpg);
static long mSlotKeyframe(long long h, long targetMs) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  jlong result = (*env)->CallStaticLongMethod(env, cls, jSlotKeyframe, (jlong)h, (jlong)targetMs);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return -1; }
  return (long)result;
}
// Keep the admitted payload list separate from metadata candidates when a gap
// removes a hint; a later-stage header must never become a payload by shifting.
static int resolverSondas(long long h, long *times, int count) {
  int out = 0;
  for (int i = 0; i < count; i++) {
    long key = mSlotKeyframe(h, times[i]);
    if (key == -2 && times[i] > 0) key = times[i];
    if (key < 0) continue;
    int duplicate = 0;
    for (int k = 0; k < out; k++) if (times[k] == key) { duplicate = 1; break; }
    if (!duplicate) times[out++] = key;
  }
  return out;
}
// One keyframe (closest or at/before tMs).
static int mQuadro(long long h, long tMs, int closest, int querJpeg, long *pts, float sig[SIG_N], int *temSig,
                   unsigned char **jpg, int *nJpg) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  jbyteArray a = (jbyteArray)(*env)->CallStaticObjectMethod(env, cls, jQuadro, (jlong)h, (jlong)tMs,
                                                            closest ? JNI_TRUE : JNI_FALSE, querJpeg ? JNI_TRUE : JNI_FALSE);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); a = NULL; }
  return desempacotar(env, a, pts, sig, temSig, jpg, nJpg);
}
// The frame AT tMs (decoded forward from the keyframe), signature only: the fine pass of the check.
static int mExato(long long h, long tMs, long *pts, float sig[SIG_N], int *temSig, unsigned char **jpg, int *nJpg) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  jbyteArray a = (jbyteArray)(*env)->CallStaticObjectMethod(env, cls, jExato, (jlong)h, (jlong)tMs, jpg ? JNI_TRUE : JNI_FALSE);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); a = NULL; }
  return desempacotar(env, a, pts, sig, temSig, jpg, nJpg);
}
static int desempacotar(JNIEnv *env, jbyteArray a, long *pts, float sig[SIG_N], int *temSig, unsigned char **jpg, int *nJpg) {
  if (!a) return 0;
  jsize n = (*env)->GetArrayLength(env, a);
  unsigned char *b = n >= CAB + SIG_N ? malloc((size_t)n) : NULL;   // signature-only replies carry no JPEG
  if (b) (*env)->GetByteArrayRegion(env, a, 0, n, (jbyte *)b);
  (*env)->DeleteLocalRef(env, a);
  if (!b) return 0;
  long long p = 0;
  for (int i = 0; i < 8; i++) p = (p << 8) | b[i];
  *pts = (long)p;
  double m = 0, s2 = 0;
  for (int i = 0; i < SIG_N; i++) m += b[CAB + i];
  m /= SIG_N;
  for (int i = 0; i < SIG_N; i++) { double d = b[CAB + i] - m; s2 += d * d; }
  double desvio = sqrt(s2 / SIG_N), norma = sqrt(s2);
  *temSig = desvio >= MIN_DESVIO && norma > 0;
  for (int i = 0; i < SIG_N; i++) sig[i] = *temSig ? (float)((b[CAB + i] - m) / norma) : 0.0f;
  if (jpg) {
    *nJpg = n - CAB - SIG_N;
    *jpg = *nJpg > 0 ? malloc((size_t)*nJpg) : NULL;
    if (*jpg) memcpy(*jpg, b + CAB + SIG_N, (size_t)*nJpg);
  }
  free(b);
  return 1;
}
static void mSoltar(long long h) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  (*env)->CallStaticVoidMethod(env, cls, jSoltar, (jlong)h);
  if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
}
static void mFechar(long long h) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  if (h) (*env)->CallStaticVoidMethod(env, cls, jFechar, (jlong)h);
  if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
}
static long long mLidos(long long h) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  jlong r = h ? (*env)->CallStaticLongMethod(env, cls, jLidos, (jlong)h) : 0;
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); r = 0; }
  return r;
}

static int mFalhaRede(long long h) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  jint r = h ? (*env)->CallStaticIntMethod(env, cls, jFalhaRede, (jlong)h) : 0;
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); r = 0; }
  return r;
}
// One multi-range request for the keyframes nearest these copy times (MiniaturaMotor.preparar): >= 0 fetched,
// -1 batches not possible for this file, -2 refused.
static int mPreparar(long long h, const long *tempos, int n) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  jlongArray a = (*env)->NewLongArray(env, n);
  if (!a) return -1;
  jlong v[LOTE];
  for (int i = 0; i < n && i < LOTE; i++) v[i] = tempos[i];
  (*env)->SetLongArrayRegion(env, a, 0, n, v);
  jint r = (*env)->CallStaticIntMethod(env, cls, jPreparar, (jlong)h, a);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); r = -1; }
  (*env)->DeleteLocalRef(env, a);
  return r;
}
static int mPrepararFuturo(long long h, const long *tempos, int n, const long *future, int count) {
  if (!count) return mPreparar(h, tempos, n);
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  jlongArray a = (*env)->NewLongArray(env, n), b = (*env)->NewLongArray(env, count);
  if (!a || !b) {
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
    if (a) (*env)->DeleteLocalRef(env, a);
    if (b) (*env)->DeleteLocalRef(env, b);
    return -1;
  }
  jlong values[LOTE];
  for (int i = 0; i < n; i++) values[i] = tempos[i];
  (*env)->SetLongArrayRegion(env, a, 0, n, values);
  for (int i = 0; i < count; i++) values[i] = future[i];
  (*env)->SetLongArrayRegion(env, b, 0, count, values);
  jint r = (*env)->CallStaticIntMethod(env, cls, jPrepararFuturo, (jlong)h, a, b);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); r = -1; }
  (*env)->DeleteLocalRef(env, a); (*env)->DeleteLocalRef(env, b);
  return r;
}
static int mPrepararSondas(long long h, const long *tempos, int n, const long *future, int count,
                          const long *headers, int nHeaders) {
  if (!nHeaders) return mPrepararFuturo(h, tempos, n, future, count);
  if (n < 0 || n > LOTE || count < 0 || count > 32 || nHeaders < 0 || nHeaders > 64) return -1;
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  jlongArray a = (*env)->NewLongArray(env, n), b = (*env)->NewLongArray(env, count), c = (*env)->NewLongArray(env, nHeaders);
  if (!a || !b || !c) {
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
    if (a) (*env)->DeleteLocalRef(env, a);
    if (b) (*env)->DeleteLocalRef(env, b);
    if (c) (*env)->DeleteLocalRef(env, c);
    return -1;
  }
  jlong values[64];
  for (int i = 0; i < n; i++) values[i] = tempos[i];
  (*env)->SetLongArrayRegion(env, a, 0, n, values);
  for (int i = 0; i < count; i++) values[i] = future[i];
  (*env)->SetLongArrayRegion(env, b, 0, count, values);
  for (int i = 0; i < nHeaders; i++) values[i] = headers[i];
  (*env)->SetLongArrayRegion(env, c, 0, nHeaders, values);
  jint r = (*env)->CallStaticIntMethod(env, cls, jPrepararSondas, (jlong)h, a, b, c);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); r = -1; }
  (*env)->DeleteLocalRef(env, a); (*env)->DeleteLocalRef(env, b); (*env)->DeleteLocalRef(env, c);
  return r;
}
static int bitsDebug(void);

// 1 when this source's thumbnails run on their own parallel lanes (a storage host that refused multi-range):
// then the playhead batches also get look-ahead hints, so the next batch is already downloading.
static int mLanesProprias(long long h) {
  if (!jLanesProprias) return 0;
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  jint r = (*env)->CallStaticIntMethod(env, cls, jLanesProprias, (jlong)h);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); r = 0; }
  return r == 1;
}
// MiniaturaMotor.contadores() since the app started; the summary takes deltas.
#define NCONT 12
static int mLote(long long h) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  jint r = (*env)->CallStaticIntMethod(env, cls, jLote, (jlong)h);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); r = 8; }
  return r < 2 ? 2 : r > LOTE ? LOTE : r;
}
static void mContadores(long long out[NCONT]) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  memset(out, 0, NCONT * sizeof *out);
  jlongArray a = (jlongArray)(*env)->CallStaticObjectMethod(env, cls, jCont);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return; }
  if (a) { (*env)->GetLongArrayRegion(env, a, 0, NCONT, (jlong *)out); (*env)->DeleteLocalRef(env, a); }
}
// The copy's link after the add-on's redirect, so further sessions do not make the add-on mint another debrid link.
static void mAlvo(long long h, const char *reserva, char *out, size_t n) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  snprintf(out, n, "%s", reserva);
  jstring sj = (jstring)(*env)->CallStaticObjectMethod(env, cls, jAlvo, (jlong)h);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return; }
  if (sj) {
    const char *c = (*env)->GetStringUTFChars(env, sj, NULL);
    if (c && c[0]) snprintf(out, n, "%s", c);
    if (c) (*env)->ReleaseStringUTFChars(env, sj, c);
    (*env)->DeleteLocalRef(env, sj);
  }
}
// The engine's read diagnostics since the last reset (zerar = reset after reading), into out.
static void mDiag(int zerar, char *out, size_t n) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  if (out && n) out[0] = 0;
  jstring sj = (jstring)(*env)->CallStaticObjectMethod(env, cls, jDiag, zerar ? JNI_TRUE : JNI_FALSE);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return; }
  if (!sj) return;
  const char *c = (*env)->GetStringUTFChars(env, sj, NULL);
  if (c && out) snprintf(out, n, "%s", c);
  if (c) (*env)->ReleaseStringUTFChars(env, sj, c);
  (*env)->DeleteLocalRef(env, sj);
}
static void mDiagDireto(char *out, size_t n) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  out[0] = 0;
  jstring sj = (jstring)(*env)->CallStaticObjectMethod(env, cls, jDiagDireto);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return; }
  if (!sj) return;
  const char *c = (*env)->GetStringUTFChars(env, sj, NULL);
  if (c) { snprintf(out, n, "%s", c); (*env)->ReleaseStringUTFChars(env, sj, c); }
  (*env)->DeleteLocalRef(env, sj);
}
// Direct mode (MiniaturaMotor.abrirDireto/quadroDireto): the playing file itself, hardware decoder + GPU resize.
static long long mAbrirDireto(const char *url, const char *cab) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  jstring u = (*env)->NewStringUTF(env, url), c = (*env)->NewStringUTF(env, cab ? cab : "");
  jlong h = (*env)->CallStaticLongMethod(env, cls, jAbrirDireto, u, c);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); h = 0; }
  (*env)->DeleteLocalRef(env, u); (*env)->DeleteLocalRef(env, c);
  return h;
}
static long mKeyframe(long long h, long tMs) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  jlong r = (*env)->CallStaticLongMethod(env, cls, jKeyframe, (jlong)h, (jlong)tMs);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); r = -1; }
  return (long)r;
}
static int desempacotar(JNIEnv *env, jbyteArray a, long *pts, float sig[SIG_N], int *temSig, unsigned char **jpg, int *nJpg);
static int mQuadroDireto(long long h, long tMs, long *pts, unsigned char **jpg, int *nJpg) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  float sig[SIG_N]; int ok;
  jbyteArray a = (jbyteArray)(*env)->CallStaticObjectMethod(env, cls, jQuadroDireto, (jlong)h, (jlong)tMs);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); a = NULL; }
  return desempacotar(env, a, pts, sig, &ok, jpg, nJpg);
}
// Free Java heap of the app (MB); the player's samples share it. Below HEAP_MIN_MB thumbnail work waits.
#define HEAP_MIN_MB 64
static long mHeapLivre(void) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  jlong r = (*env)->CallStaticLongMethod(env, cls, jHeap);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); r = 1000; }
  return (long)r;
}
#ifdef NV_MINI_FFMPEG
// CPU route: 1 HEVC, 2 H.264, 0 not possible for this source (MiniaturaMotor.codecCpu).
static int mCodecCpu(long long h) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  jint r = (*env)->CallStaticIntMethod(env, cls, jCodecCpu, (jlong)h);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); r = 0; }
  return r;
}
// The keyframe for tMs as an Annex B access unit with its parameter sets (malloc'd in *pk), *pts its time.
static int mPacoteCpu(long long h, long tMs, long *pts, unsigned char **pk, int *n) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  *pk = NULL; *n = 0;
  jbyteArray a = (jbyteArray)(*env)->CallStaticObjectMethod(env, cls, jPacoteCpu, (jlong)h, (jlong)tMs);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); a = NULL; }
  if (!a) return 0;
  jsize t = (*env)->GetArrayLength(env, a);
  unsigned char cab[8];
  if (t > 8) {
    (*env)->GetByteArrayRegion(env, a, 0, 8, (jbyte *)cab);
    *pk = malloc((size_t)(t - 8));
    if (*pk) { (*env)->GetByteArrayRegion(env, a, 8, t - 8, (jbyte *)*pk); *n = t - 8; }
  }
  (*env)->DeleteLocalRef(env, a);
  if (!*pk) return 0;
  long long v = 0;
  for (int i = 0; i < 8; i++) v = (v << 8) | cab[i];
  *pts = (long)v;
  return 1;
}
#endif
// The direct mode's hardware decoder (+ GPU step), created now (MiniaturaMotor.prepararDecoder): 1 = ready.
static int mPrepararDecoder(long long h) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  jint r = (*env)->CallStaticIntMethod(env, cls, jPrepDec, (jlong)h);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); r = 0; }
  return r;
}
static int mDecoderPronto(long long h) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  jint r = (*env)->CallStaticIntMethod(env, cls, jDecPronto, (jlong)h);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); r = 0; }
  return r;
}
static long long mAudioFalhas(void) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  jlong r = (*env)->CallStaticLongMethod(env, cls, jAudio);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); r = 0; }
  return r;
}
static long long mPerdidos(void) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  jlong r = (*env)->CallStaticLongMethod(env, cls, jPerdidos);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); r = 0; }
  return r;
}
static int mSeekRecente(void) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  jint r = (*env)->CallStaticIntMethod(env, cls, jSeekRecente);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); r = 0; }
  return r != 0;
}
// debug.nuvio.miniatura (adb shell setprop): bit 1 = force the direct playing-file path, bit 2 = old closest-cue
// prefetch (A/B of the read fix). Read when a title starts and handed to the engine.
static int bitsDebug(void) {
  // Retained shipping profile: overlap, health-gated fast pacing, bounded mixed probes,
  // two payload exchanges, shared transport, lookahead and measured memory/NAL fixes.
  // One-shot header diagnostic only: ignore JPEG loading while retaining saved
  // sets. The app supplies this local marker path; no shell property override.
  const char *probe = getenv("NUVIO_THUMBNAIL_SIZE_PROBE_MARKER");
  const char *cold = getenv("NUVIO_AUDIO_TRACE_COLD_MARKER");
  const char *control = getenv("NUVIO_AUDIO_TRACE_CONTROL_MARKER");
  const char *current = getenv("NUVIO_THUMB_CURRENT_RANGES");
  const char *dense = getenv("NUVIO_THUMB_DENSE_HEADER_HINTS");
  return 260096 | (probe && probe[0] && access(probe, F_OK) == 0 ? 256 : 0) |
      (cold && cold[0] && access(cold, F_OK) == 0 ? 256 : 0) |
      (control && control[0] && access(control, F_OK) == 0 ? 1024 : 0) |
      (current && !strcmp(current, "1") ? 262144 : 0) |
      (dense && !strcmp(dense, "1") ? 524288 : 0);
}

static void mConfigurar(int bits) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  (*env)->CallStaticVoidMethod(env, cls, jConfig, (jint)bits, (jint)ajustes_miniatura_largura());
  if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
}
static void mPipelineState(int state, int begin) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  if (!env || !cls || !jPipelineState) return;
  (*env)->CallStaticVoidMethod(env, cls, jPipelineState, (jint)state, begin ? JNI_TRUE : JNI_FALSE);
  if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
}
static void mPipelineSummary(Job *j, Uint32 t0) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  if (!env || !cls) return;
  jstring value = (jstring)(*env)->CallStaticObjectMethod(env, cls, jPipelineSummary, (jlong)(SDL_GetTicks() - t0));
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return; }
  if (value) {
    const char *text = (*env)->GetStringUTFChars(env, value, NULL);
    if (text) { printf("[miniatura] SUMMARY-TIMELINE %s | %s\n", j->titulo, text); (*env)->ReleaseStringUTFChars(env, value, text); }
    (*env)->DeleteLocalRef(env, value);
  }
}
static long long mRitmo(void) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  jlong r = (*env)->CallStaticLongMethod(env, cls, jRitmo);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); r = 0; }
  return r;
}

static long memDisponivelKB(void) {
  FILE *f = fopen("/proc/meminfo", "r");
  char l[128]; long v = -1;
  if (!f) return -1;
  while (fgets(l, sizeof l, f)) if (sscanf(l, "MemAvailable: %ld kB", &v) == 1) break;
  fclose(f);
  return v;
}

static int parou(const Job *j) { return j->parar || j->pararFaixas; }
static long mHeapLivre(void);
// The app's Java heap is tight: wait (the player's sample buffer must never trigger a blocking GC because of us).
static int heapApertado(Job *j) {
  long h = mHeapLivre();
  if (h < j->heapMin) j->heapMin = h;
  if (h >= HEAP_MIN_MB) return 0;
  if (j->esperasHeap++ % 20 == 0) { printf("[miniatura] waiting: app heap has %ld MB free (< %d)\n", h, HEAP_MIN_MB); fflush(stdout); }
  return 1;
}
static void dormir(const Job *j, int ms) { for (int t = 0; t < ms && !parou(j); t += 50) usleep(50000); }

// The player first: nothing is read while it rebuffers, reconnects or has failed. Measured 22/10 (Dune 2, 4K
// through a StremThru proxy): a burst of thumbnail reads on the playing link and the player's reconnects were
// refused together (ConnectException) until it gave up.
// Seconds the player has buffered ahead, < 0 when it does not say. On Android VOD video_buffer_fim() is live-only
// (always 0), so this gate never fired there before 23/10; the player's own stats carry the real figure. A 4K remux
// sits at ~10 s (the player's 80 MB sample cap, measured 23/10), so the floor is absolute and low, not 20 s.
// Kotlin publishes fresh, byte-aware admission for the active player. The relative ceiling below is only the
// legacy/unknown-sample fallback: a high-bitrate player's full byte buffer can hold fewer seconds than a startup peak.
#define FOLGA_MIN_S 4.0
static double folgaSeg(Job *j) {
  VideoStats st;
  double f;
  video_estatisticas(&st);
  if (st.bufferSeg >= 0) f = st.bufferSeg;
  else { double bf = video_buffer_fim(); f = bf > 0.5 ? bf - video_pos() : -1.0; }
  pthread_mutex_lock(&muFolga);
  if (f >= 0 && (f >= j->folgaTopo || SDL_GetTicks() - j->folgaTopoEm > 60000u)) { j->folgaTopo = f; j->folgaTopoEm = SDL_GetTicks(); }
  pthread_mutex_unlock(&muFolga);
  return f;
}
// "Comfortable": at least `alvo` seconds, or 70 % of the ceiling when the ceiling itself is lower.
static double folgaQuer(const Job *j, double alvo, double frac) {
  pthread_mutex_lock(&muFolga);
  double topo = j->folgaTopo;
  pthread_mutex_unlock(&muFolga);
  return topo > 0 && topo * frac < alvo ? topo * frac : alvo;
}
// -1 means there has never been a health sample; stale/unsafe samples return 0 (never a fallback bypass).
static int mHeadroom(const Job *j, double alvo) {
  if (!j->sharedHeadroom) return -1;
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  if (!env || !cls || !jHeadroom) return 0;
  int ready = (*env)->CallStaticIntMethod(env, cls, jHeadroom, (jdouble)alvo);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return 0; }
  return ready;
}
static int folgaBaixa(Job *j) {
  double f = folgaSeg(j), pos = video_pos(), dur = video_duracao();
  double quer = folgaQuer(j, FOLGA_MIN_S, 0.5);
  if (dur > 0 && dur - pos - 1.0 < quer) quer = dur - pos - 1.0;
  // Keep the existing end-of-file clamp: fewer than two seconds can remain in the entire source.
  if (dur <= 0 || dur - pos - 1.0 >= 2.0) {
    int shared = mHeadroom(j, FOLGA_MIN_S);
    if (shared >= 0) return !shared;
  }
  return f >= 0 && quer > 0 && f < quer;
}
static void dormirGate(const Job *j, int ms) {
  mPipelineState(3, 1); // PLAYBACK_GATE: native admission waits used to appear as scheduler_idle.
  dormir(j, ms);
  mPipelineState(3, 0);
}
static void esperarPlayer(Job *j) {
  int avisou = 0;
  while (!parou(j) && (bufferando || video_reconectando() || video_falhou() || folgaBaixa(j))) {
    if (!avisou) { printf("[miniatura] waiting: the player is buffering/reconnecting\n"); fflush(stdout); avisou = 1; }
    dormirGate(j, 1000);
  }
}
// A failed frame: when a READ failed (the engine says so), back off 5 s, 10 s, 20 s and end the job on the 4th in a
// row (the link is refusing us). A frame that only would not DECODE is skipped at once; 3 of those in a row end it
// (22/10: a copy whose frames all threw made decoders in a burst next to the player, whose own decoder then failed).
static __thread int falhasSeguidas, decodSeguidas;   // per lane
static int mFalhaRede(long long h);
static int falhou(const Job *j, int ok, long long h) {
  if (ok) { falhasSeguidas = 0; decodSeguidas = 0; return 0; }
  int rede = mFalhaRede(h);
  if (rede == 2) {                      // refused or silent: the debrid is at its connection limit; the player comes first
    printf("[miniatura] stopping: the source refused/stalled a connection (protecting playback)\n"); fflush(stdout);
    return 1;
  }
  if (!rede) {
    if (++decodSeguidas >= 3) { printf("[miniatura] stopping: %d frames in a row would not decode\n", decodSeguidas); fflush(stdout); return 1; }
    return 0;
  }
  if (++falhasSeguidas >= 4) {
    printf("[miniatura] stopping: %d reads in a row failed (source refusing)\n", falhasSeguidas); fflush(stdout);
    return 1;
  }
  printf("[miniatura] read failed, backing off %d s\n", 5 << (falhasSeguidas - 1)); fflush(stdout);
  dormir(j, 5000 << (falhasSeguidas - 1));
  return 0;
}

// ---- choosing the smaller copy (OpenSeek altsource.py, on the list the app already has) ----
static const char *EDICOES[] = { "extended", "directors", "director", "theatrical", "unrated", "imax", "remastered",
                                 "final cut", "special edition", "uncut", "redux", "open matte", "3d", "bd3d", "sbs", "hsbs" };
#define N_EDICOES (int)(sizeof EDICOES / sizeof *EDICOES)
static unsigned edicoes(const Stream *s) {
  char t[1200]; size_t k = 0;
  const char *fontes[] = { s->arquivo, s->rotulo, s->descricao };
  for (int f = 0; f < 3; f++)
    for (const char *p = fontes[f]; *p && k < sizeof t - 2; p++)
      t[k++] = (*p == '.' || *p == '_' || *p == '-' || *p == '\n') ? ' ' : (char)tolower((unsigned char)*p);
  t[k] = 0;
  unsigned m = 0;
  for (int i = 0; i < N_EDICOES; i++) {
    size_t n = strlen(EDICOES[i]);
    for (const char *p = t; (p = strstr(p, EDICOES[i])); p += n) {
      int ini = p == t || !isalnum((unsigned char)p[-1]), fim = !isalnum((unsigned char)p[n]);
      if (ini && fim) { m |= 1u << i; break; }
    }
  }
  return m;
}
static unsigned long long tamanho(const Stream *s) {
  return s->tamanhoBytes ? s->tamanhoBytes : (unsigned long long)s->tamanhoMB << 20;
}
// Size first: the cheapest copy that can be the same video, with guards (same edition words, cached, no samples,
// a resolution below the playing file's, or the same one at <= half its size, the 1080p 0.80 rule, bitrate floors).
// Up to TENTATIVAS, smallest first; unknown sizes last. `motivos` gets the counts behind the choice.
static int escolherCopias(const Stream *play, Copia *out, long durMs, char *motivos, size_t nmot) {
  enum { R_CACHE, R_RES, R_EDICAO, R_AMOSTRA, R_REPETIDA, R_BITRATE, R_1080, R_PESADA, R_N };
  static const char *NOMES[R_N] = { "not-cached", "resolution", "edition", "sample", "duplicate", "bitrate", "1080-rule", "heavy" };
  int rej[R_N] = {0}, nv = 0, cap = stream_n() > 0 ? stream_n() : 1;
  int *v = malloc(sizeof(int) * (size_t)cap);
  unsigned edA = edicoes(play);
  unsigned long long tamA = tamanho(play);
  if (!v) return 0;
  for (int i = 0; i < stream_n(); i++) {
    const Stream *c = stream_item(i);
    if (!c || i == stream_atual() || c->naoVideo || !c->url[0] || !strcmp(c->url, play->url)) continue;
    // Never start a download for thumbnails: a link known not cached, or a debrid RESOLVE link not declared cached
    // (opening it can make the service fetch the torrent). A plain file link (PenguPlay/R2, any direct HTTP host)
    // has no cache state and is read like the playing one (06/11/2026: all 70 PenguPlay copies were "not-cached").
    int resolve = strstr(c->url, "/resolve/") || strstr(c->url, "/playback/") || strstr(c->url, "streaming_provider") ||
                  strstr(c->url, "/debrid/") || !c->url[0];
    if (c->foraCache || (resolve && !(c->emCache || c->cacheNativo))) { rej[R_CACHE]++; continue; }
    unsigned long long tc = tamanho(c);
    int resOk = c->altura >= 480 && c->altura <= 1080 &&
                (play->altura <= 0 || c->altura < play->altura ||
                 (c->altura == play->altura && tc && tamA && tc <= (unsigned long long)(tamA * RAZAO_MESMA_RES)));
    if (!resOk) { rej[R_RES]++; continue; }
    if (edicoes(c) != edA) { rej[R_EDICAO]++; continue; }
    // Sample clips (22/10: the two "copies" of a 21.6 min episode were 30 s samples): by name, or under 50 MB.
    // A bigger sample is still caught by the runtime check, from the header alone (~1 s, ~0.4 MB).
    if (strcasestr(c->arquivo, "sample") || strcasestr(c->rotulo, "sample") || (tc && tc < (50ULL << 20))) { rej[R_AMOSTRA]++; continue; }
    if (tc && durMs > 60000) {
      double mbps = tc * 8.0 / (durMs / 1000.0) / 1e6;
      double piso = c->altura >= 1080 ? PISO_MBPS_1080 : c->altura >= 720 ? PISO_MBPS_720 : PISO_MBPS_SD;
      if (mbps < piso) { rej[R_BITRATE]++; continue; }
    }
    int repetida = 0;                   // the same file from another add-on: checking it twice is pure waste
    for (int k = 0; k < nv && !repetida; k++) {
      const Stream *o = stream_item(v[k]);
      repetida = (tc && tc == tamanho(o)) || (c->arquivo[0] && !strcmp(c->arquivo, o->arquivo));
    }
    if (repetida) { rej[R_REPETIDA]++; continue; }
    v[nv++] = i;
  }
  unsigned long long min720 = 0;
  for (int k = 0; k < nv; k++) {
    const Stream *c = stream_item(v[k]);
    if (c->altura == 720 && tamanho(c) && (!min720 || tamanho(c) < min720)) min720 = tamanho(c);
  }
  // 1080p copies the 0.80 rule sets aside are BACKUPS, tried after every smaller copy (06/11/2026: Dune on PenguPlay,
  // six 720p copies dead or not seekable while twelve 1080p copies were never tried). A heavy copy (over PESADA_MBPS,
  // a disc remux) stays out: its keyframes cost nearly what the 4K file's do.
  int m = 0, nr = 0, *reserva = malloc(sizeof(int) * (size_t)cap);
  if (!reserva) { free(v); return 0; }
  for (int k = 0; k < nv; k++) {
    const Stream *c = stream_item(v[k]);
    unsigned long long tc = tamanho(c);
    if (c->altura >= 1080 && tc && durMs > 60000 && tc * 8.0 / (durMs / 1000.0) / 1e6 > PESADA_MBPS) { rej[R_PESADA]++; continue; }
    if (c->altura >= 1080 && min720 && (!tc || tc > (unsigned long long)(min720 * RAZAO_1080))) { rej[R_1080]++; reserva[nr++] = v[k]; continue; }
    v[m++] = v[k];
  }
  nv = m;
  for (int lista = 0; lista < 2; lista++) {   // smallest first, unknown sizes last (insertion sort: lists are short)
    int *w = lista ? reserva : v, nw = lista ? nr : nv;
    for (int x = 1; x < nw; x++) {
      int y = x, cur = w[x];
      unsigned long long tcur = tamanho(stream_item(cur));
      while (y > 0) {
        unsigned long long tp = tamanho(stream_item(w[y - 1]));
        if (!(tcur && (!tp || tcur < tp))) break;
        w[y] = w[y - 1]; y--;
      }
      w[y] = cur;
    }
  }
  for (int k = 0; k < nr; k++) v[nv++] = reserva[k];
  free(reserva);
  int n = nv < TENTATIVAS ? nv : TENTATIVAS;
  for (int k = 0; k < n; k++) {
    const Stream *c = stream_item(v[k]);
    snprintf(out[k].url, sizeof out[k].url, "%s", c->url);
    snprintf(out[k].cab, sizeof out[k].cab, "%s", c->cabecalhos);
    out[k].altura = c->altura;
    out[k].tamanho = tamanho(c);
    snprintf(out[k].nome, sizeof out[k].nome, "%s", c->arquivo[0] ? c->arquivo : c->rotulo);
    snprintf(out[k].fonte, sizeof out[k].fonte, "%s", c->addonChave[0] ? c->addonChave : "?");
  }
  size_t o = (size_t)snprintf(motivos, nmot, "eligible %d (trying %d, %d of them 1080p backups), rejected:", nv, n, nr);
  for (int r = 0; r < R_N && o < nmot; r++) if (rej[r] && r != R_1080) o += (size_t)snprintf(motivos + o, nmot - o, " %s %d", NOMES[r], rej[r]);
  free(v);
  return n;
}

// ---- the timeline check (OpenSeek timeline.verify_alignment) ----
typedef struct { double pa, pb; } Par;

static int cmpDouble(const void *x, const void *y) { double a = *(const double *)x, b = *(const double *)y; return (a > b) - (a < b); }
static double mediana(double *v, int n) { qsort(v, (size_t)n, sizeof *v, cmpDouble); return n % 2 ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2; }

// Best (a, b) over the speed hypotheses (same speed; the duration ratio when it could be a real speed change).
static int hipotese(const Par *m, int n, double razao, double *a, double *b) {
  double slopes[2] = { 1.0, razao }, melhorRms = 1e300, *bs = malloc(sizeof(double) * (size_t)(n ? n : 1));
  int ns = fabs(razao - 1.0) >= 0.01 ? 2 : 1;
  if (n < 2 || !bs) { free(bs); return 0; }
  for (int h = 0; h < ns; h++) {
    for (int i = 0; i < n; i++) bs[i] = m[i].pb - slopes[h] * m[i].pa;
    double med = mediana(bs, n), rms = 0;
    for (int i = 0; i < n; i++) rms += (m[i].pb - slopes[h] * m[i].pa - med) * (m[i].pb - slopes[h] * m[i].pa - med);
    rms = sqrt(rms / n);
    if (rms < melhorRms) { melhorRms = rms; *a = slopes[h]; *b = med; }
  }
  free(bs);
  return 1;
}


// Room for the player before each 4K read: at least 8 s buffered ahead (when the player reports it).
static void esperarFolga(Job *j) {
  Uint32 inicio = SDL_GetTicks();
  int admitido = 0;
  for (int t = 0; t < 30 && !parou(j); t++) {
    double f = folgaSeg(j);
    int shared = mHeadroom(j, 6.0);
    // at least 6 s, or half the player's ceiling (0.7 of it held a 4K remux at its 7-9 s for 15 s per batch, 23/10)
    if (shared > 0 || (shared < 0 && (f < 0 || f >= folgaQuer(j, 6.0, 0.5)))) { admitido = 1; break; }
    dormirGate(j, 500);
  }
  Uint32 espera = SDL_GetTicks() - inicio;
  if (espera >= 100) { printf("[miniatura] headroom-gate: batch waitMs=%u admitted=%d cancelled=%d shared=%d\n", espera, admitido, parou(j), j->sharedHeadroom); fflush(stdout); }
}

// Fine pass (OpenSeek timeline._refine): for up to 8 spread anchors, the copy's frame at EXACT times around the
// coarse prediction, the similarity peak by a parabola; keyframe spacing no longer limits the offset. Copy only.
#define REFINAR 8
#define MIN_REFINADOS 5
static int refinar(Job *j, long long hB, const Ancora *anc, const Par *in, int ni, double a, double b, long durA,
                   double *b2, double *erro, int *nref, double minCorr) {
  static const long DES[5] = { -2000, -1000, 0, 1000, 2000 };
  Par ord[N_ANCORAS], ref[REFINAR];
  int n = 0;
  float sb[SIG_N];
  memcpy(ord, in, sizeof(Par) * (size_t)ni);
  for (int x = 0; x < ni; x++) for (int y = x + 1; y < ni; y++) if (ord[y].pa < ord[x].pa) { Par t = ord[x]; ord[x] = ord[y]; ord[y] = t; }
  int passo = ni / REFINAR > 0 ? ni / REFINAR : 1;
  for (int q = 0; q < ni && n < REFINAR && !parou(j); q += passo) {
    const float *sa = NULL;
    for (int k = 0; k < N_ANCORAS; k++) if (anc[k].estado == 1 && anc[k].ok && anc[k].pA == (long)ord[q].pa) sa = anc[k].sig;
    if (!sa) continue;
    double cs[5], ps[5]; int nc = 0;
    for (int d = 0; d < 5; d++) {
      long t = (long)(a * ord[q].pa + b) + DES[d], p; int ok;
      if (t < 0) t = 0;
      esperarPlayer(j);
      int lido = mExato(hB, t, &p, sb, &ok, NULL, NULL);
      if (falhou(j, lido, hB)) return 0;
      if (!lido || !ok) continue;
      double c = 0;
      for (int k = 0; k < SIG_N; k++) c += sa[k] * sb[k];
      cs[nc] = c; ps[nc] = p; nc++;
    }
    if (nc < 3) continue;
    int m = 0;
    for (int k = 1; k < nc; k++) if (cs[k] > cs[m]) m = k;
    if (cs[m] < minCorr) continue;
    double t = ps[m];
    if (m > 0 && m < nc - 1 && ps[m + 1] > ps[m - 1]) {                 // parabola through the peak and its neighbours
      double den = cs[m - 1] - 2 * cs[m] + cs[m + 1];
      if (den < -1e-9) {
        double s = 0.5 * (cs[m - 1] - cs[m + 1]) / den;
        if (s > 1) s = 1;
        if (s < -1) s = -1;
        t = ps[m] + s * (fabs(ps[m + 1] - ps[m]) + fabs(ps[m] - ps[m - 1])) / 2;
      }
    }
    ref[n].pa = ord[q].pa; ref[n].pb = t; n++;
  }
  *nref = n;
  if (n < MIN_REFINADOS) return 0;
  double soma = 0, sp = 0, sp2 = 0, s2 = 0;
  for (int k = 0; k < n; k++) { soma += ref[k].pb - a * ref[k].pa; sp += ref[k].pa; sp2 += ref[k].pa * ref[k].pa; }
  *b2 = soma / n;
  for (int k = 0; k < n; k++) { double r = ref[k].pb - (a * ref[k].pa + *b2); s2 += r * r; }
  double sigma = sqrt(s2 / n), spread = sqrt(fmax(0, sp2 / n - (sp / n) * (sp / n)));
  if (spread <= 0) spread = 1;
  *erro = 3 * sigma / sqrt(n) + 2.0 * (sigma / (sqrt(n) * spread)) * durA / 2 + 500.0;
  return 1;
}

// ---- the check from the SCREEN: frames the player already shows, no 4K read ----
// For a copy whose runtime matches the playing file (within 2.5 s: the same cut), the frames on screen are the
// anchors: NvPlayer.assinaturaTela copies the frame being shown (PixelCopy, 160x90) with its position. Each one is
// matched against EXACT frames of the copy around the predicted time (the first one after a keyframe scan), and
// the parabola peak gives the offset. Accepted with 4+ anchors over 10+ s of playback that agree within 1.5 s.
// Measured before (22/10): the 4K anchors cost 74-82 MB of the playing file and the hitches at the start.
#define TELA_ANCORAS 5
#define TELA_MIN_CORR 0.55
#define TELA_ESPACO_MS 4000L
#define TELA_ACORDO_MS 3000.0      // measured 22/10: real matches spread 1.7-1.9 s (screen timing + 1 s exact steps)
#define TELA_PRAZO_MS 120000u
#define TELA_DUR_IGUAL_MS 30000L    // copies up to 30 s longer/shorter go to the screen check, which measures the offset
                                    // (22/10: a Bond film's only same-codec copy was 12 s off: logo/intro variants)
#define OTIMISTA_DUR_MS 4000L       // the optimistic start only for (nearly) the same length

static int mTela(long *pts, float sig[SIG_N], int *temSig) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  jbyteArray a = (jbyteArray)(*env)->CallStaticObjectMethod(env, cls, jTela);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); a = NULL; }
  return desempacotar(env, a, pts, sig, temSig, NULL, NULL);
}

// Similarity peak of the copy's exact frames at centro + DES[i]; *t = interpolated copy time. 0 = no frame/weak.
static int picoExato(Job *j, long long hB, const float *sa, double centro, const long *des, int nd, double minCorr,
                     double *t, double *rMelhor) {
  double cs[8], ps[8]; int nc = 0;
  float sb[SIG_N];
  for (int d = 0; d < nd && !parou(j); d++) {
    long alvo = (long)(centro + des[d]), p; int ok;
    if (alvo < 0) alvo = 0;
    esperarPlayer(j);
    int lido = mExato(hB, alvo, &p, sb, &ok, NULL, NULL);
    // A check frame that did not come (a slow forward decode) only skips this probe; a READ failure still backs
    // off, and a refused connection still ends the title (23/10: three exact-frame time-outs ended a good copy).
    if (!lido && mFalhaRede(hB) && falhou(j, lido, hB)) return -1;
    if (lido) falhou(j, lido, hB);
    if (!lido || !ok) continue;
    double c = 0;
    for (int k = 0; k < SIG_N; k++) c += sa[k] * sb[k];
    cs[nc] = c; ps[nc] = p; nc++;
  }
  if (nc < 2) return 0;
  int m = 0;
  for (int k = 1; k < nc; k++) if (cs[k] > cs[m]) m = k;
  *rMelhor = cs[m];
  if (cs[m] < minCorr) return 0;
  *t = ps[m];
  if (m > 0 && m < nc - 1 && ps[m + 1] > ps[m - 1]) {
    double den = cs[m - 1] - 2 * cs[m] + cs[m + 1];
    if (den < -1e-9) {
      double s = 0.5 * (cs[m - 1] - cs[m + 1]) / den;
      if (s > 1) s = 1;
      if (s < -1) s = -1;
      *t = ps[m] + s * (fabs(ps[m + 1] - ps[m]) + fabs(ps[m] - ps[m - 1])) / 2;
    }
  }
  return 1;
}

// 1 accepted (mapa filled), 0 rejected, -1 the screen gave no frames (the caller falls back to the 4K anchors).
static int conferirTela(Job *j, long long hB, long durB, Mapa *mapa) {
  static const long LARGO[] = { -2000, -1000, 0, 1000, 2000 }, ESTREITO[] = { -1000, 0, 1000 };
  Par m[TELA_ANCORAS];
  int nm = 0, telas = 0, falhasTela = 0, provas = 0;
  long ultimo = -1000000;
  double b = 0;
  float sa[SIG_N], sb[SIG_N];
  Uint32 ini = SDL_GetTicks();
  while (nm < TELA_ANCORAS && !parou(j) && SDL_GetTicks() - ini < TELA_PRAZO_MS && provas < TELA_ANCORAS * 3) {
    esperarPlayer(j);
    if (scrubbing) { dormir(j, 500); continue; }
    long pA; int ok;
    if (!mTela(&pA, sa, &ok)) { if (++falhasTela >= 10 && !telas) return -1; dormir(j, 1000); continue; }
    telas++;
    if (!ok || labs(pA - ultimo) < TELA_ESPACO_MS || pA < 5000) { dormir(j, 1000); continue; }   // flat, too close
    ultimo = pA; provas++;
    double t, r = -2;
    int achou;
    if (nm == 0) {
      // first anchor: the copy's KEYFRAMES +-30 s around the same time (an intro, logo or title sequence of another
      // length shifts what follows; 22/10 a Bond copy 12 s shorter was -4.8 s at the opening and further off later,
      // so a check started mid-film missed it at +-8 s), fetched in ONE multi-range batch, then the exact frames
      double melhor = -2, pMelhor = pA;
      long ultimoKf = -1, ts[31];
      int nt = 0;
      for (long d = -30000; d <= 30000; d += 2000) if (pA + d >= 0 && pA + d < durB) ts[nt++] = pA + d;
      if (nt > 1) mPreparar(hB, ts, nt);
      for (int i = 0; i < nt && !parou(j); i++) {
        long alvo = ts[i], p; int okB;
        esperarPlayer(j);
        int lido = mQuadro(hB, alvo, 1, 0, &p, sb, &okB, NULL, NULL);
        if (falhou(j, lido, hB)) return 0;
        if (!lido || !okB || p == ultimoKf) continue;
        ultimoKf = p;
        double c = 0;
        for (int k = 0; k < SIG_N; k++) c += sa[k] * sb[k];
        if (c > melhor) { melhor = c; pMelhor = p; }
      }
      if (melhor < TELA_MIN_CORR - 0.15) {
        printf("[miniatura] screen check at %lds: no similar copy frame nearby (best r=%.2f)\n", pA / 1000, melhor); fflush(stdout);
        continue;
      }
      achou = picoExato(j, hB, sa, pMelhor, LARGO, 5, TELA_MIN_CORR, &t, &r);
    } else {
      achou = picoExato(j, hB, sa, pA + b, ESTREITO, 3, TELA_MIN_CORR, &t, &r);
    }
    if (achou < 0) return 0;
    printf("[miniatura] screen check at %lds: r=%.2f %s (offset %+.2fs)\n", pA / 1000, r, achou ? "match" : "no",
           achou ? (t - pA) / 1000.0 : 0.0);
    fflush(stdout);
    if (!achou) continue;
    m[nm].pa = pA; m[nm].pb = t; nm++;
    double soma = 0;
    for (int k = 0; k < nm; k++) soma += m[k].pb - m[k].pa;
    b = soma / nm;
  }
  if (!telas) return -1;
  if (nm < 4) { printf("[miniatura] screen check: rejected (%d of %d agree)\n", nm, provas); fflush(stdout); return 0; }
  // the median offset; the one anchor farthest from it is dropped when 5+ agreed (a static scene can peak wrong)
  double offs[TELA_ANCORAS];
  for (int k = 0; k < nm; k++) offs[k] = m[k].pb - m[k].pa;
  b = mediana(offs, nm);
  if (nm >= 5) {
    int pior = 0;
    for (int k = 1; k < nm; k++) if (fabs(m[k].pb - m[k].pa - b) > fabs(m[pior].pb - m[pior].pa - b)) pior = k;
    m[pior] = m[--nm];
    for (int k = 0; k < nm; k++) offs[k] = m[k].pb - m[k].pa;
    b = mediana(offs, nm);
  }
  double lo = 1e18, hi = -1e18, s2 = 0, aMin = 1e18, aMax = -1e18;
  for (int k = 0; k < nm; k++) {
    double o = m[k].pb - m[k].pa;
    if (o < lo) lo = o;
    if (o > hi) hi = o;
    s2 += (o - b) * (o - b);
    if (m[k].pa < aMin) aMin = m[k].pa;
    if (m[k].pa > aMax) aMax = m[k].pa;
  }
  if (hi - lo > TELA_ACORDO_MS || aMax - aMin < 10000) {
    printf("[miniatura] screen check: rejected (offsets spread %.1fs over %.0fs of playback)\n", (hi - lo) / 1000, (aMax - aMin) / 1000);
    fflush(stdout);
    return 0;
  }
  double erro = 3 * sqrt(s2 / nm) / sqrt(nm) + 500.0;
  mapa->a = 1.0; mapa->b = b; mapa->erro = erro; mapa->inliers = nm;
  j->ultimoBom = (long)aMax;           // the latest playback time this copy was seen to match
  printf("[miniatura] screen check: ACCEPTED t_copy = t_play %+.2fs (%d anchors over %.0fs, +-%.1fs) in %.1f s, no 4K read\n",
         b / 1000, nm, (aMax - aMin) / 1000, erro / 1000, (SDL_GetTicks() - ini) / 1000.0);
  fflush(stdout);
  return 1;
}

// One re-check: the frame on screen against the copy's exact frames at the mapped time (-1, 0, +1 s). 1 = matches,
// 0 = does not (best r < RECHECK_MIN), -1 = no usable screen frame or copy frame (paused, flat scene, read failure).
#define RECHECK_MS 180000u
#define RECHECK_MIN 0.45
static int reconferir(Job *j, long long h, Mapa m, long *pA) {
  float sa[SIG_N], sb[SIG_N];
  int ok, vistos = 0;
  long p;
  if (!h || !mTela(pA, sa, &ok) || !ok || *pA < 5000) return -1;
  double melhor = -2;
  for (long d = -1000; d <= 1000 && !parou(j); d += 1000) {
    long alvo = (long)(m.a * *pA + m.b) + d;
    int okB;
    if (alvo < 0 || !mExato(h, alvo, &p, sb, &okB, NULL, NULL) || !okB) continue;
    vistos++;
    double c = 0;
    for (int k = 0; k < SIG_N; k++) c += sa[k] * sb[k];
    if (c > melhor) melhor = c;
  }
  if (!vistos) return -1;
  return melhor >= RECHECK_MIN ? 1 : 0;
}

// After the copy stopped matching (an opening, a scene or credits of another length): find its NEW offset. Two screen
// frames, each searched over the copy's keyframes +-30 s around the old prediction (fetched in ONE multi-range batch),
// then exact frames around the best; accepted when the two offsets agree within 2 s. 1 = found (*novoB).
// Measured 22/10 (Bond, 164 min): the offset was -4.8 s for the opening, then the copy diverged.
static int reancorar(Job *j, long long h, Mapa m, double *novoB) {
  static const long LARGO[] = { -2000, -1000, 0, 1000, 2000 };
  double offs[2];
  int n = 0, provas = 0;
  long ultimo = -1000000;
  float sa[SIG_N], sb[SIG_N];
  Uint32 ini = SDL_GetTicks();
  while (n < 2 && provas < 6 && !parou(j) && SDL_GetTicks() - ini < 120000u) {
    esperarPlayer(j);
    if (scrubbing) { dormir(j, 500); continue; }
    long pA; int ok;
    if (!mTela(&pA, sa, &ok) || !ok || labs(pA - ultimo) < 3000) { dormir(j, 1000); continue; }
    ultimo = pA; provas++;
    double centro = m.a * pA + m.b, melhor = -2, pMelhor = 0;
    long ts[31]; int nt = 0;
    for (long d = -30000; d <= 30000; d += 2000) if ((long)centro + d >= 0) ts[nt++] = (long)centro + d;
    if (nt > 1) mPreparar(h, ts, nt);                      // the whole window in one request
    long ultimoKf = -1;
    for (int i = 0; i < nt && !parou(j); i++) {
      long p; int okB;
      if (!mQuadro(h, ts[i], 1, 0, &p, sb, &okB, NULL, NULL) || !okB || p == ultimoKf) continue;
      ultimoKf = p;
      double c = 0;
      for (int k = 0; k < SIG_N; k++) c += sa[k] * sb[k];
      if (c > melhor) { melhor = c; pMelhor = p; }
    }
    double t = 0, r = -2;
    int achou = melhor >= TELA_MIN_CORR && picoExato(j, h, sa, pMelhor, LARGO, 5, TELA_MIN_CORR, &t, &r) == 1;
    printf("[miniatura] re-anchor probe at %lds: best r=%.2f %s\n", pA / 1000, achou ? r : melhor,
           achou ? "match" : "no");
    fflush(stdout);
    if (achou) {
      char extra[48] = "";
      offs[n++] = t - pA;
      if (n == 2) snprintf(extra, sizeof extra, " / %+.2fs", offs[1] / 1000);
      printf("[miniatura] re-anchor offset %+.2fs%s\n", offs[0] / 1000, extra); fflush(stdout);
    }
  }
  if (n < 2 || fabs(offs[0] - offs[1]) > 2000) return 0;
  *novoB = (offs[0] + offs[1]) / 2;
  return 1;
}

static int conferir(Job *j, long long hA, long long hB, long durA, long durB, Ancora *anc, Mapa *mapa, int hdr) {
  double minCorr = hdr ? 0.50 : MIN_CORR;
  double razao = (double)durB / (double)durA;
  Par matches[N_ANCORAS];
  int nm = 0, chamadasB = 0;
  float sb[SIG_N];
  if (fabs(razao - 1.0) > MAX_RAZAO_ERRO) { printf("[miniatura] check: runtime differs %.1f%%\n", (razao - 1) * 100); return 0; }
  for (int v = 0; v < N_ANCORAS && !parou(j); v++) {
    int i = (v * 7) % N_ANCORAS;
    Ancora *an = &anc[i];
    if (an->estado == 0) {
      double frac = (i + 0.2 + 0.6 * fmod(i * 0.6180339887, 1.0)) / N_ANCORAS;
      long tA = (long)(durA * frac);
      tA = (tA + GRADE_MS / 2) / GRADE_MS * GRADE_MS;
      unsigned char *jpg = NULL; int nj = 0;
      esperarPlayer(j);
      esperarFolga(j);
      int lido = mQuadro(hA, tA, 1, 1, &an->pA, an->sig, &an->ok, &jpg, &nj);
      if (falhou(j, lido, hA)) { mSoltar(hA); return 0; }
      mSoltar(hA);                                           // the 4K decoder goes back to the player between anchors
      an->estado = lido ? 1 : -1;
      if (lido) {                                            // the anchor is also a finished tile when it lands on its slot
        int k = (int)((an->pA + GRADE_MS / 2) / GRADE_MS);
        pthread_mutex_lock(&mu);
        if (jpg && k >= 0 && k < j->n && labs(an->pA - k * GRADE_MS) <= TOL_MS && !j->tiles[k].jpg) {
          j->tiles[k].jpg = jpg; j->tiles[k].n = nj; j->tiles[k].pts = an->pA; j->tiles[k].revision++; j->tentado[k] = 1;
          j->feitos++; estado = M_PRONTO; jpg = NULL;
        }
        pthread_mutex_unlock(&mu);
      }
      free(jpg);
      dormir(j, 400);                                        // spread the 4K reads out
    }
    if (an->estado != 1 || !an->ok) continue;                // failed, or a flat frame: no information
    long pA = an->pA;
    const float *sa = an->sig;
    double a, b, centro;
    long janela, passo;
    if (hipotese(matches, nm, razao, &a, &b)) { centro = a * pA + b; janela = JANELA_ESTREITA_MS; passo = PASSO_ESTREITO_MS; }
    else { centro = razao * pA; janela = JANELA_LARGA_MS; passo = PASSO_LARGO_MS; }
    double melhor = -2, pMelhor = 0;
    double cs[64], ps[64]; int nc = 0;
    for (long t = (long)centro - janela; t <= (long)centro + janela && nc < 64 && !parou(j); t += passo) {
      long tb = t < 0 ? 0 : t >= durB ? durB - 1 : t, pB; int okB;
      esperarPlayer(j);
      int lidoB = mQuadro(hB, tb, 1, 0, &pB, sb, &okB, NULL, NULL);
      if (falhou(j, lidoB, hB)) return 0;
      if (!lidoB) continue;
      chamadasB++;
      if (!okB) continue;
      int visto = 0;
      for (int k = 0; k < nc; k++) if (ps[k] == pB) visto = 1;
      if (visto) continue;
      double c = 0;
      for (int k = 0; k < SIG_N; k++) c += sa[k] * sb[k];
      cs[nc] = c; ps[nc] = pB; nc++;
      if (c > melhor) { melhor = c; pMelhor = pB; }
    }
    double segundo = -1;
    for (int k = 0; k < nc; k++) if (fabs(ps[k] - pMelhor) > TOMADA_MS && cs[k] > segundo) segundo = cs[k];
    int casou = nc && melhor >= minCorr && melhor - segundo >= MIN_MARGEM;
    if (casou) { matches[nm].pa = pA; matches[nm].pb = pMelhor; nm++; }
    printf("[miniatura] check anchor %d/%d at %lds: best r=%.2f next r=%.2f %s (offset %+.1fs)\n", v + 1, N_ANCORAS,
           pA / 1000, melhor, segundo, casou ? "match" : "no", nc ? (pMelhor - pA) / 1000.0 : 0.0);
    fflush(stdout);
    // Enough already: from 12 anchors on, 8+ agreeing on one mapping with 2+ in every quarter is what the full 20
    // would have to show anyway (the fine pass still checks the timing). Saves ~8 4K reads and ~25 s.
    if (v + 1 >= 12 && nm >= MIN_INLIERS) {
      double a0, b0;
      int q[4] = {0}, ok = 0;
      if (hipotese(matches, nm, razao, &a0, &b0))
        for (int k = 0; k < nm; k++)
          if (fabs(matches[k].pb - (a0 * matches[k].pa + b0)) <= MAX_RESIDUO_MS) {
            int qq = (int)(matches[k].pa * 4 / durA);
            if (qq >= 0 && qq < 4) q[qq]++;
            ok++;
          }
      if (ok >= MIN_INLIERS && q[0] >= MIN_POR_QUARTO && q[1] >= MIN_POR_QUARTO && q[2] >= MIN_POR_QUARTO && q[3] >= MIN_POR_QUARTO) {
        printf("[miniatura] check: clear after %d anchors (%d agree)\n", v + 1, ok); fflush(stdout);
        break;
      }
    }
  }
  double a, b;
  if (!hipotese(matches, nm, razao, &a, &b)) { printf("[miniatura] check: no visual match (%d)\n", nm); return 0; }
  Par in[N_ANCORAS]; int ni = 0;
  for (int k = 0; k < nm; k++) if (fabs(matches[k].pb - (a * matches[k].pa + b)) <= MAX_RESIDUO_MS) in[ni++] = matches[k];
  int quartos[4] = {0};
  for (int k = 0; k < ni; k++) { int q = (int)(in[k].pa * 4 / durA); if (q >= 0 && q < 4) quartos[q]++; }
  if (ni < MIN_INLIERS || quartos[0] < MIN_POR_QUARTO || quartos[1] < MIN_POR_QUARTO || quartos[2] < MIN_POR_QUARTO ||
      quartos[3] < MIN_POR_QUARTO) {
    printf("[miniatura] check: rejected (%d anchors agree, quarters %d/%d/%d/%d)\n", ni, quartos[0], quartos[1], quartos[2], quartos[3]);
    return 0;
  }
  hipotese(in, ni, razao, &a, &b);
  double res = 0;
  for (int k = 0; k < ni; k++) { double r = fabs(in[k].pb - (a * in[k].pa + b)); if (r > res) res = r; }
  if (res > MAX_RESIDUO_MS || fabs(a - 1.0) > MAX_RAZAO_ERRO) { printf("[miniatura] check: rejected (residual %.0f ms)\n", res); return 0; }
  // the fine pass, under each plausible speed (same speed; the duration ratio when that could be a real speed change)
  double slopes[2] = { a, fabs(razao - 1.0) >= 0.01 ? (a == 1.0 ? razao : 1.0) : a };
  int ns = slopes[1] != slopes[0] ? 2 : 1;
  double melhorErro = 1e300, melhorA = a, melhorB = b;
  int melhorN = 0;
  for (int h = 0; h < ns && !parou(j); h++) {
    double bh = b, b2, erro; int nref = 0;
    if (h == 1) {                                            // offset for the other slope: median over the inliers
      double bs[N_ANCORAS];
      for (int k = 0; k < ni; k++) bs[k] = in[k].pb - slopes[1] * in[k].pa;
      bh = mediana(bs, ni);
    }
    if (refinar(j, hB, anc, in, ni, slopes[h], bh, durA, &b2, &erro, &nref, minCorr) && fabs(b2 - bh) <= MAX_RESIDUO_MS && erro < melhorErro) {
      melhorErro = erro; melhorA = slopes[h]; melhorB = b2; melhorN = nref;
    }
    printf("[miniatura] fine pass (speed %.4f): %d anchors refined%s\n", slopes[h], nref,
           nref >= MIN_REFINADOS ? "" : " (too few)");
    fflush(stdout);
  }
  if (melhorN < MIN_REFINADOS) { printf("[miniatura] check: fine pass failed\n"); fflush(stdout); return 0; }
  if (melhorErro > MAX_ERRO_MS) { printf("[miniatura] check: mapping uncertain (%.0f ms after the fine pass)\n", melhorErro); return 0; }
  mapa->a = melhorA; mapa->b = melhorB; mapa->erro = melhorErro; mapa->inliers = ni;
  printf("[miniatura] check: ACCEPTED t_copy = %.5f x t_play %+.2fs (%d anchors, fine %d, +-%.1fs, %d copy reads)\n", melhorA,
         melhorB / 1000, ni, melhorN, melhorErro / 1000, chamadasB);
  fflush(stdout);
  return 1;
}

// ---- the set on this device's storage: a rewatch or a resume shows it at once and reads nothing ----
// <data>/miniaturas/<fnv64 of the file identity>.nvm : "NVM3" (older NVM1/NVM2 sets are ignored: squeezed 320x180, or saved before the re-check existed), n, dur, complete, then (k, pts, len, jpeg)...
// Identity: the file name + exact size the add-on declared (the same file through any link), else the link.
// Kept 2 hours after the last use (long enough to come back to the film the same evening), then deleted; never
// more than 200 MB in total, least recently used first.
#define CACHE_MAX_BYTES (200LL << 20)
#define CACHE_TTL_S (2 * 3600)

static void chaveArquivo(const SeekrFonte *f, char *out, size_t n, const char *modo) {
  char id[700];
  const char *nome = f->arquivo && f->arquivo[0] ? f->arquivo : "";
  const char *barra = strrchr(nome, '/');
  if (barra) nome = barra + 1;
  // Never the link: add-on/proxy links are signed per request (measured: a resumed episode missed its saved set).
  // The title + episode, then the file's exact size and name when the add-on declares them, else its duration.
  if (f->tamanho || nome[0])
    snprintf(id, sizeof id, "t|%s:%d:%d|%llu|%s", f->imdb ? f->imdb : "", f->t, f->e, (unsigned long long)f->tamanho, nome);
  else snprintf(id, sizeof id, "d|%s:%d:%d|%ld", f->imdb ? f->imdb : "", f->t, f->e, f->durMs / 1000);
  if (modo[0]) snprintf(id + strlen(id), sizeof id - strlen(id), "|%s", modo);
  unsigned long long h = 1469598103934665603ULL;
  for (const char *p = id; *p; p++) { h ^= (unsigned char)tolower((unsigned char)*p); h *= 1099511628211ULL; }
  char dir[512];
  if (!dados_caminho(dir, sizeof dir, "miniaturas")) { out[0] = 0; return; }
  snprintf(out, n, "%s/%016llx.nvm", dir, h);
}

static void escreverI32(FILE *f, long v) { unsigned char b[4] = { (unsigned char)(v >> 24), (unsigned char)(v >> 16), (unsigned char)(v >> 8), (unsigned char)v }; fwrite(b, 1, 4, f); }
static int lerI32(FILE *f, long *v) { unsigned char b[4]; if (fread(b, 1, 4, f) != 4) return 0; *v = (long)((unsigned long)b[0] << 24 | b[1] << 16 | b[2] << 8 | b[3]); return 1; }

// Loads what an earlier run made. 1 = the set was complete (nothing to read from the network).
static void diretoRebuildCoverage(Job *j);   // caller holds mu
static int diretoCoverageComplete(const Job *j);
static int carregar0(Job *j);
static int carregar(Job *j) {
  mPipelineState(6, 1);
  int result = carregar0(j);
  mPipelineState(6, 0);
  return result;
}
static int carregar0(Job *j) {
  if (!j->arq[0]) return 0;
  struct stat st;
  if (stat(j->arq, &st) == 0 && time(NULL) - st.st_mtime > CACHE_TTL_S) { unlink(j->arq); return 0; }
  FILE *f = fopen(j->arq, "rb");
  if (!f) return 0;
  char mag[4]; long n, dur, completo, k, pts, len;
  int lidos = 0;
  if (fread(mag, 1, 4, f) != 4 || memcmp(mag, "NVM3", 4) || !lerI32(f, &n) || !lerI32(f, &dur) || !lerI32(f, &completo) ||
      n != j->n || (j->direto && dur != j->dur)) { fclose(f); return 0; }
  while (lerI32(f, &k) && lerI32(f, &pts) && lerI32(f, &len)) {
    if (k < 0 || k >= j->n || len <= 0 || len > (1 << 20) || (j->direto && (pts < 0 || pts > j->dur))) break;
    unsigned char *b = malloc((size_t)len);
    if (!b || fread(b, 1, (size_t)len, f) != (size_t)len) { free(b); break; }
    pthread_mutex_lock(&mu);
    if (!j->tiles[k].jpg) {
      j->tiles[k].jpg = b; j->tiles[k].n = (int)len; j->tiles[k].pts = pts; j->tiles[k].revision++; j->feitos++; lidos++; b = NULL;
      if (j->direto) j->framePts[k] = pts;
    }
    // A direct record index owns an image; it is not a nominal time slot.
    if (!j->direto) j->tentado[k] = 1;
    if (job == j && j->feitos) estado = M_PRONTO;
    pthread_mutex_unlock(&mu);
    free(b);
  }
  fclose(f);
  utime(j->arq, NULL);                                       // recently used: the cap removes others first
  if (j->direto) {
    pthread_mutex_lock(&mu);
    diretoRebuildCoverage(j);
    completo = completo && diretoCoverageComplete(j);
    int covered = mini_coverage_covered_count(&j->coverage);
    pthread_mutex_unlock(&mu);
    printf("[miniatura] loaded %d saved images; coverage %d/%d targets%s\n", lidos, covered, j->n,
           completo ? " (complete: nothing to read)" : " (partial)");
  } else printf("[miniatura] loaded %d saved thumbnails%s\n", lidos, completo ? " (complete: nothing to read)" : "");
  fflush(stdout);
  return completo && lidos > 0;
}

// Publish the result before permitting any progress pill. A saved set is not
// a fresh generation, even when its retained tile count already exceeds 50%.
static int carregarInformado(Job *j) {
  int complete = j->auditarRede ? 0 : carregar(j);
  pthread_mutex_lock(&mu);
  j->cacheTipo = complete ? 2 : j->feitos > 0 ? 1 : 0;
  j->cacheChecado = 1;
  pthread_mutex_unlock(&mu);
  return complete;
}

typedef struct { char nome[300]; long long tam; time_t quando; } ArqCache;
static void limparCache(const char *dirArq) {
  char dir[512];
  snprintf(dir, sizeof dir, "%s", dirArq);
  char *barra = strrchr(dir, '/');
  if (!barra) return;
  *barra = 0;
  DIR *d = opendir(dir);
  if (!d) return;
  ArqCache *v = NULL; int n = 0, cap = 0;
  long long total = 0;
  struct dirent *e;
  while ((e = readdir(d))) {
    size_t L = strlen(e->d_name);
    if (L < 5 || strcmp(e->d_name + L - 4, ".nvm")) continue;
    char p[820]; struct stat st;
    snprintf(p, sizeof p, "%s/%s", dir, e->d_name);
    if (stat(p, &st)) continue;
    if (time(NULL) - st.st_mtime > CACHE_TTL_S && strcmp(p, dirArq)) { unlink(p); continue; }   // expired
    if (n == cap) { cap = cap ? cap * 2 : 32; ArqCache *nv = realloc(v, sizeof *v * (size_t)cap); if (!nv) break; v = nv; }
    snprintf(v[n].nome, sizeof v[n].nome, "%s", p); v[n].tam = st.st_size; v[n].quando = st.st_mtime; n++;
    total += st.st_size;
  }
  closedir(d);
  while (total > CACHE_MAX_BYTES && n > 1) {
    int velhoI = 0;
    for (int i = 1; i < n; i++) if (v[i].quando < v[velhoI].quando) velhoI = i;
    if (!strcmp(v[velhoI].nome, dirArq)) { v[velhoI].quando = (time_t)0x7fffffff; continue; }
    unlink(v[velhoI].nome);
    total -= v[velhoI].tam;
    v[velhoI] = v[--n];
  }
  free(v);
}

static void salvar0(Job *j, int completo);
static void salvar(Job *j, int completo) {
  mPipelineState(6, 1);
  salvar0(j, completo);
  mPipelineState(6, 0);
}
static void salvar0(Job *j, int completo) {
  if (j->auditarRede) return; // diagnostic cold fetch: preserve the saved set
  if (!j->arq[0]) return;
  if (j->direto) {
    pthread_mutex_lock(&mu);
    completo = completo && diretoCoverageComplete(j);
    pthread_mutex_unlock(&mu);
  }
  if (!j->feitos) { unlink(j->arq); return; }   // nothing left (a dropped copy): no stale set may survive (22/10, Bond)
  char dir[sizeof j->arq], tmp[sizeof j->arq + 5];
  snprintf(dir, sizeof dir, "%s", j->arq);
  char *barra = strrchr(dir, '/');
  if (barra) { *barra = 0; mkdir(dir, 0700); }
  snprintf(tmp, sizeof tmp, "%s.tmp", j->arq);
  int append = j->direto && j->appendPronto;
  FILE *f = fopen(append ? j->arq : tmp, append ? "r+b" : "wb");
  if (!f && append) { j->appendPronto = 0; salvar(j, completo); return; }
  if (!f) return;
  if (append) {
    // Mark incomplete before appending. A truncated last record remains readable up to the previous checkpoint.
    if (fseek(f, 12, SEEK_SET)) { fclose(f); j->appendPronto = 0; return; }
    escreverI32(f, 0);
    if (fflush(f) || fseek(f, 0, SEEK_END)) { fclose(f); j->appendPronto = 0; return; }
  } else {
    fwrite("NVM3", 1, 4, f);
    escreverI32(f, j->n); escreverI32(f, j->dur); escreverI32(f, 0);
  }
  for (int k = 0; k < j->n; k++) {
    pthread_mutex_lock(&mu);
    unsigned char *b = j->tiles[k].jpg; int len = j->tiles[k].n; long pts = j->tiles[k].pts;
    int saved = j->tiles[k].salvo;
    pthread_mutex_unlock(&mu);                                // tiles are only ever added while the job lives
    if (!b || (append && saved)) continue;
    escreverI32(f, k); escreverI32(f, pts); escreverI32(f, len);
    fwrite(b, 1, (size_t)len, f);
    if (j->direto) { pthread_mutex_lock(&mu); j->tiles[k].salvo = 1; pthread_mutex_unlock(&mu); }
  }
  int ok = !ferror(f) && fflush(f) == 0;
  if (ok && completo) {
    ok = fseek(f, 12, SEEK_SET) == 0;
    if (ok) { escreverI32(f, 1); ok = !ferror(f); }
  }
  if (fclose(f)) ok = 0;
  if (ok && (append || rename(tmp, j->arq) == 0)) {
    if (j->direto) j->appendPronto = 1;
    limparCache(j->arq);
  } else {
    j->appendPronto = 0;
    pthread_mutex_lock(&mu);
    for (int k = 0; k < j->n; k++) j->tiles[k].salvo = 0;
    pthread_mutex_unlock(&mu);
    if (!append) unlink(tmp);
  }
}

// ---- which slot next ----
static int livre(const Job *j, int k) { return k >= 0 && k < j->n && !j->tentado[k]; }
static int proximo(const Job *j) {
  int n = j->n, ph = (int)(playheadMs / GRADE_MS), sc = (int)((posMs + GRADE_MS / 2) / GRADE_MS);
  if (scrubbing) for (int d = 0; d <= 1; d++) { if (livre(j, sc + d)) return sc + d; if (livre(j, sc - d)) return sc - d; }
  for (int d = 0; d <= 6; d++) { if (livre(j, ph + d)) return ph + d; if (livre(j, ph - d)) return ph - d; }
  for (int k = 0; k < n; k += 30) if (livre(j, k)) return k;
  int fatia = (int)(n * 0.05) + 1;
  for (int d = 0; d <= fatia; d++) { if (livre(j, ph + d)) return ph + d; if (livre(j, ph - d)) return ph - d; }
  for (int k = 0; k < n; k += 6) if (livre(j, k)) return k;
  for (int k = 0; k < n; k++) if (livre(j, k)) return k;
  return -1;
}

// A slot the keyframe pass left empty (the copy's keyframes too far apart), nearest to the playhead first.
static int coberto(const Job *j, int k) {
  for (int d = -1; d <= 1; d++) {
    int v = k + d;
    if (v >= 0 && v < j->n && j->tiles[v].jpg && labs(j->tiles[v].pts - k * GRADE_MS) <= TOL_COPIA_MS) return 1;
  }
  return 0;
}
// Only real gaps (nothing made within 8 s): measured 22/10, exact-frame fills of covered slots were the last
// ~50 s of a 3 h film. And only while they are worth it: not at all once 90 % of the slots have their own
// thumbnail, and never more than 10 s (measured 22/10 on 102 min: ~20 fills cost 65 s and 80 MB, ~3 s and
// 4 MB each, for spots the bar already covered with a neighbour). Called with `mu` held.
#define LACUNA_COBERTURA 0.90
#define LACUNA_PRAZO_MS 10000u
static int lacunaPermitida(const Job *j) {
  if (j->feitos >= (int)(j->n * LACUNA_COBERTURA)) return 0;
  return !j->lacunaDesde || SDL_GetTicks() - j->lacunaDesde < LACUNA_PRAZO_MS;
}
static int proximaLacuna(const Job *j) {
  int ph = (int)(playheadMs / GRADE_MS), melhor = -1;
  for (int k = 0; k < j->n; k++)
    if (j->tentado[k] == 1 && !j->tiles[k].jpg && !coberto(j, k) && (melhor < 0 || abs(k - ph) < abs(melhor - ph))) melhor = k;
  return melhor;
}

// One lane of thumbnail making (see trabalhar). Slots are claimed under the lock, so two lanes never make the same one.
typedef struct { Job *j; long long h; int usaB; int id; int feitos; unsigned long somaMs; long long lido; volatile int acabou; } Faixa;
static Mapa mapaAtual(Job *j) { pthread_mutex_lock(&mu); Mapa m = j->mapa; pthread_mutex_unlock(&mu); return m; }
static void *faixa(void *u) {
  Faixa *f = u;
  Job *j = f->j;
  long long lido0 = mLidos(f->h);
  falhasSeguidas = 0; decodSeguidas = 0;
  // Batches: claim LOTE slots, fetch their keyframes with ONE multi-range request, then decode them from memory.
  int fila[LOTE], nfila = 0, ifila = 0, lote = f->usaB ? LOTE : 0, lotes = 0, trazidos = 0;
  while (!parou(j)) {
    if (bufferando || video_reconectando() || video_falhou() || folgaBaixa(j)) { mSoltar(f->h); esperarPlayer(j); continue; }
    long disp = memDisponivelKB();
    if (disp >= 0 && (j->memMin < 0 || disp < j->memMin)) j->memMin = disp;
    if (disp >= 0 && disp < PISO_KB) { mSoltar(f->h); dormir(j, 1500); continue; }
    if (heapApertado(j)) { dormir(j, 1500); continue; }
    int k = -1, exato = 0;
    if (ifila < nfila) k = fila[ifila++];
    else {
      pthread_mutex_lock(&mu);
      nfila = ifila = 0;
      if (lote > 0) lote = mLote(f->h);   // sized to ~4 MB of this copy's keyframe windows
      for (int quer = lote > 0 ? lote : 1; nfila < quer; ) {
        int q = proximo(j);
        if (q < 0) break;
        j->tentado[q] = 1; fila[nfila++] = q;
      }
      if (!nfila && f->usaB && lacunaPermitida(j) && (k = proximaLacuna(j)) >= 0) {   // second pass: the exact frame
        if (!j->lacunaDesde) j->lacunaDesde = SDL_GetTicks();
        j->tentado[k] = 2; exato = 1;
      }
      pthread_mutex_unlock(&mu);
      if (nfila > 1 && lote > 0) {
        long ts[LOTE];
        Mapa mp = mapaAtual(j);
        for (int i = 0; i < nfila; i++) { ts[i] = (long)(mp.a * fila[i] * GRADE_MS + mp.b); if (ts[i] < 0) ts[i] = 0; }
        int r = mPreparar(f->h, ts, nfila);
        if (r == -2) { printf("[miniatura] stopping: the source refused/stalled a connection (protecting playback)\n"); fflush(stdout); break; }
        if (r < 0) { lote = 0; printf("[miniatura] lane %d: multi-range batches not possible for this copy, one at a time\n", f->id); fflush(stdout); }
        else { lotes++; trazidos += r; }
      }
      if (nfila) k = fila[ifila++];
    }
    if (k < 0) break;
    long alvo = k * GRADE_MS, p; int okS;
    float sig[SIG_N]; unsigned char *jpg = NULL; int nj = 0;
    Uint32 s = SDL_GetTicks();
    Mapa mp = mapaAtual(j);
    long tB = f->usaB ? (long)(mp.a * alvo + mp.b) : alvo;
    if (tB < 0) tB = 0;
    int ok = exato ? mExato(f->h, tB, &p, sig, &okS, &jpg, &nj) : mQuadro(f->h, tB, 1, 1, &p, sig, &okS, &jpg, &nj);
    if (falhou(j, ok, f->h)) break;
    if (ok && f->usaB) p = (long)((p - mp.b) / mp.a);                 // back on the playing file's timeline
    long tol = f->usaB && !exato ? TOL_COPIA_MS : TOL_MS;
    if (ok && jpg && labs(p - alvo) <= tol) {
      int dest = (int)((p + GRADE_MS / 2) / GRADE_MS);
      pthread_mutex_lock(&mu);
      if (dest < 0 || dest >= j->n || (j->tiles[dest].jpg && dest != k)) dest = k;
      if (!j->tiles[dest].jpg) {
        j->tiles[dest].jpg = jpg; j->tiles[dest].n = nj; j->tiles[dest].pts = p; j->tiles[dest].revision++; j->tiles[dest].prov = (unsigned char)j->otimista;
        jpg = NULL; j->feitos++;
        if (!j->primeiro) j->primeiro = SDL_GetTicks();
      }
      j->tentado[dest] = 1;
      if (job == j) estado = M_PRONTO;
      pthread_mutex_unlock(&mu);
    }
    free(jpg);
    f->feitos++; f->somaMs += SDL_GetTicks() - s;
    f->lido = mLidos(f->h) - lido0;
    if (f->id == 1 && f->feitos % 100 == 0 && !j->otimista) salvar(j, 0);   // never save unverified (optimistic) thumbnails
    if (f->id == 1 && f->feitos % 30 == 0) {
      printf("[miniatura] %d/%d tiles from %s, lane avg %lu ms, lane read %.1f MB, pace %lld/s, %d batches (%d keyframes), free %ld MB\n",
             j->feitos, j->n, origem, f->somaMs / (unsigned long)f->feitos, f->lido / 1048576.0, mRitmo(), lotes, trazidos, disp / 1024);
      fflush(stdout);
    }
    if (!f->usaB) usleep(20000);
  }
  f->lido = mLidos(f->h) - lido0;
  f->acabou = 1;
  return NULL;
}

static void resumo(Job *j, Uint32 t0) {
  if (j->resumido) return;
  j->resumido = 1;
  long long cont[NCONT];
  mContadores(cont);
  long long nl = cont[5] - j->cont0[5], nk = cont[9] - j->cont0[9];
  char dg[700], dd[700] = "";
  mDiag(0, dg, sizeof dg);
  if (j->direto) mDiagDireto(dd, sizeof dd);
  char marcos[240];
  snprintf(marcos, sizeof marcos, "cpu route %d (%ld ms each, decode %ld ms), first thumbnail %.1f s, 60 s coverage %s%.0f s, 30 s coverage %s%.0f s",
           j->cpuFeitos, j->cpuMs, j->cpuDecMs,
           j->primeiro ? (j->primeiro - t0) / 1000.0 : -1.0, j->cob60 ? "" : "not reached ", j->cob60 ? (j->cob60 - t0) / 1000.0 : 0.0,
           j->cob30 ? "" : "not reached ", j->cob30 ? (j->cob30 - t0) / 1000.0 : 0.0);
  printf("[miniatura] SUMMARY %s | play %dp %.1f min | candidates: %s | chosen: %s | offset %+.2fs, redo %s | "
         "decoders %lld, requests %lld, %.1f MB | time: network %.1f s, frames %.1f s | batches %lld (avg %.1f keyframes, %.2f MB), "
         "keyframe avg %.0f KB | keyframe thumbnails: decode %.1f s, resize+JPEG %.1f s | tiles %d/%d in %.0f s | playback: %d rebuffers, %d player errors | re-checks %d (misses %d), re-anchored %d%s | dropped frames %lld, audio underruns %lld, min free %ld MB, min heap free %ld MB (waits %d) | %s | %s%s%s\n",
         j->titulo, j->alturaA, j->dur / 60000.0, j->motivos, j->escolhida[0] ? j->escolhida : "none", j->offset / 1000,
         j->refeito ? "yes" : "no", cont[1] - j->cont0[1], cont[0] - j->cont0[0], (cont[2] - j->cont0[2]) / 1048576.0,
         (cont[3] - j->cont0[3]) / 1000.0, (cont[4] - j->cont0[4]) / 1000.0, nl,
         nl ? (double)(cont[7] - j->cont0[7]) / nl : 0.0, nl ? (cont[6] - j->cont0[6]) / 1048576.0 / nl : 0.0,
         nk ? (cont[8] - j->cont0[8]) / 1024.0 / nk : 0.0, (cont[10] - j->cont0[10]) / 1000.0, (cont[11] - j->cont0[11]) / 1000.0,
         j->feitos, j->n, (SDL_GetTicks() - t0) / 1000.0, evRebuffer, evErro, j->nRecheck, j->falhasRecheck,
         j->reancoras, j->caiuEm ? ", copy dropped mid-film" : "", mPerdidos() - j->perdidos0, mAudioFalhas() - j->audio0, j->memMin / 1024, j->heapMin, j->esperasHeap, marcos, dg,
         dd[0] ? " | " : "", dd);
  if (j->direto) {
    pthread_mutex_lock(&mu);
    printf("[miniatura] SUMMARY-COVERAGE %s | covered %d/%d targets | unique images %d | complete %s\n",
           j->titulo, mini_coverage_covered_count(&j->coverage), j->n, j->feitos,
           diretoCoverageComplete(j) ? "yes" : "no");
    pthread_mutex_unlock(&mu);
  }
  mPipelineSummary(j, t0);
  fflush(stdout);
}

// The lanes finished (or were stopped): save, raise the "generated" notice when complete, log, summary.
static void fecharLote(Job *j, Uint32 t0, Faixa *f1, Faixa *f2, int faixas) {
  pthread_mutex_lock(&mu);
  int completo = proximo(j) < 0 && (!lacunaPermitida(j) || proximaLacuna(j) < 0) && !parou(j);
  pthread_mutex_unlock(&mu);
  salvar(j, completo);
  if (completo) avisoPronto = j->g;
  printf("[miniatura] done: %d/%d tiles from %s in %.0f s (%d lane%s), read %.1f MB, pace now %lld/s\n", j->feitos, j->n,
         origem, (SDL_GetTicks() - t0) / 1000.0, faixas, faixas == 1 ? "" : "s", (f1->lido + f2->lido) / 1048576.0, mRitmo());
  fflush(stdout);
  resumo(j, t0);
}

// ---- DIRECT MODE: thumbnails from the PLAYING file (4K, experimental) ----
// For a 4K title with no usable smaller copy (or forced by debug.nuvio.miniatura bit 1): ONE lane, ONE hardware
// decoder kept for the title, keyframes fetched in multi-range batches (the socket closed after each, one request in
// flight for the whole engine), scaled on the GPU. Coverage that is useful early comes first:
//   0 the scrub target (+-30 s), 1 around the playhead (+-1 min), 2 the whole film every 60 s, 3 every 30 s,
//   4 the 10 s grid, only while playback shows headroom (no contention seen, 30+ s buffered, memory well above the floor).
// Each target is reduced to the keyframe the extractor lands on (the cue at or before it); a keyframe already made is
// not made again, and each thumbnail is stored under ITS OWN time (a keyframe at 58 s is not labelled 60 s).
// Safety: the player gates of the copy lanes (buffering, reconnect, < 20 s buffered, memory floor); dropped frames
// rising, or the buffer draining (3+ s lost in 10 s, under 8 s), while we decode = contention: the decoder is released
// and work pauses 20 s; the 3rd time ends direct mode.
#define DIR_CONTENCAO_QUADROS 8          // player frames dropped within one check window that count as contention
#define DIR_CONTENCOES_MAX 3
// The thumbnail decoder is created only while playback is PAUSED or the user scrubs/seeks (audio is interrupted
// then anyway). Measured 23/10 on the Shield with TrueHD passthrough: creating a second 4K hardware decoder caused an
// audible HDMI underrun (nvaudio_hw) at 4 of 5 starts, also 8 s into playback. Until then keyframes are only
// downloaded (to disk, MiniaturaMotor) and queued; the first pause/scrub decodes the queue in a burst (~20 ms each).
#define DIR_PENDENTES_MAX 1200           // keyframes queued before the decoder exists: the whole 10 s grid of a 3 h film
static int livreDireto(const Job *j, int k) {
  return k >= 0 && k < j->n && !j->tentado[k] && j->slotState[k] == MINI_COVER_UNRESOLVED;
}
static int proximoDireto(const Job *j, int etapaMax, int *etapa) {
  int n = j->n, ph = (int)(playheadMs / GRADE_MS), sc = (int)((posMs + GRADE_MS / 2) / GRADE_MS);
  if (scrubbing) {
    static const int D0[] = { 0, -3, 3 };
    for (int i = 0; i < 3; i++) if (livreDireto(j, sc + D0[i])) { *etapa = 0; return sc + D0[i]; }
  }
  static const int D1[] = { 0, 3, -3, 6, -6 };
  for (int i = 0; i < 5; i++) if (livreDireto(j, ph + D1[i])) { *etapa = 1; return ph + D1[i]; }
  static const int PASSO[] = { 6, 3, 1 };
  for (int e = 2; e <= etapaMax && e <= 4; e++) {
    int p = PASSO[e - 2];
    for (int d = 0; d <= n; d++) {                 // outward from the playhead, on the stage's spacing
      int a = (ph / p + d) * p, b = (ph / p - d) * p;
      if (a >= n && b < 0) break;
      if (livreDireto(j, a)) { *etapa = e; return a; }
      if (d && livreDireto(j, b)) { *etapa = e; return b; }
    }
  }
  return -1;
}
// Preview the next scheduled targets under the same lock, without claiming
// their work or making JNI/network calls while the UI mutex is held.
static int sondasFuturas(Job *j, int stage, long *times, int capacity) {
  int slots[32], count = 0;
  if (capacity > 32) capacity = 32;
  pthread_mutex_lock(&mu);
  while (count < capacity) {
    int phase, k = proximoDireto(j, stage, &phase);
    if (k < 0) break;
    slots[count] = k; times[count++] = k * GRADE_MS;
    j->tentado[k] = 1;
  }
  for (int i = 0; i < count; i++) j->tentado[slots[i]] = 0;
  pthread_mutex_unlock(&mu);
  return count;
}
// Header candidates only: never claim a target or admit its compressed payload.
// Kotlin selects at most 16 unknown cues from this bounded 64-slot window.
static int sondasDensas(Job *j, long *times, int capacity) {
  int count = 0;
  if (capacity > 64) capacity = 64;
  pthread_mutex_lock(&mu);
  int ph = (int)(playheadMs / GRADE_MS);
  if (ph < 0) ph = 0;
  if (ph >= j->n) ph = j->n - 1;
  for (int d = 0; d < j->n && count < capacity; d++) {
    int a = ph + d, b = ph - d;
    if (a < j->n && a % 3 != 0 && livreDireto(j, a)) times[count++] = a * GRADE_MS;
    if (d && count < capacity && b >= 0 && b % 3 != 0 && livreDireto(j, b)) times[count++] = b * GRADE_MS;
  }
  pthread_mutex_unlock(&mu);
  return count;
}
// Slots of a stage still open (for the coverage milestones).
// A slot of the stage counts as covered when a thumbnail lies within half the stage's spacing of it (the keyframe
// that stands for it carries its own time).
static int restamNaEtapa(const Job *j, int passo) {
  if (j->direto) {
    for (int k = 0; k < j->n; k += passo) if (j->slotState[k] != MINI_COVER_COVERED) return 1;
    return 0;
  }
  for (int k = 0; k < j->n; k += passo) {
    int achou = 0;
    for (int d = -(passo / 2); d <= passo / 2 && !achou; d++) achou = k + d >= 0 && k + d < j->n && j->tiles[k + d].jpg != NULL;
    if (!achou) return 1;
  }
  return 0;
}

#ifdef NV_MINI_FFMPEG
// ---- SMART PER DEVICE: the CPU route's measured cost on THIS device ----
// <data>/miniaturas/perfil-cpu.txt: average ms per CPU thumbnail and samples, for pictures up to 1080p and above.
// While the film plays, downloaded keyframes are decoded on the CPU (Namer03's engine: no hardware decoder, so no
// HDMI underrun) at most ~half of one core; the hardware burst still runs at a confirmed pause. A class whose
// average exceeds its limit (too slow to be worth the CPU next to playback) is skipped on this device until the
// measurement says otherwise (a title that ran it updates the average). debug.nuvio.miniatura bit 32: CPU route off.
#define CPU_MAX_MS_HD 1200
#define CPU_MAX_MS_4K 2500
#define CPU_AMOSTRAS 3                   // thumbnails before a title's own measurement decides
typedef struct { double ms[2]; int n[2]; } PerfilCpu;
static void perfilCaminho(char *out, size_t n) {
  char dir[512];
  if (!dados_caminho(dir, sizeof dir, "miniaturas")) { out[0] = 0; return; }
  snprintf(out, n, "%s/perfil-cpu.txt", dir);
}
static void perfilLer(PerfilCpu *pf) {
  char c[600];
  memset(pf, 0, sizeof *pf);
  perfilCaminho(c, sizeof c);
  FILE *f = c[0] ? fopen(c, "r") : NULL;
  if (!f) return;
  if (fscanf(f, "%lf %d %lf %d", &pf->ms[0], &pf->n[0], &pf->ms[1], &pf->n[1]) != 4) memset(pf, 0, sizeof *pf);
  fclose(f);
}
static void perfilGravar(const PerfilCpu *pf) {
  char c[600], dir[512];
  perfilCaminho(c, sizeof c);
  if (!c[0]) return;
  snprintf(dir, sizeof dir, "%s", c);
  char *b = strrchr(dir, '/');
  if (b) { *b = 0; mkdir(dir, 0700); }
  FILE *f = fopen(c, "w");
  if (!f) return;
  fprintf(f, "%.0f %d %.0f %d\n", pf->ms[0], pf->n[0], pf->ms[1], pf->n[1]);
  fclose(f);
}
// A finished direct-mode thumbnail into the set (freed when the slot is taken already).
static void guardarTileDireto(Job *j, long p, unsigned char *jpg, int nj, int *feitosAqui) {
  pthread_mutex_lock(&mu);
  int before = j->feitos;
  diretoGuardarFrame(j, p, &jpg, nj);
  *feitosAqui += j->feitos - before;
  if (!j->cob60 && !restamNaEtapa(j, 6)) j->cob60 = SDL_GetTicks();
  if (!j->cob30 && !restamNaEtapa(j, 3)) j->cob30 = SDL_GetTicks();
  pthread_mutex_unlock(&mu);
  free(jpg);
}

// THE CPU THREAD: decodes queued keyframes while the download loop goes on (measured 23/10: run in turns, the queue
// was empty at the first scrub and downloads waited behind the CPU). Oldest first (the queue follows the coverage
// stages); paused while the hardware decoder exists or the player is paused/scrubbing/buffering; full speed, slower
// for a minute after contention. Fonte's entry points are synchronised on the Kotlin side.
typedef struct CpuFaixa {
  Job *j; long long h; MiniMotor *cpu; long *pend; int *nPend; pthread_mutex_t *mp;
  _Atomic int pausa, fim, desligar, recusou, cuidado;
  long soma, decSoma, limite; int falhas, feitosAqui, semBloco, threads;
  struct CpuFaixa *pai;      // a sibling worker (copy-first): its parent's flags rule it
  MiniMotor *cpu2;           // the sibling's own decoder, opened with the parent's (closed by the parent)
} CpuFaixa;
static void *cpuTrabalho(void *u) {
  CpuFaixa *cf = u;
  Job *j = cf->j;
  CpuFaixa *ctl = cf->pai ? cf->pai : cf;   // a sibling follows its parent's pause/stop flags
  // COPY-FIRST: a copy's whole grid goes through the CPU, so a second decoder thread takes from the same queue
  // (06/11/2026: one thread at ~60 ms per 720p keyframe left decode on the critical path for 30 of 107 s)
  CpuFaixa *irmao = NULL; pthread_t irmaoT; int irmaoRodando = 0;
  if (!cf->pai && j->dCopia) {
    irmao = malloc(sizeof *irmao);
    if (irmao) {
      memset(irmao, 0, sizeof *irmao);
      irmao->j = j; irmao->h = cf->h; irmao->pend = cf->pend; irmao->nPend = cf->nPend; irmao->mp = cf->mp;
      irmao->limite = cf->limite; irmao->threads = 1; irmao->pai = cf;
      irmao->cpu = cf->cpu2;
      if (irmao->cpu) irmaoRodando = pthread_create(&irmaoT, NULL, cpuTrabalho, irmao) == 0;
      if (!irmaoRodando) { free(irmao); irmao = NULL; }
    }
  }
  while (!ctl->fim && !parou(j)) {
    if (ctl->pausa || ctl->desligar) { usleep(200000); continue; }
    long disp = memDisponivelKB(), precisaKB = (long)(mini_motor_memoria(cf->cpu, cf->threads) / 1024);
    if (disp >= 0 && disp - precisaKB < PISO_KB) { usleep(1000000); continue; }
    pthread_mutex_lock(cf->mp);
    if (*cf->nPend <= 0) { pthread_mutex_unlock(cf->mp); usleep(200000); continue; }
    long kf = cf->pend[0], p = 0;
    memmove(cf->pend, cf->pend + 1, sizeof *cf->pend * (size_t)(--*cf->nPend));
    pthread_mutex_unlock(cf->mp);
    unsigned char *pk = NULL, *jpg = NULL; int npk = 0, nj = 0; MiniQuadro q; char e[96] = "";
    int r = -1;
    if (!mPacoteCpu(cf->h, kf, &p, &pk, &npk)) {
      // no block to hand over (a layout the block reader does not take, measured 23/10 on a 1080p episode): the
      // hardware path reads it through the extractor at a confirmed pause. Not a CPU failure.
      if (mFalhaRede(cf->h) == 2) ctl->recusou = 1;
      pthread_mutex_lock(cf->mp);
      cf->pend[(*cf->nPend)++] = kf;
      int soEsses = ++cf->semBloco >= *cf->nPend;
      pthread_mutex_unlock(cf->mp);
      usleep(soEsses ? 2000000 : 20000);
      continue;
    }
    cf->semBloco = 0;
    // A packet read may have blocked across a pause/handover. Do not begin a
    // fresh decode after admission was revoked; preserve the queued keyframe.
    if (ctl->fim || ctl->pausa || parou(j)) {
      free(pk);
      pthread_mutex_lock(cf->mp);
      cf->pend[(*cf->nPend)++] = kf;
      pthread_mutex_unlock(cf->mp);
      continue;
    }
    mPipelineState(5, 1);
    r = mini_motor_pacote(cf->cpu, pk, npk, cf->threads, &jpg, &nj, &q, e, sizeof e);
    mPipelineState(5, 0);
    free(pk);
    if (r < 0) {
      pthread_mutex_lock(cf->mp);
      cf->pend[(*cf->nPend)++] = kf;              // the hardware burst takes it later
      pthread_mutex_unlock(cf->mp);
      if (++cf->falhas >= 5 && !ctl->desligar) { ctl->desligar = 1; printf("[miniatura] cpu route: 5 failures (%s), off for this title\n", e); fflush(stdout); }
      usleep(200000);
      continue;
    }
    __atomic_add_fetch(&j->cpuFeitos, 1, __ATOMIC_RELAXED); __atomic_add_fetch(&ctl->soma, (long)q.totalMs, __ATOMIC_RELAXED); __atomic_add_fetch(&ctl->decSoma, (long)q.decodMs, __ATOMIC_RELAXED);
    j->cpuMs = ctl->soma / j->cpuFeitos; j->cpuDecMs = ctl->decSoma / j->cpuFeitos;
    if (j->cpuFeitos <= 3 || j->cpuFeitos % 50 == 0) {
      printf("[miniatura] cpu route: %ld s in %u ms (decode %u ms, resize %u ms, colour+pack %u ms, JPEG %u ms, keyframe %ld KB), %d made\n", p / 1000, q.totalMs,
             q.decodMs, q.resizeMs, q.colourMs, q.jpegMs, q.bytesPacote / 1024, j->cpuFeitos);
      fflush(stdout);
    }
    guardarTileDireto(j, dParaPlay(j, p), jpg, nj, &cf->feitosAqui);
    if (j->cpuFeitos == CPU_AMOSTRAS && j->cpuMs > cf->limite) {
      ctl->desligar = 1;
      printf("[miniatura] cpu route: %ld ms per thumbnail on this device (> %ld): off, hardware burst at a confirmed pause\n",
             j->cpuMs, cf->limite);
      fflush(stdout);
    }
    // Full speed (30 ms between tiles, as Namer03's engine runs on TVs): no measured strain ever came from CPU
    // decoding (23/10 runs: 0 underruns, 0 dropped frames; the hitches were hardware-decoder creation and GC). Half
    // the frame's time as a rest only for a minute after real contention (dropped frames / audio underrun).
    usleep(ctl->cuidado ? (useconds_t)q.totalMs * 500 : 30000);
  }
  if (irmaoRodando) { pthread_join(irmaoT, NULL); j->cpuFeitos += 0; }
  if (irmao) free(irmao);
  if (!cf->pai && cf->cpu2) { mini_motor_fechar(cf->cpu2); cf->cpu2 = NULL; }
  mini_dovi_gpu_close(); // release on the GL-owning CPU worker before it exits
  return NULL;
}
#endif

// ---- PRE-WARM: the direct session and its hardware decoder while the title OPENS ----
// The thumbnail job can only start once the player knows the duration, i.e. once playback runs (measured 23/10:
// opened 13:53:37.7, playing 13:53:40.7, job 13:53:41.7): too late to create the decoder in silence. So the player
// calls miniatura_preaquecer() while it opens; this thread opens the file and creates the decoder before the audio
// starts, and trabalharDireto adopts the handle for the same link.
static pthread_mutex_t muPre = PTHREAD_MUTEX_INITIALIZER;
static char preUrl[4096];
static long long preH;
static int preDec, preRodando;
typedef struct { char url[4096], cab[512]; } PreArgs;
static void *preTrabalho(void *u) {
  PreArgs *a = u;
  Uint32 t0 = SDL_GetTicks();
  long long h = mAbrirDireto(a->url, a->cab);
  int dec = 0;
  if (h && !video_pronto() && !video_tocando() && !video_falhou()) dec = mPrepararDecoder(h);
  printf("[miniatura] pre-warm: file %s, hardware decoder %s (%u ms, playback %s)\n", h ? "open" : "not usable",
         dec ? "created" : "not created", SDL_GetTicks() - t0, video_tocando() ? "running" : "not started yet");
  fflush(stdout);
  pthread_mutex_lock(&muPre);
  if (!strcmp(preUrl, a->url) && !preH) { preH = h; preDec = dec; h = 0; }
  preRodando = 0;
  pthread_mutex_unlock(&muPre);
  if (h) mFechar(h);
  free(a);
  return NULL;
}
void miniatura_preaquecer(const char *url, const char *cabecalhos) {
  if (bitsDebug() & 1024) return; // playback-only diagnostic, no thumbnail open/decoder
  if (!url || strncmp(url, "http", 4)) return;
  pthread_mutex_lock(&muPre);
  if (preRodando || !strcmp(preUrl, url)) { pthread_mutex_unlock(&muPre); return; }
  long long velho = preH;
  preH = 0; preDec = 0;
  snprintf(preUrl, sizeof preUrl, "%s", url);
  pthread_mutex_unlock(&muPre);
  if (velho) mFechar(velho);                    // a title that never started its job
  if (bitsDebug() & 8 || !jniPreparar()) return;
  mConfigurar(bitsDebug());
  // This entry point is called by the SDL/render thread while opening the player.
  if (bitsDebug() & 131072) { tex_aliviar(); printf("[miniatura] hardware preparation: reclaimed optional artwork caches\n"); fflush(stdout); }
  PreArgs *a = calloc(1, sizeof *a);
  if (!a) return;
  snprintf(a->url, sizeof a->url, "%s", url);
  snprintf(a->cab, sizeof a->cab, "%s", cabecalhos ? cabecalhos : "");
  pthread_mutex_lock(&muPre); preRodando = 1; pthread_mutex_unlock(&muPre);
  pthread_t t; pthread_attr_t at;
  pthread_attr_init(&at); pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);
  if (pthread_create(&t, &at, preTrabalho, a)) { free(a); pthread_mutex_lock(&muPre); preRodando = 0; pthread_mutex_unlock(&muPre); }
  pthread_attr_destroy(&at);
}
// The pre-warmed session for this link (waits up to 5 s for one still being made); *dec = its decoder exists.
static long long adotarPre(Job *j, int *dec) {
  long long h = 0;
  for (int i = 0; i < 50; i++) {
    pthread_mutex_lock(&muPre);
    int mesmo = !strcmp(preUrl, j->urlA), rodando = preRodando;
    if (mesmo && !rodando && preH) { h = preH; *dec = preDec; preH = 0; }
    pthread_mutex_unlock(&muPre);
    if (!mesmo || !rodando || parou(j)) break;
    dormir(j, 100);
  }
  return h;
}

// 1 = direct mode ran (whatever it made), 0 = the playing file cannot be used directly (the copy route then tries).
static int trabalharDireto(Job *j, long long *hA) {
  char motivo[96] = "";
  int decPre = 0;
  if (!*hA && !j->dCopia) *hA = adotarPre(j, &decPre);
  if (*hA) { printf("[miniatura] direct: using the pre-warmed session (decoder %s)\n", decPre ? "ready" : "not created"); fflush(stdout); }
  if (!*hA) *hA = j->dCopia ? mAbrirDireto(j->dUrl, j->dCab) : mAbrirDireto(j->urlA, j->cabA);
  if (!*hA) {
    printf("[miniatura] direct: the %s cannot be used (see the engine log)%s\n", j->dCopia ? "checked copy" : "playing file",
           j->dCopia ? "" : ": trying the copies");
    fflush(stdout);
    return 0;
  }
  pthread_mutex_lock(&mu); j->direto = 1; pthread_mutex_unlock(&mu);
  if (j->dCopia) {
    snprintf(origem, sizeof origem, "%dp copy (fast)", j->dAltura);
    printf("[miniatura] copy-first: direct engine on the %dp copy, t_copy = %.4f x t_play %+.2fs\n", j->dAltura, j->dA, j->dB / 1000);
    fflush(stdout);
  } else {
    snprintf(origem, sizeof origem, "playing %dp (direct)", j->alturaA);
    snprintf(j->escolhida, sizeof j->escolhida, "playing file %dp (direct, GPU)", j->alturaA);
  }
  snprintf(j->arq, sizeof j->arq, "%s", j->arqDireto);
  if (carregarInformado(j)) { avisoPronto = j->g; return 1; }
  falhasSeguidas = 0; decodSeguidas = 0;
  long long perdAntes = mPerdidos();
  Uint32 janela = SDL_GetTicks();
  double folgaAntes = folgaSeg(j);
  long long audioAntes = mAudioFalhas();
  int decodificou = 0, nPend = 0, avisouEspera = 0, avisouGrade = 0;
  MiniPauseAdmission admissao = {0};
  // THE HARDWARE DECODER AS EARLY AS POSSIBLE: during the player's opening load, before its audio starts, the
  // HDMI underrun that creating it causes (measured 23/10) falls into silence. Then every thumbnail goes through the
  // hardware + GPU (~30/s) from the start, no scrub needed. Once playback runs, creation waits for a confirmed pause.
  if (decPre) decodificou = 1;
  else if (!video_pronto() && !video_tocando() && !video_falhou()) {
    Uint32 c0 = SDL_GetTicks();
    decodificou = mPrepararDecoder(*hA);
    printf("[miniatura] direct: hardware decoder %s during the opening load (%u ms, playback %s)\n",
           decodificou ? "created" : "NOT created", SDL_GetTicks() - c0, video_tocando() ? "had started" : "not started yet");
  } else printf("[miniatura] direct: playback already running: CPU route until a confirmed pause\n");
  fflush(stdout);
  Uint32 ultimaContencao = 0;
  pthread_mutex_t mp = PTHREAD_MUTEX_INITIALIZER;   // the queue (pend/nPend), shared with the CPU thread
#ifdef NV_MINI_FFMPEG
  PerfilCpu pf;
  perfilLer(&pf);
  long long fonteInfo[6] = {0};
  mInfo(*hA, fonteInfo);
  int classe = (fonteInfo[2] > 0 ? fonteInfo[2] : j->alturaA) > 1080, cpuCod = -1, cpuRodando = 0;
  pthread_t cpuT;
  CpuFaixa cf;
  memset(&cf, 0, sizeof cf);
  long cpuLimite = classe ? CPU_MAX_MS_4K : CPU_MAX_MS_HD;
  int cpuUsar = !(bitsDebug() & 32) && (pf.n[classe] < CPU_AMOSTRAS || pf.ms[classe] <= cpuLimite);
  printf("[miniatura] cpu route: %s (device profile: %s %.0f ms over %d thumbnails)\n",
         cpuUsar ? "on while playing" : "off on this device", classe ? ">1080p" : "<=1080p", pf.ms[classe], pf.n[classe]);
  fflush(stdout);
#endif
  long *pend = malloc(sizeof(long) * (size_t)j->n);
  if (!pend) { printf("[miniatura] direct: out of memory\n"); fflush(stdout); return 1; }
  // Account for zero too: use an early positive sync frame or record an explicit gap.
  int etapaMax = 3, lotes = 0, feitosAqui = 0, erros0 = evErro;
  long fila[LOTE];
  printf("[miniatura] direct: thumbnails from the playing file, stages scrub > playhead > 60 s > 30 s > 10 s\n"); fflush(stdout);
  while (!parou(j)) {
    if (j->dCopia && j->dCheck == 3) { snprintf(motivo, sizeof motivo, "copy rejected by the screen check"); break; }
    if (evErro != erros0) {                       // the player failed while we were decoding: never risk it again
      snprintf(motivo, sizeof motivo, "player error during direct mode");
      break;
    }
    // The decoder is KEPT through ordinary waits: creating a second 4K hardware decoder is what disturbed playback.
    // Measured 23/10 on the Shield (TrueHD passthrough): HDMI audio underruns (nvaudio_hw) 0.35 s and 0.9 s after the
    // thumbnail decoder was created, none while it decoded for minutes. Released only when the player is in trouble
    // (reconnecting/failed), memory is at the floor, on contention, or at the end.
#ifdef NV_MINI_FFMPEG
    // Update before every early wait, not below the hardware branch's continue.
    cf.pausa = mDecoderPronto(*hA) || scrubbing || bufferando || !video_tocando() ||
               video_reconectando() || video_falhou() || folgaBaixa(j);
#endif
    int pausaAdmitida = mini_pause_admit(&admissao, SDL_GetTicks(), video_pausa_confirmada(), scrubbing, bufferando);
    if (video_reconectando() || video_falhou()) { mSoltar(*hA); decodificou = 0; esperarPlayer(j); continue; }
    if (bufferando || folgaBaixa(j)) { esperarPlayer(j); continue; }
    long disp = memDisponivelKB();
    if (disp >= 0 && (j->memMin < 0 || disp < j->memMin)) j->memMin = disp;
    if (disp >= 0 && disp < PISO_KB) {
#ifdef NV_MINI_FFMPEG
      cf.pausa = 1;
      if (cpuRodando) { cf.fim = 1; pthread_join(cpuT, NULL); cpuRodando = 0; }
      if (cf.cpu) { mini_motor_fechar(cf.cpu); cf.cpu = NULL; }
      cf.fim = 0;
#endif
      if (mDecoderPronto(*hA)) printf("[miniatura] direct: memory floor: releasing hardware, no automatic recreation\n");
      mSoltar(*hA); decodificou = 0; dormir(j, 1500); continue;
    }
    if (heapApertado(j)) {
#ifdef NV_MINI_FFMPEG
      cf.pausa = 1;
#endif
      dormir(j, 1500); continue;
    }
    // an audible gap while we work: back off at once, as for dropped frames
    if (mAudioFalhas() != audioAntes) {
      audioAntes = mAudioFalhas();
      j->contencoes++; ultimaContencao = SDL_GetTicks();
      printf("[miniatura] direct: player audio underrun while decoding: releasing the decoder, pause 20 s (%d/%d)\n",
             j->contencoes, DIR_CONTENCOES_MAX);
      fflush(stdout);
      mSoltar(*hA);
      if (j->contencoes >= DIR_CONTENCOES_MAX) { snprintf(motivo, sizeof motivo, "playback contention (audio)"); break; }
      etapaMax = 3;
#ifdef NV_MINI_FFMPEG
      cf.pausa = 1;
#endif
      dormir(j, 20000);
      audioAntes = mAudioFalhas(); perdAntes = mPerdidos(); janela = SDL_GetTicks(); folgaAntes = folgaSeg(j);
      continue;
    }
    // contention: the player dropping frames while we decode
    if (SDL_GetTicks() - janela >= 10000u) {
      long long agora = mPerdidos(), d = agora - perdAntes;
      double f = folgaSeg(j);
      int caindo = f >= 0 && folgaAntes >= 0 && folgaAntes - f >= 3.0 && f < folgaQuer(j, 8.0, 0.6);   // draining while we read
      perdAntes = agora; janela = SDL_GetTicks(); folgaAntes = f;
      // a buffer dip alone is logged, not counted: on a 4K remux the seconds swing with the bitrate under the player's
      // 80 MB cap (measured 23/10: "draining" at 6-8 s with 0 dropped frames stopped the 10 s grid for the film)
      if (caindo && d < DIR_CONTENCAO_QUADROS && !scrubbing) printf("[miniatura] direct: player buffer dipped to %.1f s (not counted)\n", f);
      if (d >= DIR_CONTENCAO_QUADROS && !scrubbing && !mSeekRecente()) {
        j->contencoes++; ultimaContencao = SDL_GetTicks();
        printf("[miniatura] direct: %s while decoding: releasing the decoder, pause 20 s (%d/%d)\n",
               caindo ? "player buffer draining" : "player dropping frames", j->contencoes, DIR_CONTENCOES_MAX);
        printf("[miniatura] direct: dropped %lld frames in 10 s, buffer %.1f s\n", d, f);
        fflush(stdout);
        mSoltar(*hA);
        if (j->contencoes >= DIR_CONTENCOES_MAX) { snprintf(motivo, sizeof motivo, "playback contention"); break; }
        etapaMax = 3;
  #ifdef NV_MINI_FFMPEG
      cf.pausa = 1;
#endif
      dormir(j, 20000);
        perdAntes = mPerdidos(); janela = SDL_GetTicks(); folgaAntes = folgaSeg(j);
        continue;
      }
    }
    // Actual Kotlin state is authoritative, including timeout/error releases.
    decodificou = mDecoderPronto(*hA);
    if (pausaAdmitida && !decodificou) {
#ifdef NV_MINI_FFMPEG
      // No hardware allocation until the current CPU decode has returned and its
      // reference buffers are freed. A pause flag alone cannot quiesce in-flight work.
      cf.pausa = 1;
      if (cpuRodando) { cf.fim = 1; pthread_join(cpuT, NULL); cpuRodando = 0; }
      if (cf.cpu) { mini_motor_fechar(cf.cpu); cf.cpu = NULL; }
      cf.fim = 0;
#endif
      if (!parou(j) && video_pausa_confirmada() && !scrubbing && !bufferando && !video_reconectando()) {
        decodificou = mPrepararDecoder(*hA);
        printf("[miniatura] direct: exclusive paused handover: hardware %s\n", decodificou ? "ready" : "not admitted; CPU fallback");
        fflush(stdout);
      }
    }
    int liberado = decodificou;
    if (liberado && nPend > 0) {
      // the queued keyframe nearest to where the user is (scrub target, else the playhead) first
      long onde = dParaCopia(j, scrubbing ? posMs : playheadMs), melhor = -1;   // pend holds copy-time keyframes
      int iM = -1;
      pthread_mutex_lock(&mp);
      if (nPend <= 0) { pthread_mutex_unlock(&mp); continue; }
      for (int i = 0; i < nPend; i++) {
        long d = labs(pend[i] - onde); if (melhor < 0 || d < melhor) { melhor = d; iM = i; }
      }
      if (iM < 0) { pthread_mutex_unlock(&mp); dormir(j,20); continue; }
      long kf = pend[iM];
      pend[iM] = pend[--nPend];
      pthread_mutex_unlock(&mp);
      long p; unsigned char *jpg = NULL; int nj = 0;
      int ok = mQuadroDireto(*hA, kf, &p, &jpg, &nj);
      if (ok) p = dParaPlay(j, p);
      decodificou = mDecoderPronto(*hA);
      if (!ok || !jpg) {
        pthread_mutex_lock(&mu);
        for (int k = 0; k < j->n; k++) if (j->slotKey[k] == kf) mini_coverage_mark(&j->coverage, k, MINI_COVER_FAILED);
        pthread_mutex_unlock(&mu);
      }
      if (falhou(j, ok, *hA)) { free(jpg); snprintf(motivo, sizeof motivo, "reads/decodes failing"); break; }
      if (ok && jpg) {
        pthread_mutex_lock(&mu);
        int before = j->feitos;
        int dest = diretoGuardarFrame(j, p, &jpg, nj);
        feitosAqui += j->feitos - before;
        for (int k = 0; k < j->n; k++) if (j->slotKey[k] == kf && j->slotState[k] == MINI_COVER_PENDING)
          mini_coverage_mark(&j->coverage, k, MINI_COVER_FAILED);
        if (!j->cob60 && !restamNaEtapa(j, 6)) j->cob60 = SDL_GetTicks();
        if (!j->cob30 && !restamNaEtapa(j, 3)) j->cob30 = SDL_GetTicks();
        pthread_mutex_unlock(&mu);
        prepararPixelsDireto(j, *hA, dest, p);
      }
      free(jpg);
      if (feitosAqui && feitosAqui % 10 == 0) {
        pthread_mutex_lock(&mu); int covered = mini_coverage_covered_count(&j->coverage); pthread_mutex_unlock(&mu);
        printf("[miniatura] direct: coverage %d/%d targets, %d images, %d queued, %d batches, buffered %.1f s, dropped +%lld, heap free %ld MB, pace %lld/s\n",
               covered, j->n, j->feitos, nPend, lotes, folgaSeg(j), mPerdidos() - j->perdidos0, mHeapLivre(), mRitmo());
        fflush(stdout);
      }
      if (feitosAqui && feitosAqui % 50 == 0 && j->dCheck != 1 && j->dCheck != 3) salvar(j, 0);   // never an unverified copy
      if (j->cob30 && !j->resumido) resumo(j, j->t0);
      continue;
    }
#ifdef NV_MINI_FFMPEG
    // the CPU thread works only while the film plays and the hardware decoder does not exist
    cf.pausa = liberado || scrubbing || bufferando || !video_tocando();
    cf.cuidado = j->contencoes && SDL_GetTicks() - ultimaContencao < 60000u;
    if (cf.recusou) { snprintf(motivo, sizeof motivo, "source refused a connection"); break; }
    if (!cpuRodando && !liberado && cpuUsar && nPend > 0) {
      if (cpuCod < 0) cpuCod = mCodecCpu(*hA);
      if (cpuCod > 0) {
        long long inf[6]; char e[96] = "";
        mInfo(*hA, inf);
        cf.cpu = mini_motor_abrir_pacotes(cpuCod != 2, (int)inf[1], (int)inf[2], cpuCod == 3 ? 2 : inf[3] != 0, e, sizeof e);
        if (cf.cpu) {
          MiniConfig config; mini_motor_config_padrao(&config);
          config.larguraSaida = j->larguraSaida;
          mini_motor_config(cf.cpu, &config);
          if (j->dCopia && !classe) {           // copy-first: a second decoder for the sibling worker
            char e2[96] = "";
            cf.cpu2 = mini_motor_abrir_pacotes(cpuCod != 2, (int)inf[1], (int)inf[2], cpuCod == 3 ? 2 : inf[3] != 0, e2, sizeof e2);
            if (cf.cpu2) mini_motor_config(cf.cpu2, &config);
          }
          cf.j = j; cf.h = *hA; cf.pend = pend; cf.nPend = &nPend; cf.mp = &mp; cf.limite = cpuLimite;
          // 2 slice/WPP threads for pictures above 1080p (Shield: one A57 core took ~400 ms per 4K keyframe; x265
          // streams are WPP-coded, so rows decode in parallel), 1 below (~95 ms already)
          cf.threads = classe ? 2 : 1;
          cpuRodando = pthread_create(&cpuT, NULL, cpuTrabalho, &cf) == 0;
        }
        printf("[miniatura] cpu route: %s%s\n", cpuRodando ? "thread started" : "not available ", cpuRodando ? "" : e); fflush(stdout);
      } else printf("[miniatura] cpu route: this file's blocks cannot be taken directly (hardware burst only)\n");
      fflush(stdout);
      if (!cpuRodando) cpuUsar = 0;
    }
    if (j->cob30 && !j->resumido) resumo(j, j->t0);
#endif
    if (!liberado && nPend >= DIR_PENDENTES_MAX) {   // enough downloaded: wait for a confirmed pause
      if (!avisouEspera) { printf("[miniatura] direct: %d targets queued (not a download count), waiting for decoder admission\n", nPend); fflush(stdout); avisouEspera = 1; }
      dormir(j, 500);
      continue;
    }
    // claim a batch of distinct keyframes
    int nf = 0, etapa = 9, prioridadeCurta = 0;
    int quer = scrubbing ? 2 : mLote(*hA);
    pthread_mutex_lock(&mu);
    while (nf < quer) {
      int e, k = proximoDireto(j, etapaMax, &e);
      if (k < 0) break;
      j->tentado[k] = 1; // claim only; a completed image binding proves coverage
      mini_coverage_mark(&j->coverage, k, MINI_COVER_PENDING);
      pthread_mutex_unlock(&mu);
      long kf = mSlotKeyframe(*hA, dParaCopia(j, (long)mini_coverage_target_ms(&j->coverage, k)));
      pthread_mutex_lock(&mu);
      if (kf == -2 && k > 0) kf = dParaCopia(j, k * GRADE_MS); // retain non-indexed extractor fallback
      j->slotKey[k] = kf;
      if (kf < 0) { mini_coverage_mark(&j->coverage, k, MINI_COVER_UNAVAILABLE); continue; }
      if (j->slotState[k] == MINI_COVER_COVERED) continue;
      int repetido = 0;
      for (int i = 0; i < nf && !repetido; i++) repetido = fila[i] == kf;
      pthread_mutex_lock(&mp);
      for (int i = 0; i < nPend && !repetido; i++) repetido = pend[i] == kf;
      pthread_mutex_unlock(&mp);
      if (repetido) continue;                     // same random-access frame as one made or queued
      if (e < etapa) etapa = e;
      fila[nf] = kf; nf++;
      if (e <= 1 && nf >= 2) { prioridadeCurta = 1; break; }               // near targets: small batches, quick answers
    }
    pthread_mutex_unlock(&mu);
    if (!nf) {
      if (nPend > 0 && etapaMax >= 4) {             // everything downloaded: the queue waits for the decoder (or the CPU)
        if (!liberado && !avisouEspera && nPend > 0) {
          printf("[miniatura] direct: %d targets queued (not a download count), waiting for decoder admission\n", nPend); fflush(stdout); avisouEspera = 1;
        }
        dormir(j, 500);
        continue;
      }
      // nothing open up to this stage: the 10 s grid only with headroom, else wait for scrubbing/headroom
      double folga = folgaSeg(j);
      // Existing hardware needs no new decoder allocation to refine the grid.
      // Recheck actual state: a safety release must not inherit this admission.
      int hardwareReady = mDecoderPronto(*hA);
      int shared = mHeadroom(j, 8.0);
      int memOk = mini_refine_memory_ok(disp, PISO_KB, hardwareReady);
      int folgaOk = shared >= 0 ? shared : folga < 0 || folga >= folgaQuer(j, 8.0, 0.7);
      int quieto = !j->contencoes || SDL_GetTicks() - ultimaContencao > 60000u;   // 60 s without contention
      if (etapaMax < 4 && quieto && folgaOk && memOk) {
        etapaMax = 4;
        printf("[miniatura] direct: headroom (%.0f s buffered, %ld MB free, hardware %s): refining to the 10 s grid\n", folga, disp / 1024, hardwareReady ? "ready" : "absent"); fflush(stdout);
        continue;
      }
      if (etapaMax >= 4) break;                   // everything made
      if (!avisouGrade) {                         // never a silent stall
        printf("[miniatura] direct: 10 s grid waiting (%s%s%s)\n", !quieto ? "playback contention in the last minute " : "",
               folgaOk ? "" : "player buffer low ", memOk ? "" : "memory low");
        fflush(stdout);
        avisouGrade = 1;
      }
      dormirGate(j, 3000);                        // the decoder stays: waiting for a scrub or for headroom
      continue;
    }
    printf("[miniatura] batch-plan: frames=%d target=%d stage=%d reason=%s\n", nf, quer, etapa,
           prioridadeCurta ? "playhead-priority" : nf >= quer ? "frame-or-byte-budget" : "available-distinct-targets");
    fflush(stdout);
    esperarFolga(j);
    if (nf > 1) {
      long future[32];
      int bridge = j->headerBridge && !scrubbing && etapa >= 2 && etapaMax < 4;
      // Stage 1 (around the playhead) only for own lanes (06/11/2026 R2: 11 of 25 batches had no look-ahead).
      int count = !scrubbing && (etapa >= 2 || (etapa == 1 && mLanesProprias(*hA))) ? sondasFuturas(j, etapaMax, future, (bitsDebug() & 65536) ? 32 : 16) : 0;
      long headers[64];
      int nHeaders = 0;
      for (int i = 16; i < count; i++) headers[nHeaders++] = future[i];
      if (bridge) nHeaders += sondasDensas(j, headers + nHeaders, 64 - nHeaders);
      dMapear(j, future, count); dMapear(j, headers, nHeaders);   // play-time slots -> copy time
      int payloadCount = resolverSondas(*hA, future, count < 16 ? count : 16);
      nHeaders = resolverSondas(*hA, headers, nHeaders);
      int r = mPrepararSondas(*hA, fila, nf, future, payloadCount, headers, nHeaders);
      if (r == -2) { snprintf(motivo, sizeof motivo, "source refused a connection"); break; }
      if (r >= 0) lotes++;
    }
    pthread_mutex_lock(&mp);
    for (int i = 0; i < nf; i++) pend[nPend++] = fila[i];
    pthread_mutex_unlock(&mp);
    if (j->cob30 && !j->resumido) resumo(j, j->t0);   // a SUMMARY once the film has 30 s coverage (another at the end)
  }
#ifdef NV_MINI_FFMPEG
  if (cpuRodando) { cf.fim = 1; pthread_join(cpuT, NULL); }
  if (cf.cpu) mini_motor_fechar(cf.cpu);
#endif
  free(pend);
#ifdef NV_MINI_FFMPEG
  if (j->cpuFeitos >= CPU_AMOSTRAS) {
    int n = pf.n[classe] > 200 ? 200 : pf.n[classe];
    pf.ms[classe] = (pf.ms[classe] * n + (double)j->cpuMs * j->cpuFeitos) / (n + j->cpuFeitos);
    pf.n[classe] = n + j->cpuFeitos;
    perfilGravar(&pf);
    printf("[miniatura] cpu route: %d thumbnails, %ld ms each (decode %ld ms); device profile now %.0f ms over %d\n",
           j->cpuFeitos, j->cpuMs, j->cpuDecMs, pf.ms[classe], pf.n[classe]);
    fflush(stdout);
  }
#endif
  mSoltar(*hA);
  if (j->dCheckRodando) {                          // the copy is only kept once the check said so
    if (j->dCheck == 1) { printf("[miniatura] copy-first: engine done, waiting for the screen check\n"); fflush(stdout); }
    pthread_join(j->dCheckT, NULL);
    j->dCheckRodando = 0;
  }
  if (j->dCopia && j->dCheck == 3) {
    printf("[miniatura] direct: copy pass stopped (%d images discarded)\n", j->feitos); fflush(stdout);
    j->resumido = 0;
    return 1;
  }
  pthread_mutex_lock(&mu);
  if (!nPend) for (int k = 0; k < j->n; k++) if (j->slotState[k] == MINI_COVER_PENDING)
    mini_coverage_mark(&j->coverage, k, MINI_COVER_FAILED); // no decode remains to supply this binding
  int completo = etapaMax >= 4 && diretoCoverageComplete(j) && !parou(j);
  pthread_mutex_unlock(&mu);
  salvar(j, completo);                             // sparse coverage is saved as NOT complete
  // "Thumbnails generated" when the pass FINISHED, not only when every 10 s slot got one (06/11/2026: 142/143, or a
  // keyframe only every 30 s, 45/138, never showed it). A pass stopped by a refusal or a decode failure still does not.
  if (completo || (!motivo[0] && !parou(j))) avisoPronto = j->g;
  printf("[miniatura] direct: %s, %d unique images%s%s\n", completo ? "done" : "partial", j->feitos, motivo[0] ? ", stopped: " : "", motivo); fflush(stdout);
  diretoCoberturaRelatorio(j);
  j->resumido = 0;
  return 1;
}

// Copy-first: the first copy (smallest first, escolherCopias) whose header is usable and whose runtime is within
// TELA_DUR_IGUAL_MS passes the screen check (what the player shows against the copy's exact frames: no 4K decode).
// 1 = j->dCopia/dA/dB/dUrl set to it. The check's own session is closed; the engine opens the RESOLVED link (alvo), so
// no second debrid link is minted.
// The screen check on its own thread while the engine already makes thumbnails at offset 0. Accepted: every tile is
// relabelled to the measured offset (exact: a tile's copy time is its play time + the old offset). Rejected: the
// engine stops and trabalhar() discards the copy's tiles.
typedef struct { Job *j; long long hB; long durB; } CheckCopia;
static void fonteAnotar(const char *chave, int aceita);
static void *checarCopia(void *u) {
  CheckCopia *c = u;
  Job *j = c->j;
  Uint32 c0 = SDL_GetTicks();
  Mapa m = {1.0, 0.0, 0.0, 0};
  int r = conferirTela(j, c->hB, c->durB, &m);
  mFechar(c->hB);
  pthread_mutex_lock(&mu);
  if (r == 1) {
    double antigo = j->dB;
    int n = 0;
    for (int k = 0; k < j->n; k++) {
      Tile *t = &j->tiles[k];
      if (!t->jpg) continue;
      t->pts = (long)(t->pts + antigo - m.b); t->revision++; t->salvo = 0; n++;
    }
    j->dB = m.b; j->offset = m.b; j->appendPronto = 0;
    diretoRebuildCoverage(j);
    j->dCheck = 2;
    printf("[miniatura] copy-first: check ACCEPTED in %.1f s while generating: %d thumbnails relabelled by %+.2fs\n",
           (SDL_GetTicks() - c0) / 1000.0, n, -(m.b - antigo) / 1000);
  } else {
    j->dCheck = 3;
    printf("[miniatura] copy-first: check %s after %.1f s: the copy's thumbnails are discarded\n",
           r == 0 ? "REJECTED" : "found no screen frames", (SDL_GetTicks() - c0) / 1000.0);
  }
  size_t o = strlen(j->motivos);
  snprintf(j->motivos + o, sizeof j->motivos - o, " | check:%s", r == 1 ? "ACCEPTED" : r == 0 ? "rejected" : "no-screen");
  pthread_mutex_unlock(&mu);
  fflush(stdout);
  if (r != -1 || !parou(j)) fonteAnotar(j->dFonte, r == 1);   // a check cut short by the title ending decides nothing
  free(c);
  return NULL;
}
// Everything the direct engine made, gone (a copy the check rejected): the playing file starts from nothing.
static void limparDireto(Job *j) {
  pthread_mutex_lock(&mu);
  for (int k = 0; k < j->n; k++) {
    free(j->tiles[k].jpg); j->tiles[k].jpg = NULL; j->tiles[k].n = 0; j->tiles[k].pts = 0;
    j->tiles[k].prov = 0; j->tiles[k].salvo = 0; j->tiles[k].revision++;
    j->tentado[k] = 0; j->slotKey[k] = -1; j->framePts[k] = -1;
  }
  j->feitos = 0; j->primeiro = 0; j->cob60 = j->cob30 = 0; j->appendPronto = 0;
  mini_coverage_init(&j->coverage, j->n, j->dur, j->slotFrame, j->slotState);
  pthread_mutex_unlock(&mu);
  if (j->arq[0]) remove(j->arq);
}

// ---- copy sources that keep failing (06/11/2026) ----
// PenguPlay's smaller copies were dead (410/502), not seekable (HTTP 200 to a range) or another cut on every title
// tried, costing ~30 s before the playing file's own pass started. Per source (the add-on), the copies tried since
// the last accepted one are kept for FONTE_DIAS. On a FAST device (CPU route measured <= RAPIDO_MS per 4K thumbnail:
// the playing file is quick anyway) a source at FONTE_TENTATIVAS is skipped. On a slow one every copy is still tried:
// a copy saves it ten minutes or more.
#define FONTE_TENTATIVAS 6
#define FONTE_DIAS 3
#define RAPIDO_MS 800
#define FONTE_MAX 64
static pthread_mutex_t muFontes = PTHREAD_MUTEX_INITIALIZER;
typedef struct { char chave[FONTEFALHA_CHAVE]; int tentativas; long quando; } FonteCopia;
static int fontesLer(FonteCopia *f, char *caminho, size_t n) {
  char dir[512];
  caminho[0] = 0;
  if (!dados_caminho(dir, sizeof dir, "miniaturas")) return 0;
  snprintf(caminho, n, "%s/copias-fontes.txt", dir);
  FILE *a = fopen(caminho, "r");
  if (!a) return 0;
  int k = 0;
  long agora = (long)time(NULL);
  char linha[160];
  while (k < FONTE_MAX && fgets(linha, sizeof linha, a)) {
    char *t1 = strchr(linha, '\t'), *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    if (!t2) continue;
    *t1 = 0;
    snprintf(f[k].chave, sizeof f[k].chave, "%s", linha);
    f[k].tentativas = atoi(t1 + 1); f[k].quando = atol(t2 + 1);
    if (agora - f[k].quando <= FONTE_DIAS * 86400L) k++;    // older than FONTE_DIAS: forgotten, tried again
  }
  fclose(a);
  return k;
}
static int fonteTentativas(const char *chave) {
  FonteCopia f[FONTE_MAX]; char c[600];
  pthread_mutex_lock(&muFontes);
  int n = fontesLer(f, c, sizeof c), r = 0;
  for (int k = 0; k < n; k++) if (!strcmp(f[k].chave, chave)) r = f[k].tentativas;
  pthread_mutex_unlock(&muFontes);
  return r;
}
static void fonteAnotar(const char *chave, int aceita) {
  if (!chave || !chave[0] || strpbrk(chave, "\t\n")) return;
  FonteCopia f[FONTE_MAX + 1]; char c[600];
  pthread_mutex_lock(&muFontes);
  int n = fontesLer(f, c, sizeof c), achou = -1;
  for (int k = 0; k < n; k++) if (!strcmp(f[k].chave, chave)) achou = k;
  if (achou < 0 && n < FONTE_MAX) { achou = n++; snprintf(f[achou].chave, sizeof f[achou].chave, "%s", chave); f[achou].tentativas = 0; }
  if (achou >= 0) { f[achou].tentativas = aceita ? 0 : f[achou].tentativas + 1; f[achou].quando = (long)time(NULL); }
  if (c[0]) {
    FILE *a = fopen(c, "w");
    if (a) { for (int k = 0; k < n; k++) if (f[k].tentativas) fprintf(a, "%s\t%d\t%ld\n", f[k].chave, f[k].tentativas, f[k].quando); fclose(a); }
  }
  pthread_mutex_unlock(&muFontes);
}
static int aparelhoRapido(void) {
  PerfilCpu pf;
  perfilLer(&pf);
  return pf.n[1] >= CPU_AMOSTRAS && pf.ms[1] <= RAPIDO_MS;
}

static int escolherCopiaRapida(Job *j) {
  int rapido = aparelhoRapido();
  printf("[miniatura] copy-first: playing %dp %.1f min, %d smaller cop%s (%s); %s device\n", j->alturaA, j->dur / 60000.0, j->nCopias,
         j->nCopias == 1 ? "y" : "ies", j->motivos, rapido ? "fast (failing sources skipped)" : "slow (every copy tried)");
  fflush(stdout);
  esperarPlayer(j);
  // What EARLIER titles taught, read once: this title's own attempts only count from the next one (06/11/2026: read
  // per copy, six dead 720p copies made the same title skip its six 1080p backups untried). A test run (the cold
  // marker) tries everything.
  int anteriores[TENTATIVAS];
  for (int c = 0; c < j->nCopias; c++) anteriores[c] = j->auditarRede ? 0 : fonteTentativas(j->copias[c].fonte);
  for (int c = 0; c < j->nCopias && !parou(j); c++) {
    size_t o = strlen(j->motivos);
    int falhas = anteriores[c];
    if (rapido && falhas >= FONTE_TENTATIVAS) {
      printf("[miniatura] copy-first: copy %d (%dp) skipped: %d copies from its source tried in the last %d days, none usable\n",
             c + 1, j->copias[c].altura, falhas, FONTE_DIAS);
      fflush(stdout);
      snprintf(j->motivos + o, sizeof j->motivos - o, " | c%d:source-skipped", c + 1);
      continue;
    }
    Uint32 c0 = SDL_GetTicks();
    long long hB = mAbrir(j->copias[c].url, j->copias[c].cab, 1);
    if (!hB) {
      printf("[miniatura] copy-first: copy %d (%dp) would not open\n", c + 1, j->copias[c].altura); fflush(stdout);
      snprintf(j->motivos + o, sizeof j->motivos - o, " | c%d:no-open", c + 1);
      fonteAnotar(j->copias[c].fonte, 0);
      continue;
    }
    long long ib[6];
    char alvo[4096];
    mInfo(hB, ib);
    mAlvo(hB, j->copias[c].url, alvo, sizeof alvo);
    printf("[miniatura] copy-first: checking copy %d: %lldx%lld %.1f min, %.0f MB\n", c + 1, ib[1], ib[2], ib[0] / 60000.0,
           j->copias[c].tamanho / 1048576.0);
    fflush(stdout);
    const char *por = NULL;
    Mapa mapa = {1.0, 0.0, 0.0, 0};
    if (!ib[5]) por = "codec";
    else if (ib[0] <= 0) por = "no-duration";
    else if (labs((long)ib[0] - j->dur) > TELA_DUR_IGUAL_MS) por = "runtime";
    else if (labs((long)ib[0] - j->dur) <= OTIMISTA_DUR_MS) {
      // (nearly) the same length: start NOW at offset 0, the check runs alongside (06/11/2026: the blocking check left
      // the bar empty for the first 46 s on TorBox Dune)
      CheckCopia *cc = malloc(sizeof *cc);
      if (cc) {
        cc->j = j; cc->hB = hB; cc->durB = (long)ib[0];
        j->dCheck = 1;
        j->dCheckRodando = pthread_create(&j->dCheckT, NULL, checarCopia, cc) == 0;
        if (!j->dCheckRodando) { free(cc); j->dCheck = 0; }
      }
      if (j->dCheckRodando) {
        snprintf(j->dFonte, sizeof j->dFonte, "%s", j->copias[c].fonte);   // the check thread notes the verdict
        j->dCopia = 1; j->dA = 1.0; j->dB = 0.0; j->dAltura = j->copias[c].altura;
        snprintf(j->dUrl, sizeof j->dUrl, "%s", alvo[0] ? alvo : j->copias[c].url);
        snprintf(j->dCab, sizeof j->dCab, "%s", j->copias[c].cab);
        snprintf(j->escolhida, sizeof j->escolhida, "%dp %.0f MB %s (copy-first, optimistic)", j->copias[c].altura,
                 j->copias[c].tamanho / 1048576.0, j->copias[c].nome);
        printf("[miniatura] copy-first: copy %d same length (%+.1f s): thumbnails start now, screen check alongside (%.1f s in)\n",
               c + 1, (ib[0] - j->dur) / 1000.0, (SDL_GetTicks() - c0) / 1000.0);
        fflush(stdout);
        return 1;
      }
      int tela = conferirTela(j, hB, (long)ib[0], &mapa);
      if (tela != 1) por = tela == 0 ? "screen-rejected" : "no-screen-frames";
    } else {
      int tela = conferirTela(j, hB, (long)ib[0], &mapa);
      if (tela != 1) por = tela == 0 ? "screen-rejected" : "no-screen-frames";
    }
    mFechar(hB);
    printf("[miniatura] copy-first: copy %d %s in %.1f s\n", c + 1, por ? por : "ACCEPTED", (SDL_GetTicks() - c0) / 1000.0);
    fflush(stdout);
    snprintf(j->motivos + o, sizeof j->motivos - o, " | c%d:%s", c + 1, por ? por : "ACCEPTED");
    fonteAnotar(j->copias[c].fonte, !por);
    if (por) continue;
    j->dCopia = 1; j->dA = mapa.a; j->dB = mapa.b; j->dAltura = j->copias[c].altura;
    snprintf(j->dUrl, sizeof j->dUrl, "%s", alvo[0] ? alvo : j->copias[c].url);
    snprintf(j->dCab, sizeof j->dCab, "%s", j->copias[c].cab);
    j->offset = mapa.b;
    snprintf(j->escolhida, sizeof j->escolhida, "%dp %.0f MB %s (copy-first, screen %+.2fs)", j->copias[c].altura,
             j->copias[c].tamanho / 1048576.0, j->copias[c].nome, mapa.b / 1000);
    return 1;
  }
  printf("[miniatura] copy-first: no copy passed: direct mode on the playing file\n"); fflush(stdout);
  return 0;
}

static void *trabalhar(void *u) {
  Job *j = u;
  long long hA = 0, hB = 0, info[6];
  Mapa mapa = {1.0, 0.0, 0.0, 0};
  int usaB = 0, copiaUsada = 0;
  Uint32 t0 = SDL_GetTicks();
  falhasSeguidas = 0; decodSeguidas = 0;
  mContadores(j->cont0);
  mDiag(1, NULL, 0);
  int bits = bitsDebug();
  mConfigurar(bits);
  JNIEnv *timelineEnv = (JNIEnv *)SDL_AndroidGetJNIEnv();
  (*timelineEnv)->CallStaticVoidMethod(timelineEnv, cls, jPipelineStart, (jlong)(SDL_GetTicks() - t0));
  if ((*timelineEnv)->ExceptionCheck(timelineEnv)) (*timelineEnv)->ExceptionClear(timelineEnv);
  j->t0 = t0;
  evRebuffer = evErro = 0;
  j->perdidos0 = mPerdidos();                   // (before any branch: the forced-direct one returned before these)
  j->audio0 = mAudioFalhas();
  j->memMin = memDisponivelKB();
  j->heapMin = 100000;
  // DIRECT MODE FIRST, at any resolution (bit 8 of debug.nuvio.miniatura turns it off). Decided 23/10 with the user
  // after the Shield runs: no copy check (30 s-2 min, wrong versions), frames of the very file playing, no audio
  // underruns (decoder only from the first pause/scrub), ~4-6 thumbnails/s on 4K; the copy route created hardware
  // decoders for HEVC copies (HDMI underruns, ~1.5/s). The copies remain the fallback when the playing file cannot be
  // used directly (Dolby Vision profile 5, a codec other than H.264/HEVC). The 22/10 failure next to a 1080p remux
  // (player decoder 4003) came from a burst of short-lived decoders; direct mode keeps ONE, created at a pause/scrub.
  int diretoOk = !(bits & 8), diretoTentado = 0;
  // COPY-FIRST (06/11/2026, after reading Nuvio Arc): a 4K title's thumbnails from a smaller copy in the source list
  // once the screen check confirms it, made by the SAME fast direct engine (lanes, multi-range, look-ahead, map) at
  // copy time = a x play time + b. A 1080p keyframe is ~5x fewer bytes and decodes on the CPU in ~1/4 of the time.
  // No copy, or none passing: direct mode on the playing file as before. Bit 1048576 turns copy-first off.
  if (j->alturaA > 1080 && (j->nCopias == 0 || (bits & 1048576))) {
    printf("[miniatura] copy-first: not used (%s; %s)\n", (bits & 1048576) ? "off by debug bit" : "no smaller copy", j->motivos);
    fflush(stdout);
  }
  if (diretoOk && !(bits & 1048576) && j->alturaA > 1080 && j->nCopias > 0 && !parou(j)) {
    pthread_mutex_lock(&mu); j->direto = 1; pthread_mutex_unlock(&mu);
    snprintf(j->arq, sizeof j->arq, "%s", j->arqDireto);
    if (carregarInformado(j)) { avisoPronto = j->g; goto fim; }   // a complete saved set: no network at all
    pthread_mutex_lock(&mu); j->direto = 0; pthread_mutex_unlock(&mu);
    // The 4K pre-warm (file open + hardware decoder made during the opening load) is kept until a copy is CHOSEN:
    // when every copy fails, direct mode adopts it as before. 06/11/2026 (2.0.21): closed up front, an R2 title whose
    // four PenguPlay copies were dead or ignored Range fell back to the CPU route, 458 s instead of ~190 s.
    if (escolherCopiaRapida(j)) {
      { int d = 0; long long pre = adotarPre(j, &d); if (pre) mFechar(pre); }   // a copy is used: the 4K session goes
      diretoTentado = 1;
      int r = trabalharDireto(j, &hA);
      if (r && j->dCheck == 3 && !parou(j)) {      // optimistic copy rejected: its tiles go, the playing file starts
        if (hA) { mFechar(hA); hA = 0; }
        limparDireto(j);
        j->dCopia = 0; j->dCheck = 0;
        pthread_mutex_lock(&mu); j->direto = 0; pthread_mutex_unlock(&mu);
        printf("[miniatura] copy-first: rejected copy discarded: direct mode on the playing file\n"); fflush(stdout);
        trabalharDireto(j, &hA);
        goto fim;
      }
      if (r) goto fim;
      printf("[miniatura] copy-first: the copy would not open in the direct engine: direct mode on the playing file\n");
      fflush(stdout);
      if (hA) { mFechar(hA); hA = 0; }
      j->dCopia = 0;
      pthread_mutex_lock(&mu); j->direto = 0; pthread_mutex_unlock(&mu);
      diretoTentado = 0;
    }
  }
  if (diretoOk && !diretoTentado) {
    diretoTentado = 1;                           // no esperarPlayer: the decoder goes in during the opening load
    if (trabalharDireto(j, &hA)) goto fim;
    if (hA) { mFechar(hA); hA = 0; }
  }
  evRebuffer = evErro = 0;
  if (j->arq[0]) limparCache(j->arq);           // other titles' sets older than 2 h go now
  if (carregarInformado(j)) { avisoPronto = j->g; goto fim; }                    // a complete saved set: no network at all
  if (j->nCopias == 0 && diretoOk && !diretoTentado) {
    printf("[miniatura] no smaller copy in the list: direct mode\n"); fflush(stdout);
    esperarPlayer(j);
    trabalharDireto(j, &hA);
    goto fim;
  }
  if (j->nCopias == 0) {                        // nothing smaller to make them from: the playing file is not opened
    printf("[miniatura] playing source unavailable for thumbnails; no compatible fallback\n"); fflush(stdout);
    pthread_mutex_lock(&mu); if (job == j) { estado = M_FALHOU; motivoSem = 1; } pthread_mutex_unlock(&mu);
    goto fim;
  }
  esperarPlayer(j);
  memset(info, 0, sizeof info);
  // The playing file is NOT opened here: the screen check needs only what the player shows. It is opened only for
  // the 4K-anchor check (a copy whose runtime differs, or no screen frames) or to make thumbnails from a <= 1080p file.
  printf("[miniatura] playing %dp %.1f min; %d smaller cop%s in the list\n", j->alturaA, j->dur / 60000.0, j->nCopias,
         j->nCopias == 1 ? "y" : "ies");
  fflush(stdout);
  Faixa f1, f2;
  memset(&f1, 0, sizeof f1); memset(&f2, 0, sizeof f2);
  pthread_t t1, t2;
  int roda1 = 0, roda2 = 0;
  char alvoB[4096];
  for (int c = 0; c < j->nCopias && !usaB && !parou(j); c++) {
    hB = mAbrir(j->copias[c].url, j->copias[c].cab, 1);
    if (!hB) {
      printf("[miniatura] copy %d (%dp) would not open\n", c + 1, j->copias[c].altura);
      snprintf(j->motivos + strlen(j->motivos), sizeof j->motivos - strlen(j->motivos), " | c%d:no-open", c + 1);
      continue;
    }
    long long ib[6];
    mInfo(hB, ib);
    mAlvo(hB, j->copias[c].url, alvoB, sizeof alvoB);
    printf("[miniatura] checking copy %d: %lldx%lld %.1f min\n", c + 1, ib[1], ib[2], ib[0] / 60000.0); fflush(stdout);
    if (!ib[5]) {                       // not H.264/HEVC: rejected from the header, before any decoder exists
      printf("[miniatura] copy %d: codec not usable for thumbnails (only H.264/HEVC)\n", c + 1); fflush(stdout);
      snprintf(j->motivos + strlen(j->motivos), sizeof j->motivos - strlen(j->motivos), " | c%d:codec", c + 1);
      mFechar(hB); hB = 0;
      continue;
    }
    int igual = ib[0] > 0 && labs((long)ib[0] - j->dur) <= TELA_DUR_IGUAL_MS;
    if (igual && !roda1 && labs((long)ib[0] - j->dur) <= OTIMISTA_DUR_MS) {
      // OPTIMISTIC START: same length within 2.5 s, so one lane starts at offset 0 while the screen check runs; the
      // check then relabels (offset <= 3 s) or discards (larger, or rejected) what it made.
      pthread_mutex_lock(&mu);
      j->mapa.a = 1.0; j->mapa.b = 0.0; j->otimista = 1;
      pthread_mutex_unlock(&mu);
      snprintf(origem, sizeof origem, "%dp copy", j->copias[c].altura);
      f1.j = j; f1.usaB = 1; f1.id = 1; f1.feitos = 0; f1.somaMs = 0; f1.lido = 0;
      f1.h = mAbrir(alvoB, j->copias[c].cab, 1);
      if (f1.h) roda1 = pthread_create(&t1, NULL, faixa, &f1) == 0;
      if (!roda1) { mFechar(f1.h); f1.h = 0; }
      else { printf("[miniatura] optimistic start: thumbnails from copy %d at offset 0 while the check runs\n", c + 1); fflush(stdout); }
    }
    Uint32 c0 = SDL_GetTicks();
    long long lA0 = mLidos(hA);
    int aceita = 0, tela = -1;
    const char *metodo = "none";
    if (igual) { tela = conferirTela(j, hB, (long)ib[0], &mapa); metodo = "screen"; }
    if (tela == 1) aceita = 1;
    else if (tela == -1 && ib[0] > 0 && j->alturaA > 1080 && !parou(j)) {   // never decode a <= 1080p playing file
      // ROUTER (MAX_RAZAO_ERRO, 10 %): runtime differs (maybe another cut: anchors across the whole film), or the
      // screen gave no frames; beyond 10 % the 4K-anchor check refuses at once
      printf("[miniatura] %s: checking with the playing file's own frames\n", igual ? "no screen frames" : "runtime differs");
      fflush(stdout);
      metodo = "4k-anchors";
      if (!hA) { hA = mAbrir(j->urlA, j->cabA, 0); if (hA) { mInfo(hA, info); lA0 = mLidos(hA); } }
      if (hA) aceita = conferir(j, hA, hB, j->dur, (long)ib[0], j->anc, &mapa, info[3] != 0);
    }
    long long lidosB = mLidos(hB);
    size_t o = strlen(j->motivos);
    if (aceita) {
      usaB = 1;
      copiaUsada = c;
      snprintf(origem, sizeof origem, "%dp copy", j->copias[c].altura);
      pthread_mutex_lock(&mu);
      if (j->otimista) {
        if (mapa.a == 1.0 && fabs(mapa.b) <= OFFSET_MANTER_MS) {
          int n = 0;
          for (int k = 0; k < j->n; k++) if (j->tiles[k].jpg && j->tiles[k].prov) { j->tiles[k].pts -= (long)mapa.b; j->tiles[k].prov = 0; n++; }
          printf("[miniatura] optimistic thumbnails kept: %d relabelled by %+.2fs\n", n, -mapa.b / 1000);
        } else {
          for (int k = 0; k < j->n; k++)
            if (!j->tiles[k].jpg || j->tiles[k].prov) {
              if (j->tiles[k].jpg) { free(j->tiles[k].jpg); j->tiles[k].jpg = NULL; j->feitos--; }
              j->tentado[k] = 0;
            }
          j->refeito = 1;
          printf("[miniatura] offset %+.2fs is over %.0fs: optimistic thumbnails redone\n", mapa.b / 1000, OFFSET_MANTER_MS / 1000);
        }
        j->otimista = 0;
      }
      j->mapa = mapa;
      j->offset = mapa.b;
      pthread_mutex_unlock(&mu);
      fflush(stdout);
      snprintf(j->escolhida, sizeof j->escolhida, "%dp %.0f MB %s (via %s)", j->copias[c].altura, j->copias[c].tamanho / 1048576.0,
               j->copias[c].nome, metodo);
      snprintf(j->motivos + o, sizeof j->motivos - o, " | c%d:ACCEPTED", c + 1);
    } else {
      if (roda1) {                       // the optimistic lane was making a wrong copy's thumbnails: stop, discard
        j->pararFaixas = 1;
        pthread_join(t1, NULL);
        roda1 = 0;
        j->pararFaixas = 0;
        mFechar(f1.h); f1.h = 0;
        pthread_mutex_lock(&mu);
        for (int k = 0; k < j->n; k++)
          if (!j->tiles[k].jpg || j->tiles[k].prov) {
            if (j->tiles[k].jpg) { free(j->tiles[k].jpg); j->tiles[k].jpg = NULL; j->feitos--; }
            j->tentado[k] = 0;
          }
        j->otimista = 0;
        pthread_mutex_unlock(&mu);
      }
      mFechar(hB); hB = 0;
      snprintf(j->motivos + o, sizeof j->motivos - o, " | c%d:%s", c + 1,
               ib[0] <= 0 ? "no-duration" : !igual && j->alturaA <= 1080 ? "runtime" : tela == 0 ? "screen-rejected" : "rejected");
    }
    printf("[miniatura] check took %.1f s, read play %.1f MB + copy %.1f MB\n", (SDL_GetTicks() - c0) / 1000.0,
           (mLidos(hA) - lA0) / 1048576.0, lidosB / 1048576.0);
    fflush(stdout);
  }
  // Never thumbnails from the PLAYING file, at any resolution: decoding the very video the player decodes is what
  // preceded the player's own decoder failing (4003) and the source switches (22/10, a 1080p remux episode).
  if (!usaB && diretoOk && !diretoTentado && !parou(j)) {
    printf("[miniatura] no copy passed the check: direct mode\n"); fflush(stdout);
    if (hA) { mFechar(hA); hA = 0; }             // the check's session (no batches, CPU decoder): a fresh direct one
    trabalharDireto(j, &hA);
    goto fim;
  }
  if (!usaB) {
    printf("[miniatura] no copy passed the check: stopping (thumbnails are never made from the playing file)\n");
    fflush(stdout);
    pthread_mutex_lock(&mu); if (job == j && !j->feitos) { estado = M_FALHOU; motivoSem = 1; } pthread_mutex_unlock(&mu);
    goto fim;
  }
  {
    // Two lanes on the copy, each with its own reader and decoder (both on the copy's resolved link: no new debrid
    // link minted), sharing ONE request in flight (MiniaturaMotor): one decodes while the other's batch is on the wire.
    mAlvo(hB, j->copias[copiaUsada].url, alvoB, sizeof alvoB);
    // hB stays open for the re-check while watching (below)
    if (!roda1 && !parou(j)) {
      f1.j = j; f1.usaB = 1; f1.id = 1; f1.feitos = 0; f1.somaMs = 0; f1.lido = 0;
      f1.h = mAbrir(alvoB, j->copias[copiaUsada].cab, 1);
      if (f1.h) roda1 = pthread_create(&t1, NULL, faixa, &f1) == 0;
      if (!roda1) { mFechar(f1.h); f1.h = 0; }
    }
    if (!parou(j)) {
      f2.j = j; f2.usaB = 1; f2.id = 2;
      f2.h = mAbrir(alvoB, j->copias[copiaUsada].cab, 1);
      if (f2.h) roda2 = pthread_create(&t2, NULL, faixa, &f2) == 0;
      if (!roda2) { mFechar(f2.h); f2.h = 0; }
    }
    // RE-CHECK WHILE WATCHING: the screen check only saw the first minute. Every 3 min of playback one frame on screen
    // is compared with the copy at the mapped time (a single exact frame, no 4K read). Two misses in a row (the second
    // 20 s after the first) mean the copy stopped matching (a scene cut or added in a same-length copy): the lanes
    // stop and every thumbnail after the last good check is dropped.
    int juntou = 0, falhasR = 0;
    long bomAte = j->ultimoBom;        // was 0: a drop then removed EVERY thumbnail (22/10, Bond: 931 of 931)
    Uint32 proxR = SDL_GetTicks() + RECHECK_MS;
    while (!j->parar) {
      if (!juntou && (!roda1 || f1.acabou) && (!roda2 || f2.acabou)) {
        if (roda1) pthread_join(t1, NULL);
        if (roda2) pthread_join(t2, NULL);
        juntou = 1;
        fecharLote(j, t0, &f1, &f2, roda1 + roda2);
      }
      if (!scrubbing && !bufferando && SDL_GetTicks() >= proxR) {
        long pA = 0;
        int r = reconferir(j, hB, mapaAtual(j), &pA);
        if (r == 1) { falhasR = 0; bomAte = pA; proxR = SDL_GetTicks() + RECHECK_MS; j->nRecheck++; }
        else if (r == 0) {
          j->nRecheck++; j->falhasRecheck++;
          printf("[miniatura] re-check at %lds: copy did not match (%d in a row)\n", pA / 1000, falhasR + 1); fflush(stdout);
          if (++falhasR >= 2) {
            // The lanes stop (nothing may be made with the old offset meanwhile), then: a NEW offset found -> every
            // thumbnail after the last confirmed match is redone with it (a piecewise map, segment by segment as
            // re-checks find them); none -> those thumbnails are dropped and the title stops.
            j->pararFaixas = 1;
            if (!juntou) {
              if (roda1) pthread_join(t1, NULL);
              if (roda2) pthread_join(t2, NULL);
              juntou = 1;
            }
            j->pararFaixas = 0;
            double nb = 0;
            int novo = reancorar(j, hB, mapaAtual(j), &nb);
            pthread_mutex_lock(&mu);
            int tirados = 0;
            double antigo = j->mapa.b;
            for (int k = 0; k < j->n; k++)
              if ((long)k * GRADE_MS > bomAte) {
                if (j->tiles[k].jpg) { free(j->tiles[k].jpg); j->tiles[k].jpg = NULL; j->feitos--; tirados++; }
                j->tentado[k] = 0;
              }
            if (novo) { j->mapa.b = nb; j->offset = nb; j->reancoras++; }
            pthread_mutex_unlock(&mu);
            if (novo && !parou(j)) {
              printf("[miniatura] re-anchored after %lds: offset %+.2fs -> %+.2fs, %d thumbnails after it redone\n", bomAte / 1000,
                     antigo / 1000, nb / 1000, tirados);
              fflush(stdout);
              falhasR = 0;
              bomAte = pA;
              f1.acabou = f2.acabou = 0;
              roda1 = f1.h && pthread_create(&t1, NULL, faixa, &f1) == 0;
              roda2 = f2.h && pthread_create(&t2, NULL, faixa, &f2) == 0;
              juntou = 0;
              j->resumido = 0;           // a new SUMMARY when the redo finishes
              proxR = SDL_GetTicks() + RECHECK_MS;
              continue;
            }
            j->caiuEm = pA;
            printf("[miniatura] the copy stopped matching after %lds and no new offset was found: %d thumbnails after it dropped\n",
                   bomAte / 1000, tirados);
            fflush(stdout);
            salvar(j, 0);
            break;
          }
          proxR = SDL_GetTicks() + 20000;
        } else proxR = SDL_GetTicks() + 10000;   // nothing on screen yet (paused): try again soon
      }
      dormir(j, 1000);
    }
    if (!juntou) {                       // the title was closed while the lanes ran
      if (roda1) pthread_join(t1, NULL);
      if (roda2) pthread_join(t2, NULL);
      fecharLote(j, t0, &f1, &f2, roda1 + roda2);
    }
    mFechar(f1.h); mFechar(f2.h);
  }
fim:
  if (j->dCheckRodando) { pthread_join(j->dCheckT, NULL); j->dCheckRodando = 0; }   // never outlives the job
  (*timelineEnv)->CallStaticVoidMethod(timelineEnv, cls, jPipelineStop);
  if ((*timelineEnv)->ExceptionCheck(timelineEnv)) (*timelineEnv)->ExceptionClear(timelineEnv);
  mFechar(hB);
  mFechar(hA);
  resumo(j, t0);
  pthread_mutex_lock(&mu);
  j->vivo = 0;
  int orfao = job != j;
  pthread_mutex_unlock(&mu);
  if (orfao) {                                  // switched off meanwhile: the worker frees its own job
    for (int k = 0; k < j->n; k++) free(j->tiles[k].jpg);
    free(j->tiles); free(j->tentado); free(j->slotFrame); free(j->slotState); free(j->framePts); free(j->slotKey); free(j);
  }
  return NULL;
}

// ---- display (main/GL thread) ----
// Keep a bounded cache of generated RGBA pictures: nearby previews need no JPEG decode.
// Compression remains in the generation path for persistence; no extra Java heap pixel copy.
static void prepararPixelsDireto(Job *j, long long h, int k, long pts) {
  if (k < 0 || k >= j->n || labs(pts - (scrubbing ? posMs : playheadMs)) > 60000L) return;
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  jobject buffer = (*env)->CallStaticObjectMethod(env, cls, jPixelsDireto, (jlong)h);
  jint dims = (*env)->CallStaticIntMethod(env, cls, jPixelsDimensoes, (jlong)h);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); if (buffer) (*env)->DeleteLocalRef(env, buffer); return; }
  int w = (unsigned)dims >> 16, hgt = dims & 65535;
  unsigned char *rgba = buffer ? (*env)->GetDirectBufferAddress(env, buffer) : NULL;
  jlong size = buffer ? (*env)->GetDirectBufferCapacity(env, buffer) : 0;
  SDL_Surface *surface = NULL;
  if (rgba && w > 0 && w <= 640 && hgt > 0 && hgt <= 360 && size >= (jlong)w * hgt * 4) {
    surface = SDL_CreateRGBSurfaceWithFormat(0, w, hgt, 32, SDL_PIXELFORMAT_ABGR8888);
    if (surface) for (int y = 0; y < hgt; y++) memcpy((unsigned char *)surface->pixels + y * surface->pitch, rgba + y * w * 4, (size_t)w * 4);
  }
  if (buffer) (*env)->DeleteLocalRef(env, buffer);
  if (!surface) return;
  pthread_mutex_lock(&mu);
  if (job == j && !j->parar && j->tiles[k].jpg && j->tiles[k].pts == pts) {
    Tile *t = &j->tiles[k];
    MiniPixKey key = { j->g, k, t->n, t->pts, t->revision };
    pthread_mutex_lock(&pixMu);
    pixStore(key, surface);
    pthread_mutex_unlock(&pixMu);
    surface = NULL;
  }
  pthread_mutex_unlock(&mu);
  if (surface) SDL_FreeSurface(surface);
}
static struct { GLuint tex; MiniPixKey key; unsigned uso; } texs[NTEX];
static unsigned usoTex;
static int velho;

static void texLimpar(void) {
  pixReset();
  for (int i = 0; i < NTEX; i++) if (texs[i].tex) {
    gfx_tex_esquecer(texs[i].tex);
    glDeleteTextures(1, &texs[i].tex); texs[i].tex = 0;
  }
}
static GLuint texDe(Job *j, int k) {
  Tile *tile = &j->tiles[k];
  MiniPixKey key = { j->g, k, tile->n, tile->pts, tile->revision };
  int livreI = 0;
  for (int i = 0; i < NTEX; i++) {
    if (texs[i].tex && pixEqual(texs[i].key, key)) { texs[i].uso = ++usoTex; return texs[i].tex; }
    if (!texs[i].tex || texs[i].uso < texs[livreI].uso) livreI = i;
  }
  if (!j->tiles[k].jpg) return 0;
  SDL_Surface *c = pixTake(key);
  if (!c) { pixRequest(key, tile->jpg); return 0; }
  if (texs[livreI].tex) {
    gfx_tex_esquecer(texs[livreI].tex);
    glDeleteTextures(1, &texs[livreI].tex);
  }
  GLuint t;
  glGenTextures(1, &t);
  glBindTexture(GL_TEXTURE_2D, t);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, c->w, c->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, c->pixels);
  gfx_tex_esquecer(0);  // raw GL binding changed behind the renderer's cache
  SDL_FreeSurface(c);
  texs[livreI].tex = t; texs[livreI].key = key; texs[livreI].uso = ++usoTex;
  return t;
}

// What the seek bar may show. A copy's OPTIMISTIC tile (made at offset 0 while the screen check runs) stays hidden
// until the check accepts the copy: a wrong film or cut must never reach the screen, even for a few seconds
// (06/11/2026). Acceptance clears .prov (relabelled) or the tile is discarded.
#define TILE_VISIVEL(j, s) ((j)->tiles[s].jpg && !(j)->tiles[s].prov)
static int pQuadros(double posSeg, int n, GLuint *out, double *cueSeg) {
  int achou = 0;
  for (int i = 0; i < n; i++) { out[i] = 0; if (cueSeg) cueSeg[i] = -1.0; }
  pthread_mutex_lock(&mu);
  Job *j = job;
  velho = 0;
  if (j && j->n > 0) {
    int k = (int)((posSeg * 1000.0 + GRADE_MS / 2) / GRADE_MS);
    if (k >= j->n) k = j->n - 1;
    if (k < 0) k = 0;
    int atual = -1;
    if (j->direto) {
      int alias = j->slotFrame[k];
      if (alias >= 0 && alias < j->n && TILE_VISIVEL(j, alias) &&
          mini_coverage_valid(&j->coverage, k, j->tiles[alias].pts)) atual = alias;
      if (atual < 0) for (int s = 0; s < j->n; s++) {
        if (!TILE_VISIVEL(j, s) || labs(j->tiles[s].pts - k * GRADE_MS) > VIZINHO_SLOTS * GRADE_MS) continue;
        if (atual < 0 || labs(j->tiles[s].pts - k * GRADE_MS) < labs(j->tiles[atual].pts - k * GRADE_MS)) atual = s;
      }
      // The ring means "a closer thumbnail is still coming". Once the job has ended nothing more will come: the
      // nearest thumbnail stands in plainly (06/11/2026, a 1080p episode with a keyframe only every ~30 s: 45 of 138
      // slots, the rest showed a loading ring forever).
      velho = j->vivo && atual >= 0 && !mini_coverage_valid(&j->coverage, k, j->tiles[atual].pts);
    } else if (TILE_VISIVEL(j, k)) atual = k;
    else {
      for (int d = 1; d <= VIZINHO_SLOTS && atual < 0; d++) {
        if (k - d >= 0 && TILE_VISIVEL(j, k - d)) atual = k - d;
        else if (k + d < j->n && TILE_VISIVEL(j, k + d)) atual = k + d;
      }
      // a neighbour within 8 s IS this slot's thumbnail (no "still loading" ring); further away it stands in
      velho = j->vivo && atual >= 0 && labs(j->tiles[atual].pts - k * GRADE_MS) > TOL_COPIA_MS;
    }
    // Slots can be empty after cue deduplication or during the sparse pass.
    // The strip follows actual picture times, not adjacent grid positions.
    int alvos[3] = { -1, atual, -1 }, base = n == 1 ? 1 : 0;
    if (n > 1 && atual >= 0) {
      long pts = j->tiles[atual].pts;
      for (int s = 0; s < j->n; s++) {
        if (!TILE_VISIVEL(j, s) || labs(j->tiles[s].pts - pts) > VIZINHO_SLOTS * GRADE_MS) continue;
        if (j->tiles[s].pts < pts && (alvos[0] < 0 || j->tiles[s].pts > j->tiles[alvos[0]].pts)) alvos[0] = s;
        if (j->tiles[s].pts > pts && (alvos[2] < 0 || j->tiles[s].pts < j->tiles[alvos[2]].pts)) alvos[2] = s;
      }
    }
    for (int pass = 0; pass < n; pass++) {
      int i = n == 3 ? (pass == 0 ? 1 : pass == 1 ? 0 : 2) : pass;  // centre queues first
      int s = alvos[base + i];
      if (s < 0 || s >= j->n || !TILE_VISIVEL(j, s)) continue;
      out[i] = texDe(j, s);
      if (cueSeg && out[i]) cueSeg[i] = j->tiles[s].pts / 1000.0;
    }
    achou = out[n == 1 ? 0 : 1] != 0;
  }
  pthread_mutex_unlock(&mu);
  return achou;
}
static int pVelho(void) { return velho; }
static void pOcioso(void) { texLimpar(); }

static void encerrar(void) {
  pthread_mutex_lock(&mu);
  Job *j = job;
  job = NULL;
  estado = M_PARADO;
  urlAtual[0] = 0;
  geracao++;
  int vivo = j && j->vivo;
  if (j) j->parar = 1;
  pthread_mutex_lock(&muPre);
  if (j && !preRodando && !preH && !strcmp(preUrl, j->urlA)) preUrl[0] = 0;
  pthread_mutex_unlock(&muPre);
  pthread_mutex_unlock(&mu);
  texLimpar();
  if (j && !vivo) {
    for (int k = 0; k < j->n; k++) free(j->tiles[k].jpg);
    free(j->tiles); free(j->tentado); free(j->slotFrame); free(j->slotState); free(j->framePts); free(j->slotKey); free(j);
  }
}

static void pPedir(const SeekrFonte *f) {
  if (bitsDebug() & 1024) {
    pthread_mutex_lock(&mu); int running = job != NULL; pthread_mutex_unlock(&mu);
    if (running) encerrar();
    return;
  }
  posMs = f->posMs; playheadMs = f->playheadMs; scrubbing = f->scrubbing;
  if (avisoPronto && avisoPronto == geracao) {          // the playing title's set is complete
    avisoPronto = 0;
    pthread_mutex_lock(&mu); if (job && job->g == geracao) job->avisoCompleto = 1; pthread_mutex_unlock(&mu);
  }
  bufferando = video_bufferando_ms() > 0;
  { static int bAntes, eAntes;
    int e = video_falhou();
    if (bufferando && !bAntes && !mSeekRecente()) evRebuffer++;
    if (e && !eAntes) evErro++;
    bAntes = bufferando; eAntes = e; }
  if (!f->url || strncmp(f->url, "http", 4) || f->durMs < 60000) return;
  if (!strcmp(urlAtual, f->url)) return;                       // the same title: the worker is on it
  encerrar();
  motivoSem = 0;
  if (!jniPreparar()) { estado = M_FALHOU; snprintf(urlAtual, sizeof urlAtual, "%s", f->url); return; }
  Job *j = calloc(1, sizeof *j);
  if (!j) return;
  j->dur = f->durMs;
  j->folgaTopo = -1;
  const char *headroom = getenv("NUVIO_THUMBNAIL_SHARED_HEADROOM");
  j->sharedHeadroom = !headroom || strcmp(headroom, "0");
  j->headerBridge = (bitsDebug() & 524288) != 0;
  const char *coverageAudit = getenv("NUVIO_THUMBNAIL_COVERAGE_AUDIT");
  j->coverageAudit = coverageAudit && !strcmp(coverageAudit, "1");
  j->n = (int)(f->durMs / GRADE_MS) + 1;
  j->tiles = calloc((size_t)j->n, sizeof *j->tiles);
  j->tentado = calloc((size_t)j->n, 1);
  j->slotFrame = malloc((size_t)j->n * sizeof *j->slotFrame);
  j->slotState = calloc((size_t)j->n, 1);
  j->framePts = malloc((size_t)j->n * sizeof *j->framePts);
  j->slotKey = malloc((size_t)j->n * sizeof *j->slotKey);
  if (!j->tiles || !j->tentado || !j->slotFrame || !j->slotState || !j->framePts || !j->slotKey) {
    free(j->tiles); free(j->tentado); free(j->slotFrame); free(j->slotState); free(j->framePts); free(j->slotKey); free(j); return;
  }
  mini_coverage_init(&j->coverage, j->n, j->dur, j->slotFrame, j->slotState);
  for (int k = 0; k < j->n; k++) { j->framePts[k] = -1; j->slotKey[k] = -1; }
  snprintf(j->urlA, sizeof j->urlA, "%s", f->url);
  j->auditarRede = (bitsDebug() & 256) != 0;
  j->larguraSaida = ajustes_miniatura_largura();
  char cacheMode[64];
  snprintf(cacheMode, sizeof cacheMode, "thumb-w%d", j->larguraSaida);
  chaveArquivo(f, j->arq, sizeof j->arq, cacheMode);
  snprintf(cacheMode, sizeof cacheMode, "direct-gpu-kf2-cov1-dvcolour1-w%d", j->larguraSaida);
  chaveArquivo(f, j->arqDireto, sizeof j->arqDireto, cacheMode);
  snprintf(j->cabA, sizeof j->cabA, "%s", f->cabecalhos ? f->cabecalhos : "");
  const Stream *st = stream_fonte_reproducao(f->url, f->cabecalhos);
  j->alturaA = st ? st->altura : 0;
  j->nCopias = st ? escolherCopias(st, j->copias, f->durMs, j->motivos, sizeof j->motivos) : 0;
  if (f->imdb && strchr(f->imdb, ':')) snprintf(j->titulo, sizeof j->titulo, "%s", f->imdb);
  else snprintf(j->titulo, sizeof j->titulo, "%s%s%d:%d", f->imdb ? f->imdb : "", f->t ? " " : "", f->t, f->e);
  pthread_mutex_lock(&mu);
  j->g = ++geracao;
  j->vivo = 1;
  job = j;
  estado = M_BUSCANDO;
  snprintf(urlAtual, sizeof urlAtual, "%s", f->url);
  pthread_mutex_unlock(&mu);
  pthread_t t;
  pthread_attr_t at;
  pthread_attr_init(&at);
  pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);
  if (pthread_create(&t, &at, trabalhar, j)) { pthread_mutex_lock(&mu); j->vivo = 0; pthread_mutex_unlock(&mu); encerrar(); }
  pthread_attr_destroy(&at);
}

static int pTemChave(void) { return 1; }
static int pEstado(void) {
  int e;
  pthread_mutex_lock(&mu); e = estado; pthread_mutex_unlock(&mu);
  return e == M_PRONTO ? SEEKR_PRONTO : e == M_BUSCANDO ? SEEKR_BUSCANDO : e == M_FALHOU ? SEEKR_SEM_PREVIA : SEEKR_DESLIGADO;
}
static const char *pRotulo(int est) {
  switch (est) {
    case SEEKR_BUSCANDO: return "Fazendo miniaturas neste aparelho";
    case SEEKR_PRONTO: return "Miniaturas disponíveis";
    case SEEKR_SEM_PREVIA: return "Este título não tem miniaturas";
    default: return "Fazendo miniaturas neste aparelho";
  }
}
static void pUso(SeekrUso *u) { memset(u, 0, sizeof *u); u->persistente = 1; u->chavePropria = 1; }
static void pTentar(void) { encerrar(); }
static void pDesligar(void) { encerrar(); }

static const SeekrProvedor PROVEDOR = {
  pTemChave, pEstado, pRotulo, pUso, pTentar, pPedir, pDesligar, pQuadros, pVelho, pOcioso
};
const SeekrProvedor *miniatura_provedor(void) { return &PROVEDOR; }
int miniatura_status_agora(void) {
  pthread_mutex_lock(&mu);
  Job *j = job;
  int covered = j && j->direto ? mini_coverage_covered_count(&j->coverage) : j ? j->feitos : 0;
  // 50 % of the slots that CAN get a thumbnail: a slot with no keyframe near it never counts against it.
  int possible = j ? (j->direto ? j->n - mini_coverage_unavailable_count(&j->coverage) : j->n - 1) : 0;
  int half = j && j->n > 1 && possible > 0 && covered * 2 >= possible;
  int result = mini_progress(&progresso, j ? j->g : 0, j && j->cacheChecado, half,
                             j && j->avisoCompleto, j ? j->cacheTipo : 0, SDL_GetTicks());
  pthread_mutex_unlock(&mu);
  return result;
}
int miniatura_prontas_agora(void) { return miniatura_status_agora() == 3; }
#endif
