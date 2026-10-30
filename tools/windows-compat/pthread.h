/* Windows-only pthread vocabulary backed by SDL; no winpthreads DLL. */
#ifndef NV_WINDOWS_PTHREAD_H
#define NV_WINDOWS_PTHREAD_H
#include <SDL2/SDL.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
typedef uintptr_t pthread_t;
typedef SDL_mutex *pthread_mutex_t;
typedef SDL_cond *pthread_cond_t;
typedef struct { SDL_TLSID id; void (*destroy)(void *); } pthread_key_t;
typedef SDL_atomic_t pthread_once_t;
typedef struct { size_t stack; int detached; } pthread_attr_t;
typedef int pthread_mutexattr_t;
#define PTHREAD_MUTEX_INITIALIZER NULL
#define PTHREAD_ERRORCHECK_MUTEX_INITIALIZER NULL
#define PTHREAD_COND_INITIALIZER NULL
#define PTHREAD_ONCE_INIT {0}
#define PTHREAD_CREATE_DETACHED 1
#define PTHREAD_MUTEX_RECURSIVE 1
static inline SDL_mutex *nv_mutex(pthread_mutex_t *m) {
  SDL_mutex *p = SDL_AtomicGetPtr((void **)m);
  if (!p) {
    p = SDL_CreateMutex();
    if (!p) abort();
    if (!SDL_AtomicCASPtr((void **)m, NULL, p)) { SDL_DestroyMutex(p); p = SDL_AtomicGetPtr((void **)m); }
  }
  return p;
}
static inline SDL_cond *nv_cond(pthread_cond_t *c) {
  SDL_cond *p = SDL_AtomicGetPtr((void **)c);
  if (!p) {
    p = SDL_CreateCond();
    if (!p) abort();
    if (!SDL_AtomicCASPtr((void **)c, NULL, p)) { SDL_DestroyCond(p); p = SDL_AtomicGetPtr((void **)c); }
  }
  return p;
}
static inline int pthread_mutex_init(pthread_mutex_t *m, const pthread_mutexattr_t *a) { (void)a; *m = SDL_CreateMutex(); return *m ? 0 : ENOMEM; }
static inline int pthread_mutex_lock(pthread_mutex_t *m) { return SDL_LockMutex(nv_mutex(m)); }
static inline int pthread_mutex_trylock(pthread_mutex_t *m) { return SDL_TryLockMutex(nv_mutex(m)) == 0 ? 0 : EBUSY; }
static inline int pthread_mutex_unlock(pthread_mutex_t *m) { return SDL_UnlockMutex(*m); }
static inline int pthread_mutex_destroy(pthread_mutex_t *m) { SDL_DestroyMutex(*m); *m = NULL; return 0; }
static inline int pthread_mutexattr_init(pthread_mutexattr_t *a) { *a = 0; return 0; }
static inline int pthread_mutexattr_settype(pthread_mutexattr_t *a, int t) { *a = t; return 0; }
static inline int pthread_mutexattr_destroy(pthread_mutexattr_t *a) { (void)a; return 0; }
static inline int pthread_cond_init(pthread_cond_t *c, const void *a) { (void)a; *c = SDL_CreateCond(); return *c ? 0 : ENOMEM; }
static inline int pthread_cond_wait(pthread_cond_t *c, pthread_mutex_t *m) { return SDL_CondWait(nv_cond(c), nv_mutex(m)); }
static inline int pthread_cond_timedwait(pthread_cond_t *c, pthread_mutex_t *m, const struct timespec *t) {
  struct timespec now; timespec_get(&now, TIME_UTC);
  int64_t ms = (int64_t)(t->tv_sec - now.tv_sec) * 1000 + (t->tv_nsec - now.tv_nsec) / 1000000;
  int r = SDL_CondWaitTimeout(nv_cond(c), nv_mutex(m), ms < 0 ? 0 : ms > 0xfffffffe ? 0xfffffffe : (Uint32)ms);
  return r == SDL_MUTEX_TIMEDOUT ? ETIMEDOUT : r;
}
static inline int pthread_cond_signal(pthread_cond_t *c) { return SDL_CondSignal(nv_cond(c)); }
static inline int pthread_cond_broadcast(pthread_cond_t *c) { return SDL_CondBroadcast(nv_cond(c)); }
static inline int pthread_cond_destroy(pthread_cond_t *c) { SDL_DestroyCond(*c); *c = NULL; return 0; }
static inline int pthread_once(pthread_once_t *o, void (*fn)(void)) {
  if (SDL_AtomicCAS(o, 0, 1)) { fn(); SDL_AtomicSet(o, 2); }
  else while (SDL_AtomicGet(o) != 2) SDL_Delay(0);
  return 0;
}
typedef struct { void *(*fn)(void *); void *arg; } NvThreadStart;
static int nv_thread_start(void *p) { NvThreadStart s = *(NvThreadStart *)p; free(p); s.fn(s.arg); return 0; }
static inline int pthread_create(pthread_t *t, const pthread_attr_t *a, void *(*fn)(void *), void *arg) {
  NvThreadStart *s = malloc(sizeof *s); if (!s) return ENOMEM;
  s->fn = fn; s->arg = arg;
  SDL_Thread *thread = SDL_CreateThreadWithStackSize(nv_thread_start, "Nuvio", a ? a->stack : 0, s);
  if (!thread) { free(s); return EAGAIN; }
  *t = (uintptr_t)thread; if (a && a->detached) SDL_DetachThread(thread); return 0;
}
static inline int pthread_join(pthread_t t, void **r) { SDL_WaitThread((SDL_Thread *)t, NULL); if (r) *r = NULL; return 0; }
static inline int pthread_detach(pthread_t t) { SDL_DetachThread((SDL_Thread *)t); return 0; }
static inline pthread_t pthread_self(void) { return SDL_ThreadID(); }
static inline int pthread_equal(pthread_t a, pthread_t b) { return a == b; }
static inline int pthread_attr_init(pthread_attr_t *a) { a->stack = 0; a->detached = 0; return 0; }
static inline int pthread_attr_setstacksize(pthread_attr_t *a, size_t n) { a->stack = n; return 0; }
static inline int pthread_attr_setdetachstate(pthread_attr_t *a, int d) { a->detached = d; return 0; }
static inline int pthread_attr_destroy(pthread_attr_t *a) { (void)a; return 0; }
static inline int pthread_key_create(pthread_key_t *k, void (*d)(void *)) { k->destroy = d; k->id = SDL_TLSCreate(); return k->id ? 0 : ENOMEM; }
static inline void *pthread_getspecific(pthread_key_t k) { return SDL_TLSGet(k.id); }
static inline int pthread_setspecific(pthread_key_t k, const void *p) { return SDL_TLSSet(k.id, p, k.destroy); }
#endif
