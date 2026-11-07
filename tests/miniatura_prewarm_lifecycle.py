"""Drive the production pre-warm block, encerrar and pDesligar with synthetic handles.

Covers: late completion after exit, a set served from the complete cache (never adopted), the 5 s adoption
timeout, the same URL with distinct fake headers, and replacement (pPedir's encerrar, a new title while one opens).
No network: mAbrirDireto/mFechar are stubs that count synthetic handles.
"""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
text = (root / 'src/miniatura_android.c').read_text()


def cut(start, end):
    a = text.index(start)
    return text[a:text.index(end, a)]


prewarm = cut('static pthread_mutex_t muPre', '\n// 1 = direct mode ran')
encerrar = cut('static void encerrar(void) {', '\n}\n') + '\n}\n'
desligar = cut('static void pDesligar(void)', '\n') + '\n'

prefix = r'''
#define _DEFAULT_SOURCE
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
typedef unsigned Uint32;
typedef struct { unsigned char *jpg; } Tile;
typedef struct {
  char urlA[4096], cabA[512]; int parar, vivo, n;
  unsigned preIdentityGen;
  int g;
  Tile *tiles; unsigned char *tentado, *slotState; long *slotFrame, *framePts, *slotKey;
} Job;
typedef struct {
  const char *imdb; int t, e; long durMs;
  const char *url;
  const char *cabecalhos;
} SeekrFonte;
enum { M_PARADO, M_FALHOU, M_BUSCANDO, M_PRONTO };
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static Job *job; static int estado; static char urlAtual[4096]; static char cabAtual[512]; static int geracao;
static void texLimpar(void) {}
static int bitsDebug(void) { return 0; }
static int jni_ok = 1;
static int jniPreparar(void) { return jni_ok; }
static void mConfigurar(int b) { (void)b; }
static void tex_aliviar(void) {}
static volatile Uint32 fakeNowMs;
static Uint32 SDL_GetTicks(void) { return fakeNowMs; }
static void fake_advance_ms(int ms) { fakeNowMs += (Uint32)ms; }
static int video_pronto(void) { return 0; }
static int video_tocando(void) { return 0; }
static int video_falhou(void) { return 0; }
static int parou(Job *j) { return j->parar; }
static void dormir(Job *j, int ms) {
  Uint32 end = fakeNowMs + (Uint32)ms;
  while (fakeNowMs < end && (!j || !parou(j))) fakeNowMs++;
}
static void encerrar(void);
static void job_bind_pre_identity(Job *j);
static void *trabalhar(void *u);

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
  assert(h < MAXH);
  snprintf(hUrl[h], sizeof hUrl[h], "%s", url); snprintf(hCab[h], sizeof hCab[h], "%s", cab);
  pthread_mutex_unlock(&g);
  return h;
}
static int mPrepararDecoder(long long h) { return h > 0; }
static void mFechar(long long h) {
  if (!h) return;
  pthread_mutex_lock(&g); assert(closes[h] == 0 && "closed twice"); closes[h]++; pthread_mutex_unlock(&g);
}
typedef struct { void *(*fn)(void *); void *arg; } Tramp;
static void *trampoline(void *u) {
  Tramp t = *(Tramp *)u; free(u);
  t.fn(t.arg);
  pthread_mutex_lock(&g); finished++; pthread_cond_broadcast(&cv); pthread_mutex_unlock(&g);
  return NULL;
}
static int fail_pthread_create;
static int tcreate(pthread_t *t, const pthread_attr_t *at, void *(*fn)(void *), void *arg) {
  if (fail_pthread_create) return EAGAIN;
  Tramp *x = malloc(sizeof *x); x->fn = fn; x->arg = arg;
  int r = pthread_create(t, at, trampoline, x);
  if (r) free(x);
  return r;
}
#define pthread_create tcreate
'''

