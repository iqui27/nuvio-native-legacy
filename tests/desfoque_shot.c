// Fundo desfocado da abertura Cinema (gfx_desfocado): a copia 96x54 da arte
// e medida e a tela cheia e gravada, para comparar antes e depois do fix.
//  - Mede a copia pixel a pixel (gfx_desfocado_px): luminancia media, variancia,
//    gradiente medio (quanto menor, mais liso) e o residuo passa-alta RMS (manchas
//    e listras: a parte da imagem que o desfoque deveria ter apagado).
//  - Grava a tela cheia como o Cinema desenha (GFX_SNAP, B-spline) em
//    NUVIO_DESFOQUE_SAIDA (prefixo, padrao /tmp/nuvio-desfoque).
// Precisa de janela GL. Uso: desfoque_shot [rotulo]
#include "gfx.h"
#include "nvtmp.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#endif

#define W 96
#define H 54

static void grava(const char *nome) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s;
  int y;
  assert(pix);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < 1080; y++) memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(IMG_SavePNG(s, nome) == 0);
  SDL_FreeSurface(s);
  free(pix);
  printf("captura: %s\n", nome);
}

// Arte de 480x270 (a largura que o Cinema pede a tex_obter_larg), de um JPG.
static GLuint arteTex(const char *caminho) {
  SDL_Surface *s = IMG_Load(caminho), *t, *r;
  GLuint tex = 0;
  assert(s);
  t = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ABGR8888, 0);
  SDL_FreeSurface(s);
  assert(t);
  r = SDL_CreateRGBSurfaceWithFormat(0, 480, 270, 32, SDL_PIXELFORMAT_ABGR8888);
  assert(r);
  SDL_SetSurfaceBlendMode(t, SDL_BLENDMODE_NONE);
  SDL_BlitScaled(t, NULL, r, NULL);
  SDL_FreeSurface(t);
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 480, 270, 0, GL_RGBA, GL_UNSIGNED_BYTE, r->pixels);
  SDL_FreeSurface(r);
  return tex;
}

static void mede(GLuint copia, const char *rotulo) {
  static float L[H][W];
  double soma = 0, soma2 = 0, grad = 0, hp = 0;
  int x, y, n = 0, nhp = 0;
  for (y = 0; y < H; y++)
    for (x = 0; x < W; x++) {
      unsigned char c[3];
      float u = (x + 0.5f) / W, v = (y + 0.5f) / H;
      assert(gfx_desfocado_px(copia, u, v, c));
      L[y][x] = 0.299f * c[0] + 0.587f * c[1] + 0.114f * c[2];
      soma += L[y][x];
      soma2 += L[y][x] * L[y][x];
      n++;
    }
  for (y = 0; y < H; y++)
    for (x = 0; x < W; x++) {
      if (x + 1 < W) grad += fabs(L[y][x + 1] - L[y][x]);
      if (y + 1 < H) grad += fabs(L[y + 1][x] - L[y][x]);
      if (x > 0 && x + 1 < W && y > 0 && y + 1 < H) {
        float m = 0;
        int dx, dy;
        for (dy = -1; dy <= 1; dy++)
          for (dx = -1; dx <= 1; dx++) m += L[y + dy][x + dx];
        m /= 9.0f;
        hp += (L[y][x] - m) * (L[y][x] - m);
        nhp++;
      }
    }
  {
    double media = soma / n;
    printf("%s: lum=%.1f var=%.1f grad=%.2f hp_rms=%.2f\n", rotulo, media,
           soma2 / n - media * media, grad / (2.0 * n), sqrt(hp / nhp));
  }
}

int main(int argc, char **argv) {
  const char *rotulo = argc > 1 ? argv[1] : "atual";
  const char *arte = getenv("NUVIO_DESFOQUE_ARTE") ? getenv("NUVIO_DESFOQUE_ARTE") : "deploy/app/art/00.jpg";
  const char *saida = getenv("NUVIO_DESFOQUE_SAIDA") ? getenv("NUVIO_DESFOQUE_SAIDA") : nvt("/nuvio-desfoque");
  char nome[700];
  SDL_Window *w;
  SDL_GLContext gl;
  GLuint src, copia;
  GfxRect tela = { 0, 0, 1920, 1080 };

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("desfoque", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1920, 1080,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());

  src = arteTex(arte);
  copia = gfx_desfocado(src, arte);
  assert(copia);
  mede(copia, rotulo);

  // Tela cheia como o Cinema: copia esticada, B-spline, 16:9 sem recorte.
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  gfx_tex_aspect_atual = 16.0f / 9.0f;
  gfx_rect(tela, copia, GFX_SNAP, 0.0f, 2.0f, 0.0f, 0.0f, 0, 0, 0, 1.0f);
  gfx_tex_aspect_atual = 0.0f;
  snprintf(nome, sizeof nome, "%s-%s.png", saida, rotulo);
  grava(nome);
  SDL_GL_SwapWindow(w);

  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  return 0;
}
