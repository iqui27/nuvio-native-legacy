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
typedef unsigned Uint32;
typedef struct { unsigned char *jpg; } Tile;
typedef struct {
  char urlA[4096], cabA[512]; int parar, vivo, n;
  unsigned preIdentityGen;
  Tile *tiles; unsigned char *tentado, *slotState; long *slotFrame, *framePts, *slotKey;
} Job;
enum { M_PARADO };
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static Job *job; static int estado; static char urlAtual[4096]; static int geracao;
static void texLimpar(void) {}
static int bitsDebug(void) { return 0; }
static int jni_ok = 1;
static int jniPreparar(void) { return jni_ok; }
static void mConfigurar(int b) { (void)b; }
static void tex_aliviar(void) {}
static Uint32 SDL_GetTicks(void) { return 0; }
static int video_pronto(void) { return 0; }
static int video_tocando(void) { return 0; }
static int video_falhou(void) { return 0; }
static int parou(Job *j) { return j->parar; }
static void dormir(Job *j, int ms) { (void)j; (void)ms; }   // fake clock: the 5 s wait elapses at once

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
#undef pthread_create
static void job_bind_pre_identity(Job *j) {
  pthread_mutex_lock(&muPre);
  j->preIdentityGen = preMesmo(j->urlA, j->cabA) ? preSlotIdentityGen : 0;
  pthread_mutex_unlock(&muPre);
}
static int fails;
// Bounded: a pre-warm that never starts reports FAIL instead of hanging.
static void waitFor(int *v, int n) {
  struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); ts.tv_sec += 3;
  pthread_mutex_lock(&g);
  while (*v < n) if (pthread_cond_timedwait(&cv, &g, &ts)) { fprintf(stderr, "FAIL expected thread event never came\n"); fails++; break; }
  pthread_mutex_unlock(&g);
}
static void release(void) { pthread_mutex_lock(&g); gateOpen = 1; pthread_cond_broadcast(&cv); pthread_mutex_unlock(&g); }
static void block(void) { pthread_mutex_lock(&g); gateOpen = 0; pthread_mutex_unlock(&g); }
static int live(void) { int n = 0; pthread_mutex_lock(&g); for (int h = 1; h <= opens; h++) n += !closes[h]; pthread_mutex_unlock(&g); return n; }
static Job mk(const char *u, const char *c) { Job j; memset(&j, 0, sizeof j); snprintf(j.urlA, sizeof j.urlA, "%s", u); snprintf(j.cabA, sizeof j.cabA, "%s", c); return j; }
#define CHECK(c, msg) do { if (!(c)) { fprintf(stderr, "FAIL %s\n", msg); fails++; } } while (0)
int main(void) {
  int d, fin = 0, st = 0;
  // 1. late completion after the player exits: the open finishes after pDesligar and must close.
  block();
  miniatura_preaquecer("http://synthetic.invalid/slow-a", "X-Fake: 1"); waitFor(&started, ++st);
  pDesligar(); release(); waitFor(&finished, ++fin);
  CHECK(live() == 0, "late completion after exit leaves an open session");
  // 2. complete cache: the job never adopts the session; production fim calls preSoltar (same as shutdown for this stub).
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
  { int wantFin = finished + 1;
  miniatura_preaquecer("http://synthetic.invalid/slow-h", "X-Fake: h"); waitFor(&started, ++st);
  release(); waitFor(&finished, wantFin); }
  CHECK(live() == 1, "new identical pre-warm must park a handle");
  preSoltar(j_old.urlA, j_old.cabA, 0, j_old.preIdentityGen);   // stale identity: must not close the new slot
  CHECK(live() == 1, "stale preSoltar must not invalidate newer same-url pre-warm");
  Job j_new = mk("http://synthetic.invalid/slow-h", "X-Fake: h"); job_bind_pre_identity(&j_new);
  h = adotarPre(&j_new, &d);
  CHECK(h && !strcmp(hUrl[h], "http://synthetic.invalid/slow-h"), "adopter still gets the new pre-warm after stale preSoltar");
  mFechar(h);
  pDesligar(); release();
  // 7. jniPreparar fails after slot reservation: must not leave identity blocking retry.
  jni_ok = 0;
  miniatura_preaquecer("http://synthetic.invalid/jni-fail", "X-Fake: 1"); release();
  jni_ok = 1;
  { int wantSt = started + 1;
  miniatura_preaquecer("http://synthetic.invalid/jni-fail", "X-Fake: 1"); waitFor(&started, wantSt); }
  CHECK(live() == 1, "jni failure must allow a later pre-warm for the same link");
  Job jj = mk("http://synthetic.invalid/jni-fail", "X-Fake: 1"); job_bind_pre_identity(&jj);
  h = adotarPre(&jj, &d); CHECK(h, "adopt after jni retry"); mFechar(h);
  pDesligar(); release();
  // 8. pthread_create fails: slot must not stay stuck (preSlotIdentityGen cleared).
  fail_pthread_create = 1;
  miniatura_preaquecer("http://synthetic.invalid/create-fail", "X-Fake: 1"); release();
  fail_pthread_create = 0;
  { int wantSt = started + 1;
  miniatura_preaquecer("http://synthetic.invalid/create-fail", "X-Fake: 1"); waitFor(&started, wantSt); }
  CHECK(live() == 1, "pthread_create failure must allow a later pre-warm");
  Job jf = mk("http://synthetic.invalid/create-fail", "X-Fake: 1"); job_bind_pre_identity(&jf);
  h = adotarPre(&jf, &d); CHECK(h, "adopt after create-fail retry"); mFechar(h);
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
