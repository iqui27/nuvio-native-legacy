// Bold subtitle line (owner's style: Montserrat, 90 %, bold) through the real
// text path (txt_linha_corta_enfase + txt_desenhar_alpha) at 1920x1080:
//
//   bash tests/legenda_negrito_shot.sh <dir>
//
// Checks, and prints the numbers:
//   1. raster == screen: the line texture is exactly the quad drawn (1:1);
//   2. the drawn pixels equal a direct TTF_RenderUTF8_Blended of the Bold
//      face at the same pixel size (no resampling on the way to the screen);
//   3. the bold line uses the real Bold face, not TTF_STYLE_BOLD smeared on
//      the Regular bitmap (wider glyphs, hard edges: "pixelada, esticada").
#include "gfx.h"
#include "text.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nvtmp.h"

#define LW 1920
#define LH 1080
static GLuint fbo, fboTex;
static unsigned char quadro[LW * LH * 4];
static const char *S = "N\xc3\xa3o depois do que aconteceu no vale.";

static void ler(void) {
  glFinish(); glBindFramebuffer(GL_FRAMEBUFFER, fbo); glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, LW, LH, GL_RGBA, GL_UNSIGNED_BYTE, quadro);
}
// Brightness at screen (x, y), top-left origin.
static int px(int x, int y) { return quadro[((size_t)(LH - 1 - y) * LW + x) * 4]; }

static void salvar(const char *dir, const char *id) {
  SDL_Surface *s = SDL_CreateRGBSurface(0, LW, LH, 24, 0xff, 0xff00, 0xff0000, 0);
  char nome[700]; int x, y;
  for (y = 0; y < LH; y++) for (x = 0; x < LW; x++) {
    unsigned char *d = (unsigned char *)s->pixels + y * s->pitch + x * 3;
    const unsigned char *o = quadro + ((size_t)(LH - 1 - y) * LW + x) * 4;
    d[0] = o[0]; d[1] = o[1]; d[2] = o[2];
  }
  snprintf(nome, sizeof nome, "%s/%s.bmp", dir, id);
  assert(SDL_SaveBMP(s, nome) == 0);
  SDL_FreeSurface(s);
  fprintf(stderr, "captura: %s\n", nome);
}

// Share of edge pixels with partial coverage, over everything non-zero.
static double rampa(const unsigned char *a, int pitch, int bpp, int w, int h) {
  long sat = 0, mid = 0; int x, y;
  for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
    int v = a[y * pitch + x * bpp];
    if (v >= 250) sat++; else if (v > 5) mid++;
  }
  return 100.0 * mid / (double)(mid + sat);
}

int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : nvt_dir();
  int pctTam = 90, corpo = 20 + (pctTam - 50) / 10 * 4;   // TXT_LEG_90 = 36 px
  TxtEstilo est = (TxtEstilo)(TXT_LEG_50 + (pctTam - 50) / 10);
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("legenda negrito", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN); assert(w);
  SDL_GLContext gl = SDL_GL_CreateContext(w); assert(gl);
  glGenTextures(1, &fboTex); glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, LW, LH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, LW, LH); gfx_tamanho_alvo(LW, LH); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));

  txt_novo_quadro();
  TxtLinha l = txt_linha_corta_enfase(est, S, 255, 255, 255, 255, 1e9f, TXT_FAMILIA_MONTSERRAT, TXT_ENF_NEGRITO);
  assert(l.tex);
  GLint tw = 0, th = 0;
  glBindTexture(GL_TEXTURE_2D, l.tex);
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &tw);
  glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &th);
  gfx_tex_esquecer(0);

  // The references: Montserrat-Bold and Regular+TTF_STYLE_BOLD, same size.
  TTF_Font *fb = TTF_OpenFont("deploy/app/fonts/Montserrat-Bold.ttf", corpo);
  TTF_Font *fr = TTF_OpenFont("deploy/app/fonts/Montserrat-Regular.ttf", corpo);
  assert(fb && fr);
  SDL_Color branco = { 255, 255, 255, 255 };
  SDL_Surface *ref = SDL_ConvertSurfaceFormat(TTF_RenderUTF8_Blended(fb, S, branco), SDL_PIXELFORMAT_ABGR8888, 0);
  TTF_SetFontStyle(fr, TTF_STYLE_BOLD);
  SDL_Surface *sint = SDL_ConvertSurfaceFormat(TTF_RenderUTF8_Blended(fr, S, branco), SDL_PIXELFORMAT_ABGR8888, 0);
  assert(ref && sint);

  // Drawn where the player puts a centered line, on black.
  int x0 = (LW - l.w) / 2, y0 = 900;
  glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, LW, LH);
  glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
  txt_desenhar_alpha(l, (float)x0, (float)y0, 1.0f);
  ler();
  salvar(dir, "legenda-negrito-1080p");

  // Drawn pixels vs the direct rasterization of the Bold face.
  int x, y, difMax = 0; long difSoma = 0;
  for (y = 0; y < ref->h && y < l.h; y++) for (x = 0; x < ref->w && x < l.w; x++) {
    int a = ((unsigned char *)ref->pixels)[y * ref->pitch + x * 4 + 3];
    int d = abs(px(x0 + x, y0 + y) - a);
    if (d > difMax) difMax = d;
    difSoma += d;
  }
  { unsigned char *tela = malloc((size_t)l.w * l.h);
    for (y = 0; y < l.h; y++) for (x = 0; x < l.w; x++) tela[y * l.w + x] = (unsigned char)px(x0 + x, y0 + y);
    fprintf(stderr,
      "raster %dx%d -> tela %dx%d (quad)\n"
      "Bold real (TTF direto): %dx%d, borda parcial %.1f%%\n"
      "Regular+TTF_STYLE_BOLD:  %dx%d, borda parcial %.1f%%\n"
      "na tela:                 borda parcial %.1f%%, diferenca para o TTF direto max %d media %.3f\n",
      tw, th, l.w, l.h,
      ref->w, ref->h, rampa((unsigned char *)ref->pixels + 3, ref->pitch, 4, ref->w, ref->h),
      sint->w, sint->h, rampa((unsigned char *)sint->pixels + 3, sint->pitch, 4, sint->w, sint->h),
      rampa(tela, l.w, 1, l.w, l.h), difMax, (double)difSoma / (ref->w * ref->h));
    free(tela); }

  assert(tw == l.w && th == l.h);           // 1:1 at 1080p, no stretched texture
  assert(tw == ref->w && th == ref->h);     // the real Bold face, not the smear
  assert(sint->w > ref->w);                 // the smear really is wider
  assert(difMax <= 2);                      // no resampling between texture and screen
  printf("legenda_negrito_shot: ok\n");
  return 0;
}
