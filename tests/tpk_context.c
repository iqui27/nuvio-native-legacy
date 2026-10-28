/* Real texture pump with Samsung's forced header and a host EGL stub. */
#include "../src/sdlcompat.h"
#include <assert.h>
#include <stdio.h>

#define EGL_NO_CONTEXT NULL
typedef void *EGLContext;
static EGLContext current;
static EGLContext currentEgl(void) { return current; }
static struct { EGLContext (*GetCurrentContext)(void); } tpkEgl;
#define eglGetCurrentContext tpkEgl.GetCurrentContext
#include "tpk_context_bridge.inc"

/* SDL dummy cannot report the context owned by the Samsung host. */
SDL_GLContext (SDL_GL_GetCurrentContext)(void) { return NULL; }
#include "../src/tex_cache.c"

int main(void) {
  /* Use host GL for actual pixel uploads; keep the production context query. */
#undef SDL_CreateWindow
#undef SDL_GL_CreateContext
#undef SDL_GL_DeleteContext
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  SDL_Window *window = SDL_CreateWindow("TPK context", 0, 0, 32, 32,
                                       SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(window);
  SDL_GLContext context = SDL_GL_CreateContext(window);
  assert(context);
  assert((SDL_GL_GetCurrentContext)() == NULL);
  assert(SDL_GL_GetCurrentContext() == NULL); /* EGL not loaded. */
  tpkEgl.GetCurrentContext = currentEgl;
  assert(SDL_GL_GetCurrentContext() == NULL); /* EGL_NO_CONTEXT. */
  current = context;
  assert(SDL_GL_GetCurrentContext() == context);

  mtx = SDL_CreateMutex(); assert(mtx);
  nMax = 1; orcamento = 32L * 1024 * 1024;
  itens[0].estado = DECODIFICADO;
  itens[0].sup = SDL_CreateRGBSurfaceWithFormat(0, 32, 32, 32,
                                               SDL_PIXELFORMAT_ABGR8888);
  assert(itens[0].sup);
  snprintf(itens[0].caminho, sizeof itens[0].caminho, "tpk-context-proof");
  /* Bind exactly as tex_iniciar does, before testing lost/changed contexts. */
  uploadThread = SDL_ThreadID(); uploadContext = SDL_GL_GetCurrentContext();
  current = NULL;
  assert(tex_bombear(1) == 0 && itens[0].sup);
  current = (void *)1;
  assert(tex_bombear(1) == 0 && itens[0].sup);
  current = context;
  assert(tex_bombear(1) > 0);
  assert(itens[0].estado == PRONTO && itens[0].tex && tex_upl_n > 0);
  glDeleteTextures(1, &itens[0].tex);
  SDL_DestroyMutex(mtx);
  SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
  puts("PASS: SDL reports NULL; EGL bridge uploads textures; context guards hold");
}
