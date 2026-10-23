// Relogio folgado: a regra SOLO deve valer mesmo numa GPU rapida.
#define NV_TEX_UPLOAD_BUDGET_MS 100.0
#ifdef NV_PLX_BEFORE
#include "/Volumes/ExternalSSD/tmp/jobtmp/plx-before-tex.c"
#else
#include "../src/tex_cache.c"
#endif
#include <assert.h>
#ifdef NV_PLX_BEFORE
int tex_lookahead_parado(TexLookahead *s, float y) { (void)s; (void)y; return 0; }
int tex_lookahead_pedir(const char *p, float w) { (void)p; (void)w; return 0; }
#endif

static void item(int i, int w, int h) {
  snprintf(itens[i].caminho, sizeof itens[i].caminho, "local-%d", i);
  itens[i].hash = hashCaminho(itens[i].caminho);
  itens[i].estado = DECODIFICADO;
  itens[i].sup = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ABGR8888);
  assert(itens[i].sup);
  SDL_FillRect(itens[i].sup, NULL, 0xff3579ab);
}
int main(void) {
  assert(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER));
  SDL_Window *w = SDL_CreateWindow("plx-arte", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w); SDL_GLContext ctx = SDL_GL_CreateContext(w); assert(ctx);
  assert(gfx_iniciar());
  mtx = SDL_CreateMutex(); cond = SDL_CreateCond(); condDec = SDL_CreateCond();
  nMax = 16; orcamento = 512L * 1024 * 1024;
  item(0, 1280, 720); item(1, 160, 240); item(2, 160, 240);
  glViewport(3, 4, 48, 50);
  glClearColor(0.2f, 0.3f, 0.4f, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  unsigned char antes[4], depois[4];
  glReadPixels(10, 10, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, antes);
  tex_upl_n = 0;
  tex_bombear(3);
  printf("SOLO: uploads/frame before=3, observed=%d (expected <=1)\n", tex_upl_n);
  fflush(stdout);
  assert(itens[1].estado == DECODIFICADO && itens[2].estado == DECODIFICADO);
  while (uplIdx >= 0) tex_bombear(3);
  assert(itens[0].estado == PRONTO);
  unsigned char *pixels = malloc(1280 * 720 * 4); assert(pixels);
  glBindTexture(GL_TEXTURE_2D, itens[0].tex);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  assert(pixels[0] == 0xab && pixels[1] == 0x79 && pixels[2] == 0x35);
  SDL_Surface *shot = SDL_CreateRGBSurfaceWithFormatFrom(pixels, 1280, 720, 32,
                           1280 * 4, SDL_PIXELFORMAT_ABGR8888);
  assert(shot && !IMG_SavePNG(shot, "/Volumes/ExternalSSD/tmp/jobtmp/plx-arte-upload.png"));
  SDL_FreeSurface(shot); free(pixels); gfx_tex_esquecer(0);
  tex_bombear(3);
  assert(itens[1].estado == PRONTO);
  assert(glGetError() == GL_NO_ERROR);
  GLint vp[4], fbo;
  glGetIntegerv(GL_VIEWPORT, vp); glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
  assert(vp[0] == 3 && vp[1] == 4 && vp[2] == 48 && vp[3] == 50 && fbo == 0);
  glReadPixels(10, 10, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, depois);
  assert(!memcmp(antes, depois, 4));
#ifndef NV_PLX_BEFORE
  TexLookahead ahead = {0};
  assert(!tex_lookahead_parado(&ahead, 0));
  tex_novo_quadro(); assert(!tex_lookahead_parado(&ahead, 0));
  tex_novo_quadro(); assert(tex_lookahead_parado(&ahead, 0));
  assert(!tex_lookahead_parado(&ahead, 20) && ahead.direcao == 1);
  assert(!tex_lookahead_parado(&ahead, 10) && ahead.direcao == -1);
  while (emVoo()) tex_bombear(3);
  assert(!tex_lookahead_pedir(itens[0].caminho, 160));
  assert(tex_lookahead_pedir("https://x/ahead.jpg", 160));
  int a = acharIndice("https://x/ahead.jpg", hashCaminho("https://x/ahead.jpg"));
  assert(a >= 0 && itens[a].lookahead && !quente(&itens[a]));
  assert(!tex_lookahead_pedir("https://x/second.jpg", 160));
  tex_novo_quadro(); assert(!tex_lookahead_pedir("https://x/second.jpg", 160));
  puts("lookahead: 1 request/frame, idle only, two settled frames, not hot");
#endif
  gfx_encerrar(); SDL_GL_DeleteContext(ctx); SDL_DestroyWindow(w);
  puts("plx_arte: PASS");
  return 0;
}