main = r'''
static void *trabalhar(void *u) { (void)u; return NULL; }
static void pPedir(const SeekrFonte *f) {
  if (!f || !f->url || strncmp(f->url, "http", 4) || f->durMs < 60000) return;
  encerrar();
  if (!jniPreparar()) return;
  Job *j = calloc(1, sizeof *j);
  if (!j) return;
  snprintf(j->urlA, sizeof j->urlA, "%s", f->url);
  snprintf(j->cabA, sizeof j->cabA, "%s", f->cabecalhos ? f->cabecalhos : "");
  job_bind_pre_identity(j);
  pthread_mutex_lock(&mu);
  j->g = ++geracao;
  j->vivo = 1;
  job = j;
  estado = M_BUSCANDO;
  snprintf(urlAtual, sizeof urlAtual, "%s", f->url);
  snprintf(cabAtual, sizeof cabAtual, "%s", f->cabecalhos ? f->cabecalhos : "");
  pthread_mutex_unlock(&mu);
  pthread_t t;
  pthread_attr_t at;
  pthread_attr_init(&at);
  pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);
  if (pthread_create(&t, &at, trabalhar, j)) {
    unsigned preId = j->preIdentityGen;
    pthread_mutex_lock(&mu); j->vivo = 0; pthread_mutex_unlock(&mu);
    if (preId) preSoltar(j->urlA, j->cabA, 0, preId);
    encerrar();
  }
  pthread_attr_destroy(&at);
}
static int fails;
// Bounded: a pre-warm that never starts reports FAIL instead of hanging.
static void waitFor(int *v, int n) {
  struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); ts.tv_sec += 3;
  pthread_mutex_lock(&g);
  while (*v < n) if (pthread_cond_timedwait(&cv, &g, &ts)) { fprintf(stderr, "FAIL expected thread event never came\n"); fails++; break; }
  pthread_mutex_unlock(&g);
}
/* Wait until preSlotIdentityGen == want and a handle is parked (not a stale slot from a prior case). */
static void waitPreReady(unsigned want) {
  struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); ts.tv_sec += 3;
  pthread_mutex_lock(&muPre);
  while (!preH || preSlotIdentityGen != want)
    if (pthread_cond_timedwait(&preCv, &muPre, &ts)) { fprintf(stderr, "FAIL pre slot never ready gen=%u\n", want); fails++; break; }
  pthread_mutex_unlock(&muPre);
}
static void release(void) { pthread_mutex_lock(&g); gateOpen = 1; pthread_cond_broadcast(&cv); pthread_mutex_unlock(&g); }
static void block(void) { pthread_mutex_lock(&g); gateOpen = 0; pthread_mutex_unlock(&g); }
static int live(void) { int n = 0; pthread_mutex_lock(&g); for (int h = 1; h <= opens; h++) n += !closes[h]; pthread_mutex_unlock(&g); return n; }
static Job mk(const char *u, const char *c) { Job j; memset(&j, 0, sizeof j); snprintf(j.urlA, sizeof j.urlA, "%s", u); snprintf(j.cabA, sizeof j.cabA, "%s", c); return j; }
#define CHECK(c, msg) do { if (!(c)) { fprintf(stderr, "FAIL %s\n", msg); fails++; } } while (0)
int main(void) {
  int d, fin = 0, st = 0;
  fakeNowMs = 1;
  // 1. late completion after the player exits: the open finishes after pDesligar and must close.
  block();
  miniatura_preaquecer("http://synthetic.invalid/slow-a", "X-Fake: 1"); waitFor(&started, ++st);
  pDesligar(); release(); waitFor(&finished, ++fin);
  CHECK(live() == 0, "late completion after exit leaves an open session");
  // 2. complete cache: fim path preSoltar closes the unadopted parked session (production trabalhar fim); this stub mirrors that.
  miniatura_preaquecer("http://synthetic.invalid/b", "X-Fake: 1"); waitFor(&finished, ++fin); st++;
  CHECK(live() == 1, "pre-warm opened");
  Job jb = mk("http://synthetic.invalid/b", "X-Fake: 1"); job_bind_pre_identity(&jb);
  preSoltar(jb.urlA, jb.cabA, 0, jb.preIdentityGen);
  CHECK(live() == 0, "complete-cache path must close the unadopted session (fim preSoltar)");
  pDesligar(); release();
  CHECK(live() == 0, "shutdown stays clean after fim preSoltar");
  // 3. adoption timeout: the job opens its own; the late pre-warm must not stay parked.
  block();
  miniatura_preaquecer("http://synthetic.invalid/slow-c", "X-Fake: 1"); waitFor(&started, ++st);
  Job jc = mk("http://synthetic.invalid/slow-c", "X-Fake: 1"); job_bind_pre_identity(&jc);
  fake_advance_ms(6000);
  CHECK(adotarPre(&jc, &d) == 0, "timeout adopts nothing");
  release(); waitFor(&finished, ++fin);
  CHECK(live() == 0, "late pre-warm after the adoption timeout stays open");
  miniatura_preaquecer("http://synthetic.invalid/slow-c", "X-Fake: 1");   // the player keeps calling while opening
  CHECK(started == st, "a timed-out link is reopened by repeated calls");
  pDesligar(); release();
  // 4. same URL, distinct fake headers: never adopt a session opened with other headers.
  miniatura_preaquecer("http://synthetic.invalid/d", "X-Fake: one"); waitFor(&finished, ++fin); st++;
  Job jd = mk("http://synthetic.invalid/d", "X-Fake: two");
  long long h = adotarPre(&jd, &d);
  CHECK(h == 0, "adopted a session opened with different headers");
  miniatura_preaquecer("http://synthetic.invalid/d", "X-Fake: two"); waitFor(&finished, ++fin); st++;
  job_bind_pre_identity(&jd);
  h = adotarPre(&jd, &d);
  CHECK(h && !strcmp(hCab[h], "X-Fake: two") && d, "adopts the session for its own headers");
  mFechar(h);                                     // the job owns it now (trabalhar's fim)
  CHECK(live() == 0, "the replaced header session was closed");
  pDesligar(); release();
  // 5a. replacement through pPedir: encerrar for the old job must keep the new title's pre-warm.
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
  // 5b. a new title while the previous one is still opening: the new one pre-warms, the old one closes once.
  block();
  miniatura_preaquecer("http://synthetic.invalid/slow-f", "X-Fake: 1"); waitFor(&started, ++st);
  miniatura_preaquecer("http://synthetic.invalid/g", "X-Fake: 1");
  waitFor(&finished, ++fin); st++;
  CHECK(!strcmp(hUrl[opens], "http://synthetic.invalid/g"),
        "a new title cannot pre-warm while the previous one opens");
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
  // 6. same identity: stale old-job preSoltar must not drop a newer identical pre-warm slot.
  block();
  miniatura_preaquecer("http://synthetic.invalid/slow-h", "X-Fake: h"); waitFor(&started, ++st);
  Job j_old;
  memset(&j_old, 0, sizeof j_old);
  snprintf(j_old.urlA, sizeof j_old.urlA, "%s", "http://synthetic.invalid/slow-h");
  snprintf(j_old.cabA, sizeof j_old.cabA, "%s", "X-Fake: h");
  pthread_mutex_lock(&muPre); j_old.preIdentityGen = preSlotIdentityGen; pthread_mutex_unlock(&muPre);
  miniatura_preaquecer("http://synthetic.invalid/other-h", "X-Fake: 1");   // replace slot (other link)
  release(); waitFor(&finished, ++fin);
  block();
  miniatura_preaquecer("http://synthetic.invalid/slow-h", "X-Fake: h"); waitFor(&started, ++st);
  release(); waitFor(&finished, ++fin);
  unsigned want_h = preSlotIdentityGen;
  waitPreReady(want_h);
  CHECK(live() == 1, "new identical pre-warm must park a handle");
  preSoltar(j_old.urlA, j_old.cabA, 0, j_old.preIdentityGen);   // stale identity: must not close the new slot
  CHECK(live() == 1, "stale preSoltar must not invalidate newer same-url pre-warm");
  Job j_new = mk("http://synthetic.invalid/slow-h", "X-Fake: h");
  job_bind_pre_identity(&j_new);
  h = adotarPre(&j_new, &d);
  CHECK(h && !strcmp(hUrl[h], "http://synthetic.invalid/slow-h"), "adopter still gets the new pre-warm after stale preSoltar");
  mFechar(h);
  pDesligar(); release();
  // 7. jniPreparar fails after slot reservation: must not leave identity blocking retry.
  jni_ok = 0;
  miniatura_preaquecer("http://synthetic.invalid/jni-fail", "X-Fake: 1"); release();
  jni_ok = 1;
  miniatura_preaquecer("http://synthetic.invalid/jni-fail", "X-Fake: 1"); waitFor(&finished, ++fin);
  { unsigned w = preSlotIdentityGen; waitPreReady(w); }
  CHECK(live() == 1, "jni failure must allow a later pre-warm for the same link");
  Job jj = mk("http://synthetic.invalid/jni-fail", "X-Fake: 1"); job_bind_pre_identity(&jj);
  h = adotarPre(&jj, &d); CHECK(h, "adopt after jni retry"); mFechar(h);
  pDesligar(); release();
  // 8. pthread_create fails: slot must not stay stuck (preSlotIdentityGen cleared).
  fail_pthread_create = 1;
  miniatura_preaquecer("http://synthetic.invalid/create-fail", "X-Fake: 1"); release();
  fail_pthread_create = 0;
  miniatura_preaquecer("http://synthetic.invalid/create-fail", "X-Fake: 1"); waitFor(&finished, ++fin);
  { unsigned w = preSlotIdentityGen; waitPreReady(w); }
  CHECK(live() == 1, "pthread_create failure must allow a later pre-warm");
  Job jf = mk("http://synthetic.invalid/create-fail", "X-Fake: 1"); job_bind_pre_identity(&jf);
  h = adotarPre(&jf, &d); CHECK(h, "adopt after create-fail retry"); mFechar(h);
  pDesligar(); release();
  // 9. stale identity: a job that bound an older round must not adopt the newer same-url pre-warm.
  miniatura_preaquecer("http://synthetic.invalid/stale-id", "X-Fake: z"); waitFor(&finished, ++fin); st++;
  Job j_bind = mk("http://synthetic.invalid/stale-id", "X-Fake: z");
  job_bind_pre_identity(&j_bind);
  unsigned staleGen = j_bind.preIdentityGen;
  CHECK(staleGen != 0, "bind must capture slot generation");
  miniatura_preaquecer("http://synthetic.invalid/other-z", "X-Fake: 1");   // new slot, bumps round
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
  // 10. zero pre-identity at job creation: late pre-warm parks until bind+adopt.
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
  // 11. pthread_create fails in pPedir: encerrar cleans job, prewarm slot released.
  fail_pthread_create = 1;
  miniatura_preaquecer("http://synthetic.invalid/pedir-fail", "X-Fake: 1"); waitFor(&finished, ++fin); st++;
  unsigned slotGen = preSlotIdentityGen;
  SeekrFonte sf = {0};
  sf.url = "http://synthetic.invalid/pedir-fail";
  sf.cabecalhos = "X-Fake: 1";
  sf.durMs = 120000;
  pPedir(&sf);
  fail_pthread_create = 0;
  pthread_mutex_lock(&mu);
  Job *jLeak = job;
  pthread_mutex_unlock(&mu);
  CHECK(!jLeak, "pPedir create failure must clear job");
  CHECK(live() == 0, "pPedir create failure must release prewarm session");
  miniatura_preaquecer("http://synthetic.invalid/pedir-fail", "X-Fake: 1"); waitFor(&finished, ++fin); st++;
  Job jRetry = mk("http://synthetic.invalid/pedir-fail", "X-Fake: 1"); job_bind_pre_identity(&jRetry);
  CHECK(jRetry.preIdentityGen != slotGen, "new prewarm round after pPedir failure");
  { unsigned w = preSlotIdentityGen; waitPreReady(w); }
  h = adotarPre(&jRetry, &d); CHECK(h, "adopt after pPedir worker failure"); mFechar(h);
  pDesligar(); release();
  // 12. canceled job must not take a parked handle.
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
  for (int k = 1; k <= opens; k++) CHECK(closes[k] == 1, "every synthetic session closes exactly once");
  if (fails) return 1;
  printf("PASS prewarm lifecycle (%d synthetic sessions)\n", opens);
  return 0;
}
'''

with tempfile.TemporaryDirectory() as d:
    c = Path(d) / 'test.c'
    c.write_text(prefix + prewarm + encerrar + desligar + main)
    subprocess.run(['cc', '-std=c11', '-pthread', '-Wall', '-Wno-unused-function', str(c), '-o', str(Path(d) / 'test')], check=True)
    subprocess.run([str(Path(d) / 'test')], check=True, timeout=10)
