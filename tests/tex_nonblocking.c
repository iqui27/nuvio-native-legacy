/* Real pump: cache contention defers; missing CPU mips never use GPU fallback. */
#include "../src/sdlcompat.h"
#include "../src/gl_compat.h"
#include <assert.h>
#include <stdio.h>
#include <pthread.h>
static int gpuMips;
static void countMips(GLenum target) { (void)target; gpuMips++; }
#undef glGenerateMipmap
#define glGenerateMipmap countMips
#ifndef NV_TEX_SOURCE
#define NV_TEX_SOURCE "../src/tex_cache.c"
#endif
#define NV_TEX_MEMORY_TEST 1
#include NV_TEX_SOURCE
static SDL_sem *locked;
static void *busy(void *unused) {
  (void)unused;
  SDL_LockMutex(mtx); SDL_SemPost(locked);
  SDL_Delay(250); SDL_UnlockMutex(mtx);
  return NULL;
}
int main(void) {
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  SDL_Window *window = SDL_CreateWindow("texture regression", 0, 0, 32, 32,
                                       SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(window);
  SDL_GLContext context = SDL_GL_CreateContext(window);
  assert(context);
  mtx = SDL_CreateMutex(); locked = SDL_CreateSemaphore(0); nMax = 1;
  orcamento = 32L * 1024 * 1024; mipCpu = 1;
  SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 64, 64, 32,
                                                       SDL_PIXELFORMAT_ABGR8888);
  assert(surface);
  decodeReservado = 48L * 1024 * 1024;
  assert(!decodeCabe());
  decodeReservado = 0;
  assert(decodeCabe());
  itens[0].estado = DECODIFICADO; itens[0].sup = surface; itens[0].card = 1;
  snprintf(itens[0].caminho, sizeof itens[0].caminho, "test-poster");
  pthread_t worker;
  assert(!pthread_create(&worker, NULL, busy, NULL));
  SDL_SemWait(locked);
  Uint32 start = SDL_GetTicks();
  assert(tex_bombear(1) == 0);
  Uint32 elapsed = SDL_GetTicks() - start;
  pthread_join(worker, NULL);
  assert(elapsed < 50);
  assert(itens[0].sup == surface && itens[0].estado == DECODIFICADO);
  tex_bombear(1);
  assert(itens[0].estado == PRONTO);
  assert(gpuMips == 0 && itens[0].mip == 0);
  glDeleteTextures(1, &itens[0].tex);
  SDL_DestroySemaphore(locked); SDL_DestroyMutex(mtx);
  SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
  puts("PASS: nonblocking contended pump; CPU mip failure has no GPU fallback");
}
