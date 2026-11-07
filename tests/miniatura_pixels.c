#define _DEFAULT_SOURCE
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// SDL is mocked so the real worker/ownership code can run under host sanitizers.
typedef struct { int pixel; } SDL_Surface;
typedef struct { const unsigned char *bytes; } SDL_RWops;
#define SDL_PIXELFORMAT_ABGR8888 1
static pthread_t uiThread;
static pthread_mutex_t decodeMu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t decodeWake = PTHREAD_COND_INITIALIZER;
static int blocked = 1, entered;
static SDL_RWops *SDL_RWFromConstMem(const void *bytes, int n) {
  assert(n > 0); SDL_RWops *rw = malloc(sizeof *rw); rw->bytes = bytes; return rw;
}
static SDL_Surface *IMG_Load_RW(SDL_RWops *rw, int freeRw) {
  assert(!pthread_equal(uiThread, pthread_self()));
  pthread_mutex_lock(&decodeMu); entered = 1; pthread_cond_broadcast(&decodeWake);
  while (blocked) pthread_cond_wait(&decodeWake, &decodeMu);
  pthread_mutex_unlock(&decodeMu);
  SDL_Surface *surface = malloc(sizeof *surface); surface->pixel = rw->bytes[0];
  if (freeRw) free(rw);
  return surface;
}
static SDL_Surface *SDL_ConvertSurfaceFormat(SDL_Surface *s, int fmt, int flags) {
  (void)fmt; (void)flags; SDL_Surface *copy = malloc(sizeof *copy); *copy = *s; return copy;
}
static void SDL_FreeSurface(SDL_Surface *s) { free(s); }
#include "../src/miniatura_pixels.h"

static SDL_Surface *waitFor(MiniPixKey key) {
  for (int i = 0; i < 2000; i++) { SDL_Surface *s = pixTake(key); if (s) return s; usleep(1000); }
  assert(!"worker timed out"); return NULL;
}
int main(void) {
  uiThread = pthread_self();
  unsigned char jpeg[] = { 42, 0, 0 };
  MiniPixKey key = { 1, 7, sizeof jpeg, 60000, 123 };
  pixRequest(key, jpeg);
  pthread_mutex_lock(&decodeMu);
  while (!entered) pthread_cond_wait(&decodeWake, &decodeMu);
  pthread_mutex_unlock(&decodeMu);
  // UI requests return while decode is blocked. Worker owns a copy of the JPEG.
  assert(!pixTake(key)); jpeg[0] = 99;
  pixRequest(key, jpeg); // duplicate in-flight request does not consume queue capacity
  pthread_mutex_lock(&pixMu); assert(!pixCount); pthread_mutex_unlock(&pixMu);
  for (int i = 0; i < 20; i++) { MiniPixKey next = key; next.k += i + 1; pixRequest(next, jpeg); }
  pthread_mutex_lock(&pixMu); assert(pixCount == MINI_PIX_QUEUE); pthread_mutex_unlock(&pixMu);
  // Closing/switching clears queued data; the old in-flight result must not reappear.
  pixReset();
  pthread_mutex_lock(&decodeMu); blocked = 0; pthread_cond_broadcast(&decodeWake); pthread_mutex_unlock(&decodeMu);
  for (int i = 0; i < 2000; i++) {
    pthread_mutex_lock(&pixMu); int working = pixWorking; pthread_mutex_unlock(&pixMu);
    if (!working) break;
    usleep(1000);
  }
  assert(!pixTake(key));
  key.g++; jpeg[0] = 25; pixRequest(key, jpeg); jpeg[0] = 99;
  SDL_Surface *surface = waitFor(key); assert(surface->pixel == 25); SDL_FreeSurface(surface);
  // Changed timestamp/identity cannot consume old pixels even at the same slot.
  pthread_mutex_lock(&pixMu);
  surface = malloc(sizeof *surface); surface->pixel = 12; pixStore(key, surface);
  pthread_mutex_unlock(&pixMu);
  MiniPixKey changed = key; changed.pts++;
  assert(!pixTake(changed));
  changed = key; changed.identity++; assert(!pixTake(changed));
  surface = pixTake(key); assert(surface && surface->pixel == 12); SDL_FreeSurface(surface);
  pixReset();
  puts("PASS: background decode, bounded queue, copied input, cancellation and pixel identity");
}
