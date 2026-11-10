// Bounded JPEG preparation queue. The worker owns copies, never Job/Tile pointers.
// Caller supplies SDL/SDL_image and pthread declarations before this header.
#ifndef NV_MINIATURA_PIXELS_H
#define NV_MINIATURA_PIXELS_H
#include <stdint.h>
#define MINI_PIX_READY 6
#define MINI_PIX_QUEUE 3
typedef struct { unsigned g; int k, n; long pts; uintptr_t identity; } MiniPixKey;
typedef struct { MiniPixKey key; unsigned char *jpg; unsigned epoch; } MiniPixRequest;
typedef struct { MiniPixKey key; SDL_Surface *surface; unsigned age; } MiniPixReady;
static pthread_mutex_t pixMu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t pixWake = PTHREAD_COND_INITIALIZER;
static MiniPixRequest pixQueue[MINI_PIX_QUEUE];
static MiniPixReady pixReady[MINI_PIX_READY];
static MiniPixKey pixBusy;
static int pixCount, pixRunning, pixWorking;
static unsigned pixEpoch, pixAge;

static int pixEqual(MiniPixKey a, MiniPixKey b) {
  return a.g == b.g && a.k == b.k && a.n == b.n && a.pts == b.pts && a.identity == b.identity;
}
// pixMu held; ownership of surface transfers here, including on replacement.
static void pixStore(MiniPixKey key, SDL_Surface *surface) {
  int slot = -1;
  for (int i = 0; i < MINI_PIX_READY; i++) {
    if (pixReady[i].surface && pixEqual(pixReady[i].key, key)) { slot = i; break; }
  }
  if (slot < 0) {
    slot = 0;
    for (int i = 0; i < MINI_PIX_READY; i++) {
      if (!pixReady[i].surface) { slot = i; break; }
      if (pixReady[i].age < pixReady[slot].age) slot = i;
    }
  }
  if (pixReady[slot].surface) SDL_FreeSurface(pixReady[slot].surface);
  pixReady[slot] = (MiniPixReady){ key, surface, ++pixAge };
}
static void *pixWorker(void *unused) {
  (void)unused;
  for (;;) {
    pthread_mutex_lock(&pixMu);
    while (!pixCount) pthread_cond_wait(&pixWake, &pixMu);
    MiniPixRequest request = pixQueue[0];
    memmove(pixQueue, pixQueue + 1, sizeof *pixQueue * (size_t)--pixCount);
    pixBusy = request.key; pixWorking = 1;
    pthread_mutex_unlock(&pixMu);
    SDL_RWops *rw = SDL_RWFromConstMem(request.jpg, request.key.n);
    SDL_Surface *decoded = rw ? IMG_Load_RW(rw, 1) : NULL;
    SDL_Surface *surface = decoded ? SDL_ConvertSurfaceFormat(decoded, SDL_PIXELFORMAT_ABGR8888, 0) : NULL;
    if (decoded) SDL_FreeSurface(decoded);
    free(request.jpg);
    pthread_mutex_lock(&pixMu);
    pixWorking = 0;
    if (surface && request.epoch == pixEpoch) pixStore(request.key, surface);
    else if (surface) SDL_FreeSurface(surface);
    pthread_mutex_unlock(&pixMu);
  }
  return NULL;
}
static void pixReset(void) {
  pthread_mutex_lock(&pixMu);
  pixEpoch++;
  for (int i = 0; i < pixCount; i++) free(pixQueue[i].jpg);
  pixCount = 0;
  for (int i = 0; i < MINI_PIX_READY; i++) {
    if (pixReady[i].surface) SDL_FreeSurface(pixReady[i].surface);
    pixReady[i].surface = NULL;
  }
  pthread_mutex_unlock(&pixMu);
}
static SDL_Surface *pixTake(MiniPixKey key) {
  SDL_Surface *surface = NULL;
  pthread_mutex_lock(&pixMu);
  for (int i = 0; i < MINI_PIX_READY; i++) if (pixReady[i].surface && pixEqual(pixReady[i].key, key)) {
    surface = pixReady[i].surface; pixReady[i].surface = NULL; break;
  }
  pthread_mutex_unlock(&pixMu);
  return surface;
}
static void pixRequest(MiniPixKey key, const unsigned char *jpg) {
  if (!jpg || key.n <= 0 || key.n > (1 << 20)) return;
  pthread_mutex_lock(&pixMu);
  if (pixWorking && pixEqual(pixBusy, key)) goto done;
  for (int i = 0; i < pixCount; i++) if (pixEqual(pixQueue[i].key, key)) goto done;
  for (int i = 0; i < MINI_PIX_READY; i++) if (pixReady[i].surface && pixEqual(pixReady[i].key, key)) goto done;
  if (!pixRunning) {
    pthread_t thread;
    if (pthread_create(&thread, NULL, pixWorker, NULL)) goto done;
    pthread_detach(thread); pixRunning = 1;
  }
  if (pixCount == MINI_PIX_QUEUE) goto done;
  unsigned char *copy = malloc((size_t)key.n);
  if (!copy) goto done;
  memcpy(copy, jpg, (size_t)key.n);
  pixQueue[pixCount++] = (MiniPixRequest){ key, copy, pixEpoch };
  pthread_cond_signal(&pixWake);
done:
  pthread_mutex_unlock(&pixMu);
}
#endif
