/* Cache contention must defer uploads; CPU mip allocation failure must not
 * move mip generation onto the render thread. Uses the real texture pump. */
#include "../src/sdlcompat.h"
#include "../src/gl_compat.h"
#include <assert.h>
#include <stdio.h>
#include <pthread.h>
static int gpuMips;
static void slowImage(GLenum target, GLint level, GLint internal, GLsizei w,
                      GLsizei h, GLint border, GLenum format, GLenum type,
                      const void *pixels) {
  SDL_Delay(250);
  nvPerfTexImage2D(target, level, internal, w, h, border, format, type, pixels);
}
#undef glTexImage2D
#define glTexImage2D slowImage
static void countMips(GLenum target) { (void)target; gpuMips++; }
#undef glGenerateMipmap
#define glGenerateMipmap countMips
#define NV_TEX_ASYNC_TEST 1
#ifndef NV_TEX_SOURCE
#define NV_TEX_SOURCE "../src/tex_cache.c"
#endif
#include NV_TEX_SOURCE
static SDL_sem *locked;
static void *busy(void *unused) {
  (void)unused;
  SDL_LockMutex(mtx);
  SDL_SemPost(locked);
  SDL_Delay(250);
  SDL_UnlockMutex(mtx);
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
  orcamento = 32L * 1024 * 1024;
  mipCpu = 1;
#ifndef NV_TEX_BASELINE
  iniciarEnvioAsync();
  assert(envioThread);
#endif
  SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 64, 64, 32,
                                                       SDL_PIXELFORMAT_ABGR8888);
  assert(surface);
  itens[0].estado = DECODIFICADO; itens[0].sup = surface; itens[0].card = 1;
  snprintf(itens[0].caminho, sizeof itens[0].caminho, "test-poster");
  pthread_t worker;
  assert(!pthread_create(&worker, NULL, busy, NULL));
  SDL_SemWait(locked);
  Uint32 start = SDL_GetTicks();
  assert(tex_bombear(1) == 0);
  Uint32 elapsed = SDL_GetTicks() - start;
  pthread_join(worker, NULL);
  printf("contended pump: %u ms (worker holds lock 250 ms)\n", elapsed);
  assert(elapsed < 50);
  assert(itens[0].sup == surface && itens[0].estado == DECODIFICADO);
  /* No userdata models an unsuccessful CPU mip allocation. */
  start = SDL_GetTicks();
  tex_bombear(1);
  elapsed = SDL_GetTicks() - start;
  printf("queued driver submission: %u ms\n", elapsed);
  assert(elapsed < 50);
#ifndef NV_TEX_BASELINE
  for (int i = 0; i < 200 && SDL_AtomicGet(&envioEstado) != 2; i++) SDL_Delay(5);
  assert(SDL_AtomicGet(&envioEstado) == 2);
  assert(!pthread_create(&worker, NULL, busy, NULL));
  SDL_SemWait(locked);
  start = SDL_GetTicks();
  assert(tex_bombear(1) == 0);
  assert(SDL_GetTicks() - start < 50);
  assert(itens[0].estado == ENVIANDO && itens[0].sup == surface);
  pthread_join(worker, NULL);
#endif
  for (int i = 0; i < 200 && itens[0].estado != PRONTO; i++) {
    SDL_Delay(5);
    tex_bombear(1);
  }
  assert(itens[0].estado == PRONTO);
  assert(gpuMips == 0 && itens[0].mip == 0);
#ifndef NV_TEX_BASELINE
  /* Above the old stripe threshold: completion still publishes only whole art. */
  surface = SDL_CreateRGBSurfaceWithFormat(0, 1024, 1024, 32,
                                           SDL_PIXELFORMAT_ABGR8888);
  assert(surface);
  itens[0].sup = surface; itens[0].estado = DECODIFICADO;
  start = SDL_GetTicks();
  tex_bombear(1);
  assert(SDL_GetTicks() - start < 50);
  assert(itens[0].estado == ENVIANDO && itens[0].w == 64);
  for (int i = 0; i < 200 && itens[0].estado != PRONTO; i++) {
    SDL_Delay(5); tex_bombear(1);
  }
  assert(itens[0].estado == PRONTO && itens[0].w == 1024);
  /* A budget-blocked hero must not starve a smaller queued poster. */
  nMax = 3;
  orcamento = bytesUsados + 64L * 64 * 4;
  quadroAtual = 2;
  itens[0].ultimoQuadro = quadroAtual;
  itens[1].sup = SDL_CreateRGBSurfaceWithFormat(0, 1024, 1024, 32,
                                               SDL_PIXELFORMAT_ABGR8888);
  itens[2].sup = SDL_CreateRGBSurfaceWithFormat(0, 32, 32, 32,
                                               SDL_PIXELFORMAT_ABGR8888);
  assert(itens[1].sup && itens[2].sup);
  itens[1].estado = itens[2].estado = DECODIFICADO;
  itens[1].urgente = 1;
  itens[2].ultimoQuadro = quadroAtual;
  tex_bombear(1);
  assert(itens[1].estado == DECODIFICADO && itens[1].sup);
  assert(itens[2].estado == ENVIANDO);
  for (int i = 0; i < 200 && itens[2].estado != PRONTO; i++) {
    SDL_Delay(5); tex_bombear(1);
  }
  assert(itens[2].estado == PRONTO && itens[0].estado == PRONTO);
  liberarSup(itens[1].sup); itens[1].sup = NULL;
  glDeleteTextures(1, &itens[2].tex);
  encerrarEnvioAsync();
  /* Startup failure (no worker) and worker MakeCurrent failure must both
   * defer new art instead of submitting pixels on the frame thread. */
  itens[1].sup = SDL_CreateRGBSurfaceWithFormat(0, 64, 64, 32,
                                               SDL_PIXELFORMAT_ABGR8888);
  assert(itens[1].sup);
  itens[1].estado = DECODIFICADO;
  orcamento = 32L * 1024 * 1024;
  start = SDL_GetTicks();
  assert(tex_bombear(1) == 0);
  assert(SDL_GetTicks() - start < 50);
  assert(itens[1].estado == DECODIFICADO && itens[1].sup);
  iniciarEnvioAsync();
  assert(envioThread);
  SDL_AtomicSet(&envioEstado, -2);
  start = SDL_GetTicks();
  assert(tex_bombear(1) == 0);
  assert(SDL_GetTicks() - start < 50);
  assert(itens[1].estado == DECODIFICADO && itens[1].sup);
  encerrarEnvioAsync();
  liberarSup(itens[1].sup); itens[1].sup = NULL;
#endif
  glDeleteTextures(1, &itens[0].tex);
  SDL_DestroySemaphore(locked); SDL_DestroyMutex(mtx);
  SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
  puts("PASS: no wait on contended cache; no render-thread CPU-mip fallback");
  return 0;
}
