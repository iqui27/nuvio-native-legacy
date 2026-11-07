"""Drive production pre-warm, encerrar, pPedir, pTentar and trabalharDireto.

No network. Handles are synthetic. Cancellation uses per-worker gates, not a
surrogate pPedir or a racy clock.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
text = (root / "src/miniatura_android.c").read_text()


def cut(start, end):
    a = text.index(start)
    return text[a:text.index(end, a)]


prewarm = cut("static pthread_mutex_t muPre", "\n// 1 = direct mode ran")
direto = cut("static int trabalharDireto(Job *j, long long *hA) {", "\n// Copy-first: the first copy")
encerrar = cut("static void jobFree(Job *j) {", "\nstatic void pPedir")
tentar = cut("static void pTentar(void)", "\nstatic void pDesligar") + "\n"
desligar = cut("static void pDesligar(void)", "\nstatic const SeekrProvedor") + "\n"
pedir = cut("static void pPedir(const SeekrFonte *f) {", "\nstatic int pTemChave") + "\n"

prefix = r'''
#define _DEFAULT_SOURCE
#define MINI_PREWARM_TEST
#include <setjmp.h>
#include <signal.h>
#include <sys/mman.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "miniatura_coverage.h"
#define GRADE_MS 10000L
typedef unsigned Uint32;
typedef struct { unsigned char *jpg; } Tile;
typedef struct { int altura; } Stream;
typedef struct { char url[8], cab[8], nome[8]; int altura; unsigned long long tamanho; } Copia;
typedef struct {
  char urlA[4096], cabA[512], dUrl[4096], dCab[512], arq[640], arqDireto[640], titulo[48], motivos[320];
  int parar, pararFaixas, vivo, n, dCopia, avisoCompleto, auditarRede, larguraSaida;
  int sharedHeadroom, headerBridge, coverageAudit, alturaA, nCopias;
  unsigned preIdentityGen, g;
  long dur;
  double folgaTopo;
  Tile *tiles; unsigned char *tentado, *slotState;
  int *slotFrame; int64_t *framePts; long *slotKey;
  MiniCoverage coverage;
  Copia copias[8];
} Job;
typedef struct {
  const char *imdb; int t, e; long durMs, posMs, playheadMs; int scrubbing;
  const char *url, *cabecalhos;
} SeekrFonte;
enum { M_PARADO, M_FALHOU, M_BUSCANDO, M_PRONTO };
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static Job *job; static int estado; static char urlAtual[4096]; static char cabAtual[512];
static unsigned geracao, avisoPronto;
static volatile long posMs, playheadMs;
static volatile int scrubbing, bufferando, evRebuffer, evErro;
static int motivoSem;
static void texLimpar(void) {}
static int bitsDebug(void) { return 0; }
static int jni_ok = 1;
static int jniPreparar(void) { return jni_ok; }
static void mConfigurar(int b) { (void)b; }
static void tex_aliviar(void) {}
static pthread_mutex_t clockMu = PTHREAD_MUTEX_INITIALIZER;
static Uint32 fakeNowMs;
static Uint32 SDL_GetTicks(void) {
  pthread_mutex_lock(&clockMu); Uint32 t = fakeNowMs; pthread_mutex_unlock(&clockMu); return t;
}
static void fake_advance_ms(int ms) {
  pthread_mutex_lock(&clockMu); fakeNowMs += (Uint32)ms; pthread_mutex_unlock(&clockMu);
}
static int video_pronto(void) { return 0; }
static int video_tocando(void) { return 0; }
static int video_falhou(void) { return 0; }
static int video_bufferando_ms(void) { return 0; }
static int mSeekRecente(void) { return 0; }
static int parou(const Job *j) { return j && (j->parar || j->pararFaixas); }
static void dormir(const Job *j, int ms) {
  pthread_mutex_lock(&clockMu);
  Uint32 end = fakeNowMs + (Uint32)ms;
  while (fakeNowMs < end && !parou(j)) fakeNowMs++;
  pthread_mutex_unlock(&clockMu);
}
static int ajustes_miniatura_largura(void) { return 320; }
static void chaveArquivo(const SeekrFonte *f, char *out, size_t n, const char *modo) {
  (void)f; (void)modo; if (n) out[0] = 0;
}
static const Stream *stream_fonte_reproducao(const char *u, const char *c) { (void)u; (void)c; return NULL; }
static int escolherCopias(const Stream *play, Copia *out, long durMs, char *motivos, size_t nmot) {
  (void)play; (void)out; (void)durMs; (void)motivos; (void)nmot; return 0;
}
static void encerrar(void);
static void jobAposFim(Job *j);
static void *trabalhar(void *u);
static int fails;
static Job *uaf_job;
static int uaf_arm, uaf_stage;
static size_t uaf_len;
static pthread_mutex_t uafMu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t uafCv = PTHREAD_COND_INITIALIZER;
static sigjmp_buf uaf_jmp;
static void (*libc_free)(void *) = free;
static void test_free(void *p) {
  if (uaf_arm && p == uaf_job) {
    munmap(p, uaf_len);
    uaf_job = NULL;
    pthread_mutex_lock(&uafMu);
    uaf_stage = 3;
    pthread_cond_broadcast(&uafCv);
    pthread_mutex_unlock(&uafMu);
    return;
  }
  libc_free(p);
}
#define free test_free
typedef struct { pthread_mutex_t mu; pthread_cond_t cv; int entered, go; } Gate;
static Gate hold_direto;
static int hold_arm;
static void gate_init(Gate *s) {
  pthread_mutex_init(&s->mu, NULL); pthread_cond_init(&s->cv, NULL); s->entered = s->go = 0;
}
static void mini_hook(const char *name, void *p) {
  (void)p;
  if (hold_arm && !strcmp(name, "direto_before_bind")) {
    pthread_mutex_lock(&hold_direto.mu);
    int ticket = ++hold_direto.entered;
    pthread_cond_broadcast(&hold_direto.cv);
    struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); ts.tv_sec += 3;
    while (hold_direto.go < ticket)
      if (pthread_cond_timedwait(&hold_direto.cv, &hold_direto.mu, &ts)) {
        fprintf(stderr, "FAIL worker barrier timed out\n"); fails++; break;
      }
    pthread_mutex_unlock(&hold_direto.mu);
    return;
  }
  if (uaf_arm && !strcmp(name, "encerrar_after_unlock")) {
    pthread_mutex_lock(&uafMu);
    struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); ts.tv_sec += 3;
    while (uaf_stage < 1)
      if (pthread_cond_timedwait(&uafCv, &uafMu, &ts)) { fails++; break; }
    uaf_stage = 2;
    pthread_cond_broadcast(&uafCv);
    while (uaf_stage < 3)
      if (pthread_cond_timedwait(&uafCv, &uafMu, &ts)) { fails++; break; }
    pthread_mutex_unlock(&uafMu);
  }
}
#define MINI_OWNER_HOOK(name, p) mini_hook(name, (void *)(p))
#define MAXH 64
static pthread_mutex_t g = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
static int opens, started, finished, gateOpen, closes[MAXH];
static char hUrl[MAXH][128], hCab[MAXH][64];
static long long mAbrirDireto(const char *url, const char *cab) {
  pthread_mutex_lock(&g);
  started++; pthread_cond_broadcast(&cv);
  while (strstr(url, "slow") && !gateOpen) pthread_cond_wait(&cv, &g);
  long long h = ++opens;
  if (h < MAXH) {
    snprintf(hUrl[h], sizeof hUrl[h], "%s", url); snprintf(hCab[h], sizeof hCab[h], "%s", cab);
  }
  pthread_mutex_unlock(&g);
  return h;
}
static int mPrepararDecoder(long long h) { return h > 0; }
static void mFechar(long long h) {
  if (!h || h >= MAXH) return;
  pthread_mutex_lock(&g); if (closes[h]) { fprintf(stderr, "FAIL closed twice\n"); fails++; } closes[h]++; pthread_mutex_unlock(&g);
}
typedef struct { void *(*fn)(void *); void *arg; } Tramp;
static void *trampoline(void *u) {
  Tramp t = *(Tramp *)u; free(u);
  t.fn(t.arg);
  pthread_mutex_lock(&g); finished++; pthread_cond_broadcast(&cv); pthread_mutex_unlock(&g);
  return NULL;
}
static int fail_all_create;
static void *(*fail_fn)(void *);
static int (*real_create)(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *) = pthread_create;
static int tcreate(pthread_t *t, const pthread_attr_t *at, void *(*fn)(void *), void *arg) {
  if (fail_all_create || (fail_fn && fn == fail_fn)) return EAGAIN;
  Tramp *x = malloc(sizeof *x); if (!x) return EAGAIN;
  x->fn = fn; x->arg = arg;
  int r = real_create(t, at, trampoline, x);
  if (r) free(x);
  return r;
}
#define pthread_create tcreate
'''

main = r'''
static int fim_only;
static void *trabalhar(void *u) {
  Job *j = u;
  long long hA = 0;
  if (!fim_only) {
    trabalharDireto(j, &hA);
    if (hA) mFechar(hA);
  }
  preSoltar(j, j->urlA, j->cabA, 0, j->preIdentityGen);
  jobAposFim(j);
  return NULL;
}
static void *uaf_worker(void *u) {
  pthread_mutex_lock(&uafMu);
  uaf_stage = 1;
  pthread_cond_broadcast(&uafCv);
  struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); ts.tv_sec += 3;
  while (uaf_stage < 2)
    if (pthread_cond_timedwait(&uafCv, &uafMu, &ts)) break;
  pthread_mutex_unlock(&uafMu);
  jobAposFim(u);
  return NULL;
}
static void on_uaf(int sig) { (void)sig; siglongjmp(uaf_jmp, 1); }
static int snap(int *v) { pthread_mutex_lock(&g); int n = *v; pthread_mutex_unlock(&g); return n; }
static void waitFor(int *v, int n) {
  struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); ts.tv_sec += 3;
  pthread_mutex_lock(&g);
  while (*v < n) if (pthread_cond_timedwait(&cv, &g, &ts)) { fprintf(stderr, "FAIL expected thread event never came\n"); fails++; break; }
  pthread_mutex_unlock(&g);
}
static void waitPreReady(unsigned want) {
  struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); ts.tv_sec += 3;
  pthread_mutex_lock(&muPre);
  while (!preH || preSlotIdentityGen != want)
    if (pthread_cond_timedwait(&preCv, &muPre, &ts)) { fprintf(stderr, "FAIL pre slot never ready gen=%u\n", want); fails++; break; }
  pthread_mutex_unlock(&muPre);
}
static void wait_entered(int n) {
  struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); ts.tv_sec += 3;
  pthread_mutex_lock(&hold_direto.mu);
  while (hold_direto.entered < n)
    if (pthread_cond_timedwait(&hold_direto.cv, &hold_direto.mu, &ts)) { fprintf(stderr, "FAIL worker did not reach direto\n"); fails++; break; }
  pthread_mutex_unlock(&hold_direto.mu);
}
static void release_ticket(int n) {
  pthread_mutex_lock(&hold_direto.mu); hold_direto.go = n; pthread_cond_broadcast(&hold_direto.cv); pthread_mutex_unlock(&hold_direto.mu);
}
static void release(void) { pthread_mutex_lock(&g); gateOpen = 1; pthread_cond_broadcast(&cv); pthread_mutex_unlock(&g); }
static void block(void) { pthread_mutex_lock(&g); gateOpen = 0; pthread_mutex_unlock(&g); }
static int live(void) { int n = 0; pthread_mutex_lock(&g); for (int h = 1; h <= opens && h < MAXH; h++) n += !closes[h]; pthread_mutex_unlock(&g); return n; }
static Job mk(const char *u, const char *c) { Job j; memset(&j, 0, sizeof j); snprintf(j.urlA, sizeof j.urlA, "%s", u); snprintf(j.cabA, sizeof j.cabA, "%s", c); return j; }
static SeekrFonte fonte(const char *u, const char *c) {
  SeekrFonte f; memset(&f, 0, sizeof f); f.url = u; f.cabecalhos = c; f.durMs = 120000; return f;
}
#define CHECK(c, msg) do { if (!(c)) { fprintf(stderr, "FAIL %s\n", msg); fails++; } } while (0)
int main(void) {
  int d, fin = 0, st = 0;
  gate_init(&hold_direto);
  fakeNowMs = 1;
  block();
  miniatura_preaquecer("http://synthetic.invalid/slow-a", "X-Fake: 1"); waitFor(&started, ++st);
  pDesligar(); release(); waitFor(&finished, ++fin);
  CHECK(live() == 0, "late completion after exit leaves an open session");
  miniatura_preaquecer("http://synthetic.invalid/b", "X-Fake: 1"); waitFor(&finished, ++fin); st++;
  CHECK(live() == 1, "pre-warm opened");
  Job jb = mk("http://synthetic.invalid/b", "X-Fake: 1"); job_bind_pre_identity(&jb);
  preSoltar(&jb, jb.urlA, jb.cabA, 0, jb.preIdentityGen);
  CHECK(live() == 0, "complete-cache path must close the unadopted session (fim preSoltar)");
  pDesligar(); release();
  CHECK(live() == 0, "shutdown stays clean after fim preSoltar");
  block();
  miniatura_preaquecer("http://synthetic.invalid/slow-c", "X-Fake: 1"); waitFor(&started, ++st);
  Job jc = mk("http://synthetic.invalid/slow-c", "X-Fake: 1"); job_bind_pre_identity(&jc);
  fake_advance_ms(6000);
  CHECK(adotarPre(&jc, &d) == 0, "timeout adopts nothing");
  release(); waitFor(&finished, ++fin);
  CHECK(live() == 0, "late pre-warm after the adoption timeout stays open");
  miniatura_preaquecer("http://synthetic.invalid/slow-c", "X-Fake: 1");
  CHECK(started == st, "a timed-out link is reopened by repeated calls");
  pDesligar(); release();
  miniatura_preaquecer("http://synthetic.invalid/d", "X-Fake: one"); waitFor(&finished, ++fin); st++;
  Job jd = mk("http://synthetic.invalid/d", "X-Fake: two");
  long long h = adotarPre(&jd, &d);
  CHECK(h == 0, "adopted a session opened with different headers");
  miniatura_preaquecer("http://synthetic.invalid/d", "X-Fake: two"); waitFor(&finished, ++fin); st++;
  job_bind_pre_identity(&jd);
  h = adotarPre(&jd, &d);
  CHECK(h && !strcmp(hCab[h], "X-Fake: two") && d, "adopts the session for its own headers");
  mFechar(h);
  CHECK(live() == 0, "the replaced header session was closed");
  pDesligar(); release();
  Job old = mk("http://synthetic.invalid/old", "X-Fake: 1"); old.vivo = 1;
  miniatura_preaquecer("http://synthetic.invalid/e", "X-Fake: 1"); waitFor(&finished, ++fin); st++;
  pthread_mutex_lock(&mu); job = &old; pthread_mutex_unlock(&mu);
  encerrar();
  Job je = mk("http://synthetic.invalid/e", "X-Fake: 1");
  job_bind_pre_identity(&je);
  h = adotarPre(&je, &d);
  CHECK(h && !strcmp(hUrl[h], "http://synthetic.invalid/e"), "pPedir's encerrar dropped the new title's pre-warm");
  mFechar(h);
  pDesligar(); release();
  block();
  miniatura_preaquecer("http://synthetic.invalid/slow-f", "X-Fake: 1"); waitFor(&started, ++st);
  miniatura_preaquecer("http://synthetic.invalid/g", "X-Fake: 1");
  waitFor(&finished, ++fin); st++;
  CHECK(!strcmp(hUrl[opens], "http://synthetic.invalid/g"), "a new title cannot pre-warm while the previous one opens");
  miniatura_preaquecer("http://synthetic.invalid/g", "X-Fake: 1");
  CHECK(started == st, "repeated calls for the same title reopen it");
  release(); waitFor(&finished, ++fin);
  Job jg = mk("http://synthetic.invalid/g", "X-Fake: 1");
  job_bind_pre_identity(&jg);
  h = adotarPre(&jg, &d);
  CHECK(h && !strcmp(hUrl[h], "http://synthetic.invalid/g"), "new title adopts its own session");
  mFechar(h);
  CHECK(live() == 0, "the replaced title's late session stays open");
  pDesligar(); release();
  block();
  miniatura_preaquecer("http://synthetic.invalid/slow-h", "X-Fake: h"); waitFor(&started, ++st);
  Job j_old = mk("http://synthetic.invalid/slow-h", "X-Fake: h");
  pthread_mutex_lock(&muPre); j_old.preIdentityGen = preSlotIdentityGen; pthread_mutex_unlock(&muPre);
  miniatura_preaquecer("http://synthetic.invalid/other-h", "X-Fake: 1");
  release(); waitFor(&finished, ++fin);
  block();
  miniatura_preaquecer("http://synthetic.invalid/slow-h", "X-Fake: h"); waitFor(&started, ++st);
  release(); waitFor(&finished, ++fin);
  unsigned want_h = preSlotIdentityGen;
  waitPreReady(want_h);
  CHECK(live() == 1, "new identical pre-warm must park a handle");
  preSoltar(&j_old, j_old.urlA, j_old.cabA, 0, j_old.preIdentityGen);
  CHECK(live() == 1, "stale preSoltar must not invalidate newer same-url pre-warm");
  Job j_new = mk("http://synthetic.invalid/slow-h", "X-Fake: h");
  job_bind_pre_identity(&j_new);
  h = adotarPre(&j_new, &d);
  CHECK(h && !strcmp(hUrl[h], "http://synthetic.invalid/slow-h"), "adopter still gets the new pre-warm after stale preSoltar");
  mFechar(h);
  pDesligar(); release();
  jni_ok = 0;
  miniatura_preaquecer("http://synthetic.invalid/jni-fail", "X-Fake: 1"); release();
  jni_ok = 1;
  miniatura_preaquecer("http://synthetic.invalid/jni-fail", "X-Fake: 1"); waitFor(&finished, ++fin);
  { unsigned w = preSlotIdentityGen; waitPreReady(w); }
  CHECK(live() == 1, "jni failure must allow a later pre-warm for the same link");
  Job jj = mk("http://synthetic.invalid/jni-fail", "X-Fake: 1"); job_bind_pre_identity(&jj);
  h = adotarPre(&jj, &d); CHECK(h, "adopt after jni retry"); mFechar(h);
  pDesligar(); release();
  fail_all_create = 1;
  miniatura_preaquecer("http://synthetic.invalid/create-fail", "X-Fake: 1"); release();
  fail_all_create = 0;
  miniatura_preaquecer("http://synthetic.invalid/create-fail", "X-Fake: 1"); waitFor(&finished, ++fin);
  { unsigned w = preSlotIdentityGen; waitPreReady(w); }
  CHECK(live() == 1, "pthread_create failure must allow a later pre-warm");
  Job jf = mk("http://synthetic.invalid/create-fail", "X-Fake: 1"); job_bind_pre_identity(&jf);
  h = adotarPre(&jf, &d); CHECK(h, "adopt after create-fail retry"); mFechar(h);
  pDesligar(); release();
  miniatura_preaquecer("http://synthetic.invalid/stale-id", "X-Fake: z"); waitFor(&finished, ++fin); st++;
  Job j_bind = mk("http://synthetic.invalid/stale-id", "X-Fake: z");
  job_bind_pre_identity(&j_bind);
  unsigned staleGen = j_bind.preIdentityGen;
  CHECK(staleGen != 0, "bind must capture slot generation");
  miniatura_preaquecer("http://synthetic.invalid/other-z", "X-Fake: 1");
  release(); waitFor(&finished, ++fin);
  block();
  miniatura_preaquecer("http://synthetic.invalid/stale-id", "X-Fake: z"); waitFor(&started, ++st);
  release(); waitFor(&finished, ++fin);
  { unsigned w = preSlotIdentityGen; waitPreReady(w); }
  Job j_stale = mk("http://synthetic.invalid/stale-id", "X-Fake: z");
  j_stale.preIdentityGen = staleGen;
  fake_advance_ms(6000);
  CHECK(adotarPre(&j_stale, &d) == 0, "stale same-identity job must not adopt newer pre-warm");
  Job j_fresh = mk("http://synthetic.invalid/stale-id", "X-Fake: z"); job_bind_pre_identity(&j_fresh);
  h = adotarPre(&j_fresh, &d);
  CHECK(h && !strcmp(hUrl[h], "http://synthetic.invalid/stale-id"), "fresh-bound job adopts current slot");
  mFechar(h);
  pDesligar(); release();
  block();
  miniatura_preaquecer("http://synthetic.invalid/slow-zero", "X-Fake: 1"); waitFor(&started, ++st);
  Job jz = mk("http://synthetic.invalid/slow-zero", "X-Fake: 1");
  CHECK(jz.preIdentityGen == 0 && adotarPre(&jz, &d) == 0, "zero token adopts nothing before bind");
  release(); waitFor(&finished, ++fin);
  { unsigned w = preSlotIdentityGen; waitPreReady(w); }
  job_bind_pre_identity(&jz);
  h = adotarPre(&jz, &d);
  CHECK(h && !strcmp(hUrl[h], "http://synthetic.invalid/slow-zero"), "bind after pre-warm still adopts");
  mFechar(h);
  pDesligar(); release();
  miniatura_preaquecer("http://synthetic.invalid/stop-adopt", "X-Fake: 1"); waitFor(&finished, ++fin); st++;
  { unsigned w = preSlotIdentityGen; waitPreReady(w); }
  Job jp = mk("http://synthetic.invalid/stop-adopt", "X-Fake: 1");
  job_bind_pre_identity(&jp);
  jp.parar = 1;
  h = adotarPre(&jp, &d);
  CHECK(h == 0, "canceled job must not adopt parked handle");
  Job jp2 = mk("http://synthetic.invalid/stop-adopt", "X-Fake: 1"); job_bind_pre_identity(&jp2);
  h = adotarPre(&jp2, &d);
  CHECK(h && !strcmp(hUrl[h], "http://synthetic.invalid/stop-adopt"), "handle still available after canceled adopt");
  mFechar(h);
  pDesligar(); release();
  /* 11. parked pre-warm, then the real pPedir worker thread fails to start. */
  { int base = snap(&finished);
    miniatura_preaquecer("http://synthetic.invalid/pedir-fail", "X-Fake: 1");
    waitFor(&finished, base + 1); }
  { unsigned w; pthread_mutex_lock(&muPre); w = preSlotIdentityGen; pthread_mutex_unlock(&muPre); waitPreReady(w);
    CHECK(live() == 1, "prewarm parked before worker create failure"); }
  int opensBeforeFail = snap(&opens);
  fail_fn = trabalhar;
  SeekrFonte sf = fonte("http://synthetic.invalid/pedir-fail", "X-Fake: 1");
  pPedir(&sf);
  fail_fn = NULL;
  CHECK(job == NULL, "pPedir create failure must clear job");
  CHECK(live() == 0 && closes[opensBeforeFail] == 1, "pPedir create failure must release the parked session");
  { int base = snap(&finished);
    miniatura_preaquecer("http://synthetic.invalid/pedir-fail", "X-Fake: 1");
    waitFor(&finished, base + 1); }
  { unsigned w; pthread_mutex_lock(&muPre); w = preSlotIdentityGen; pthread_mutex_unlock(&muPre); waitPreReady(w); }
  Job jRetry = mk("http://synthetic.invalid/pedir-fail", "X-Fake: 1"); job_bind_pre_identity(&jRetry);
  h = adotarPre(&jRetry, &d); CHECK(h, "adopt after pPedir worker failure"); mFechar(h);
  pDesligar(); release();
  /* 13. complete cache: real pPedir worker goes to fim without adopting. */
  fim_only = 1;
  { int base = snap(&finished);
    miniatura_preaquecer("http://synthetic.invalid/cache-fim", "X-Fake: 1");
    waitFor(&finished, base + 1); }
  int cacheH = snap(&opens);
  { int base = snap(&finished);
    sf = fonte("http://synthetic.invalid/cache-fim", "X-Fake: 1");
    pPedir(&sf);
    waitFor(&finished, base + 1); }
  CHECK(closes[cacheH] == 1 && live() == 0, "complete-cache fim must close the parked session");
  pDesligar(); release();
  fim_only = 0;
  /* 14. late pre-warm: job exists first; trabalharDireto binds once and adopts. */
  hold_arm = 1; hold_direto.entered = hold_direto.go = 0;
  sf = fonte("http://synthetic.invalid/late-bind", "X-Fake: 1");
  pPedir(&sf);
  wait_entered(1);
  { int base = snap(&finished);
    miniatura_preaquecer("http://synthetic.invalid/late-bind", "X-Fake: 1");
    waitFor(&finished, base + 1); }
  { unsigned w; pthread_mutex_lock(&muPre); w = preSlotIdentityGen; pthread_mutex_unlock(&muPre); waitPreReady(w); }
  int lateH = snap(&opens);
  { int base = snap(&finished);
    release_ticket(1);
    waitFor(&finished, base + 1); }
  CHECK(opens == lateH && closes[lateH] == 1, "late prewarm adopted by the live job, no second open");
  pDesligar(); release();
  hold_arm = 0;
  /* 15. canceled worker vs replacement with the same url and headers. */
  { int base = snap(&finished);
    miniatura_preaquecer("http://synthetic.invalid/race-same", "X-Fake: 1");
    waitFor(&finished, base + 1); }
  { unsigned w; pthread_mutex_lock(&muPre); w = preSlotIdentityGen; pthread_mutex_unlock(&muPre); waitPreReady(w); }
  int raceH = snap(&opens);
  hold_arm = 1; hold_direto.entered = hold_direto.go = 0;
  sf = fonte("http://synthetic.invalid/race-same", "X-Fake: 1");
  pPedir(&sf);
  wait_entered(1);
  pTentar();
  pPedir(&sf);
  wait_entered(2);
  { int base = snap(&finished);
    release_ticket(1);
    waitFor(&finished, base + 1); }
  CHECK(live() == 1 && closes[raceH] == 0 && opens == raceH, "canceled worker stole or closed the successor prewarm");
  { int base = snap(&finished);
    release_ticket(2);
    waitFor(&finished, base + 1); }
  CHECK(closes[raceH] == 1 && opens == raceH, "successor must adopt the original parked session");
  pDesligar(); release();
  hold_arm = 0;
  /* 16. encerrar must not touch a live job after an orphan worker frees it. */
  uaf_len = sizeof(Job);
  Job *uj = mmap(NULL, uaf_len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
  CHECK(uj != MAP_FAILED, "mmap job");
  if (uj != MAP_FAILED) {
    memset(uj, 0, sizeof *uj);
    uj->vivo = 1; uj->preIdentityGen = 1;
    pthread_mutex_lock(&mu); job = uj; pthread_mutex_unlock(&mu);
    uaf_stage = 0;
    uaf_job = uj; uaf_arm = 1;
    struct sigaction sa; memset(&sa, 0, sizeof sa); sa.sa_handler = on_uaf;
    sigaction(SIGSEGV, &sa, NULL); sigaction(SIGBUS, &sa, NULL);
    pthread_t uth;
    real_create(&uth, NULL, uaf_worker, uj);
    int crashed = 0;
    if (sigsetjmp(uaf_jmp, 1) == 0) encerrar();
    else crashed = 1;
    pthread_join(uth, NULL);
    uaf_arm = 0;
    CHECK(!crashed, "encerrar used job memory after orphan free");
  }
  pDesligar(); release();
  for (int k = 1; k <= opens && k < MAXH; k++) CHECK(closes[k] == 1, "every synthetic session closes exactly once");
  if (fails) return 1;
  printf("PASS prewarm lifecycle (%d synthetic sessions)\n", opens);
  return 0;
}
'''

with tempfile.TemporaryDirectory() as d:
    c = Path(d) / "test.c"
    c.write_text(prefix + prewarm + direto + encerrar + tentar + desligar + pedir + main)
    src = str(root / "src")
    subprocess.run(
        ["cc", "-std=c11", "-pthread", "-Wall", "-Wno-unused-function", "-I", src, str(c), "-o", str(Path(d) / "test")],
        check=True,
    )
    subprocess.run([str(Path(d) / "test")], check=True, timeout=20)
