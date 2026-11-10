"""Lazy decoders (mQuadro/mExato -> MiniaturaMotor.decoder) under the hardware-decoder lease.

Production C cut from src/miniatura_android.c: mQuadro, mExato and the lease section (decPreparar, decLiberar,
and decEntrar/decSair when present). Substituted: JNIEnv (a fake whose quadro/quadroExato model
MiniaturaMotor.decoder: hardware first above 1088p, software first up to 1088p, hardware only when the C side
allows it), mPrepararDecoder, mFechar (JNI close, then the production decLiberar), desempacotar. No network.
Invariant: at most one session holds a hardware decoder. Built with ThreadSanitizer (lock order, races).
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
src = Path(sys.argv[1]) if len(sys.argv) > 1 else root / "src/miniatura_android.c"
text = src.read_text()


def cut(start, end):
    a = text.index(start)
    return text[a:text.index(end, a)]


leased = "static int mQuadro(Job *j," in text
quadros = cut("static int decEntrar(Job *j" if leased else "static int mQuadro(", "\nstatic int desempacotar(JNIEnv")
lease = cut("static pthread_mutex_t muDec", "\nstatic int preMesmo(")

prefix = r'''
#define _DEFAULT_SOURCE
#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define SIG_N 4
typedef long long jlong; typedef int jint; typedef unsigned char jboolean; typedef void *jbyteArray;
#define JNI_TRUE 1
#define JNI_FALSE 0
struct FakeJni {
  void *(*CallStaticObjectMethod)(void *env, void *cls, void *m, ...);
  int (*ExceptionCheck)(void *env);
  void (*ExceptionClear)(void *env);
};
typedef const struct FakeJni *JNIEnv;
typedef struct { _Atomic int parar; } Job;
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER, muPre = PTHREAD_MUTEX_INITIALIZER;
static unsigned preGen;
'''

fake = r'''
// ---- fake MiniaturaMotor (Kotlin) ----
enum { NONE, SOFT, HARD };
static pthread_mutex_t km = PTHREAD_MUTEX_INITIALIZER;
static int dec[8], altura[8], falhaCriar[8], excecao, hwAtivos, hwMax, ultimoHw;
static char tagQ, tagE, tagCls;
static void *jQuadro = &tagQ, *jExato = &tagE, *cls = &tagCls;
static void conta(void) { int n = 0; for (int i = 0; i < 8; i++) n += dec[i] == HARD; hwAtivos = n; if (n > hwMax) hwMax = n; }
static void *kQuadro(void *env, void *c, void *m, ...) {
  (void)env; (void)c;
  va_list ap; va_start(ap, m);
  long long h = va_arg(ap, jlong); (void)va_arg(ap, jlong);
  (void)va_arg(ap, int);
  if (m == jQuadro) (void)va_arg(ap, int);
  int hw = LEASED ? va_arg(ap, int) : 1;   // before the lease reached this path, Kotlin always could
  va_end(ap);
  pthread_mutex_lock(&km);
  ultimoHw = hw;
  if (dec[h] == NONE && !falhaCriar[h]) {
    // MiniaturaMotor.decoder: above 1088p hardware first, then software; up to 1088p software first
    dec[h] = altura[h] > 1088 ? (hw ? HARD : SOFT) : SOFT;
    conta();
  }
  int tem = dec[h] != NONE;
  pthread_mutex_unlock(&km);
  return tem ? (void *)1 : NULL;
}
static int kCheck(void *env) { (void)env; int e = excecao; return e; }
static void kClear(void *env) { (void)env; excecao = 0; }
static const struct FakeJni fakeJni = { kQuadro, kCheck, kClear };
static JNIEnv fakeEnv = &fakeJni;
static void *SDL_AndroidGetJNIEnv(void) { return &fakeEnv; }
static int desempacotar(JNIEnv *env, jbyteArray a, long *pts, float sig[SIG_N], int *temSig, unsigned char **jpg, int *nJpg) {
  (void)env; (void)sig; (void)jpg; (void)nJpg; *pts = 0; *temSig = 0; return a != NULL;
}
__attribute__((unused)) static int mDecoderHardware(long long h) {
  pthread_mutex_lock(&km); int r = dec[h] == HARD; pthread_mutex_unlock(&km); return r;
}
static int mPrepararDecoder(long long h) {   // decoderDireto: always hardware
  pthread_mutex_lock(&km); dec[h] = HARD; conta(); pthread_mutex_unlock(&km); return 1;
}
static void decLiberar(long long h);
static void mFechar(long long h) {           // production order: JNI close, then decLiberar(h)
  pthread_mutex_lock(&km); dec[h] = NONE; conta(); pthread_mutex_unlock(&km);
  decLiberar(h);
}
// ---- deterministic gate on the production dec_wait hook ----
static pthread_mutex_t gm = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gc = PTHREAD_COND_INITIALIZER;
static int esperando, terminou;
#define MINI_OWNER_HOOK(name, p) do { (void)(p); if (!strcmp(name, "dec_wait")) { \
  pthread_mutex_lock(&gm); esperando++; pthread_cond_broadcast(&gc); pthread_mutex_unlock(&gm); } } while (0)
'''

suffix = r'''
static int falhas;
#define CHECK(c, msg) do { if (c) printf("ok   %s\n", msg); else { printf("FAIL %s\n", msg); falhas++; } } while (0)
#if LEASED
#define QUADRO(j, h) ({ long p; float s[SIG_N]; int ok; mQuadro(j, h, 0, 1, 0, &p, s, &ok, NULL, NULL); })
#define EXATO(j, h) ({ long p; float s[SIG_N]; int ok; mExato(j, h, 0, &p, s, &ok, NULL, NULL); })
#else
#define QUADRO(j, h) ({ (void)(j); long p; float s[SIG_N]; int ok; mQuadro(h, 0, 1, 0, &p, s, &ok, NULL, NULL); })
#define EXATO(j, h) ({ (void)(j); long p; float s[SIG_N]; int ok; mExato(h, 0, &p, s, &ok, NULL, NULL); })
#endif
typedef struct { Job *j; long long h; int ok; } Succ;
static void *sucessor(void *u) {
  Succ *s = u;
  s->ok = decPreparar(s->j, 0, s->h);
  pthread_mutex_lock(&gm); terminou = 1; pthread_cond_broadcast(&gc); pthread_mutex_unlock(&gm);
  return NULL;
}
static void cancelar(Job *j) {               // encerrar: parar under mu, then wake lease waiters
  pthread_mutex_lock(&mu); j->parar = 1; pthread_mutex_unlock(&mu);
  decAcordar();
}
static void zerar(void) {
  for (int i = 0; i < 8; i++) { if (dec[i]) mFechar(i); dec[i] = NONE; falhaCriar[i] = 0; altura[i] = 2160; }
  hwMax = hwAtivos = 0; esperando = terminou = 0; excecao = 0;
}
int main(void) {
  // 1. Mixed fallback/successor: the old job's fallback frame created a 4K hardware decoder on hA; the job is
  //    canceled mid-frame; the successor's direct worker asks for the decoder on hB. It must wait (worker thread)
  //    until the old fallback closes hA, never run two hardware decoders.
  zerar();
  { Job a = {0}, b = {0};
    QUADRO(&a, 1);
    CHECK(dec[1] == HARD, "live fallback above 1088p gets the hardware decoder when the lease is free");
    cancelar(&a);
    Succ s = { &b, 2, 0 }; pthread_t t; pthread_create(&t, NULL, sucessor, &s);
    pthread_mutex_lock(&gm); while (!esperando && !terminou) pthread_cond_wait(&gc, &gm);
    int esperou = esperando && !terminou; pthread_mutex_unlock(&gm);
    CHECK(esperou, "successor waits for the old fallback's hardware decoder (no second one)");
    QUADRO(&a, 1);                            // the canceled fallback finishes its frame on the decoder it owns
    mFechar(1);                               // ...and its fim closes hA: release, successor proceeds
    pthread_join(t, NULL);
    CHECK(s.ok && dec[2] == HARD, "successor gets the decoder after the fallback releases");
    CHECK(hwMax == 1, "never two hardware decoders (fallback + successor)");
  }
  // 2. Canceled fallback with no decoder yet, successor holding the lease: software only, no wait.
  zerar();
  { Job a = {0}, b = {0};
    CHECK(decPreparar(&b, 0, 2), "successor takes the lease");
    cancelar(&a);
    QUADRO(&a, 1);
    CHECK(dec[1] == SOFT && !ultimoHw, "canceled fallback is refused hardware (software decoder)");
    CHECK(!esperando, "lazy path never waits on the lease");
    CHECK(hwMax == 1, "one hardware decoder with a canceled fallback beside a successor");
  }
  // 3. Live fallback while another session holds the lease: no-lease behavior = software, no wait.
  zerar();
  { Job a = {0}, b = {0};
    decPreparar(&b, 0, 2);
    EXATO(&a, 3);
    CHECK(dec[3] == SOFT && !esperando && hwMax == 1, "live exact-frame path without the lease: software, no wait");
  }
  // 4. Lease taken, then the session made a software decoder (up to 1088p): lease released at once.
  zerar();
  { Job a = {0}, b = {0};
    altura[1] = 1080;
    QUADRO(&a, 1);
    CHECK(dec[1] == SOFT, "1080p fallback decodes in software");
    CHECK(decPreparar(&b, 0, 2) && !esperando, "a software decoder does not keep the lease");
  }
  // 5. Creation failure and JNI exception release the lease too.
  zerar();
  { Job a = {0}, b = {0};
    falhaCriar[1] = 1;
    QUADRO(&a, 1);
    falhaCriar[4] = 1; excecao = 1; EXATO(&a, 4);
    CHECK(decPreparar(&b, 0, 2) && !esperando, "failed creation / JNI exception release the lease");
  }
  // 6. Hardware already on the session: it keeps the lease across frames until mFechar.
  zerar();
  { Job a = {0}, b = {0};
    QUADRO(&a, 1); QUADRO(&a, 1); EXATO(&a, 1);
    CHECK(dec[1] == HARD && hwMax == 1, "hardware fallback keeps its decoder across frames");
    mFechar(1);
    CHECK(decPreparar(&b, 0, 2) && !esperando, "released at mFechar");
  }
  zerar();
  if (falhas) { printf("FAIL: %d lazy-decoder lease checks\n", falhas); return 1; }
  puts("PASS: lazy decoders follow the hardware-decoder lease (mixed fallback/successor, cancel, release)");
  return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="nuvio-lazy-lease-") as tmp:
    c = Path(tmp) / "lease.c"
    c.write_text(prefix + f"#define LEASED {int(leased)}\n" + fake + quadros + "\n" + lease + suffix)
    exe = Path(tmp) / "lease"
    subprocess.run(["cc", "-std=gnu11", "-Wall", "-Wextra", "-Wno-gnu-statement-expression", "-fsanitize=thread",
                    "-g", str(c), "-pthread", "-o", str(exe)], check=True)
    r = subprocess.run([str(exe)], env={**os.environ, "TSAN_OPTIONS": "halt_on_error=1"}, timeout=60)
    sys.exit(r.returncode)
