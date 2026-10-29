#define NV_DESKTOP_RELEASE 1
#include "../src/desktop.h"
#ifndef _WIN32
/* Darwin's sys/types reserves pthread typedefs even without pthread.h. */
#define pthread_t nv_test_pthread_t
#define pthread_mutex_t nv_test_pthread_mutex_t
#define pthread_cond_t nv_test_pthread_cond_t
#define pthread_key_t nv_test_pthread_key_t
#define pthread_once_t nv_test_pthread_once_t
#define pthread_attr_t nv_test_pthread_attr_t
#define pthread_mutexattr_t nv_test_pthread_mutexattr_t
#endif
#include "../tools/windows-compat/pthread.h"
#include <assert.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ready = PTHREAD_COND_INITIALIZER;
static pthread_once_t once = PTHREAD_ONCE_INIT;
static pthread_key_t key;
static SDL_atomic_t destroyed;
static int count, initialized;
static void initialize(void) { initialized++; }
static void destroy(void *p) { free(p); SDL_AtomicAdd(&destroyed, 1); }
static void *worker(void *p) {
  (void)p;
  pthread_once(&once, initialize);
  int *value = malloc(sizeof *value);
  assert(value); *value = 42;
  assert(!pthread_setspecific(key, value));
  assert(*(int *)pthread_getspecific(key) == 42);
  for (int i = 0; i < 1000; i++) {
    pthread_mutex_lock(&lock); count++; pthread_mutex_unlock(&lock);
  }
  pthread_cond_signal(&ready);
  return NULL;
}
int main(void) {
  assert(!SDL_Init(SDL_INIT_VIDEO));
  SDL_Window *w = SDL_CreateWindow("desktop check", 0, 0, 640, 360, 0);
  assert(w);
  SDL_Event e = {0};
  e.type = SDL_CONTROLLERBUTTONDOWN; e.cbutton.button = SDL_CONTROLLER_BUTTON_A;
  assert(!desktop_event(&e, w, 0)); assert(e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_RETURN);
  e.type = SDL_CONTROLLERBUTTONUP; e.cbutton.button = SDL_CONTROLLER_BUTTON_B;
  assert(!desktop_event(&e, w, 0)); assert(e.type == SDL_KEYUP && e.key.keysym.sym == SDLK_AC_BACK);
  for (int edge = 0; edge < 2; edge++) {
    e.type = edge ? SDL_KEYUP : SDL_KEYDOWN; e.key.keysym.sym = SDLK_ESCAPE;
    assert(!desktop_event(&e, w, 0)); assert(e.key.keysym.sym == SDLK_AC_BACK);
  }
  e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_BACKSPACE;
  assert(!desktop_event(&e, w, 1)); assert(e.key.keysym.sym == SDLK_BACKSPACE);
  assert(!desktop_event(&e, w, 0)); assert(e.key.keysym.sym == SDLK_AC_BACK);
  e.key.keysym.sym = SDLK_f;
  assert(!desktop_event(&e, w, 1));
  assert(!desktop_event(&e, w, 0)); // Typing f must never enter fullscreen.
  e.key.keysym.sym = SDLK_F11;
  assert(desktop_event(&e, w, 0)); assert(SDL_GetWindowFlags(w) & SDL_WINDOW_FULLSCREEN_DESKTOP);
  e.key.keysym.sym = SDLK_f;
  e.key.keysym.mod = KMOD_GUI | KMOD_CTRL;
  assert(desktop_event(&e, w, 1)); assert(!(SDL_GetWindowFlags(w) & SDL_WINDOW_FULLSCREEN_DESKTOP));

  assert(!pthread_key_create(&key, destroy));
  pthread_t threads[8];
  for (int i = 0; i < 8; i++) assert(!pthread_create(&threads[i], NULL, worker, NULL));
  for (int i = 0; i < 8; i++) assert(!pthread_join(threads[i], NULL));
  assert(count == 8000 && initialized == 1 && SDL_AtomicGet(&destroyed) == 8);
  struct timespec until; timespec_get(&until, TIME_UTC);
  pthread_mutex_lock(&lock);
  assert(pthread_cond_timedwait(&ready, &lock, &until) == ETIMEDOUT);
  pthread_mutex_unlock(&lock);
  pthread_cond_destroy(&ready); pthread_mutex_destroy(&lock);
  SDL_DestroyWindow(w); SDL_Quit();
  puts("desktop: input, fullscreen, concurrent mutex/once, TLS cleanup, timeout OK");
  return 0;
}
