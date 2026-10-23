// CAPTURA DO SELO DE ASSISTIDO nos modos Icone e Escrito: fileira de posteres
// (200x300, 160x240, 120x180) e um cartao redondo. NUVIO_SHOT_SELO=0|2 (valor
// gravado de seloVistoLocal). Fora da suite (*_shot.sh): janela GL e olho humano.
#include "badges.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "ajustes.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-selo-visto.bmp";
  const char *m = getenv("NUVIO_SHOT_SELO");
  const char *dir = getenv("NUVIO_DADOS");
  SDL_Window *w;
  SDL_GLContext gl;
  int i;

  assert(dir && *dir);
  { char caminho[700]; FILE *f;
    snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);
    f = fopen(caminho, "w"); assert(f);
    fprintf(f, "idioma 0\nseloVistoLocal %s\n", m ? m : "0");
    fclose(f);
    ajustes_dir(dir); }
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: selo visto", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1920, 1080,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");

  for (i = 0; i < 60; i++) {
    float x = 120;
    static const float larg[3] = { 200, 160, 120 };
    int k;
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(6);
    glClearColor(0.025f, 0.025f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    gfx_novo_quadro();
    gfx_cor((GfxRect){ 0, 0, 1920, 1080 }, 0, .12f, .13f, .16f, 1);
    for (k = 0; k < 3; k++) {
      GfxRect c = { x, 300, larg[k], larg[k] * 1.5f };
      gfx_cor(c, 0.04f, .75f - k * .12f, .62f, .5f + k * .12f, 1);   // poster claro: o pior caso
      badges_selo_visto(c.x, c.y, c.w, 1.0f);
      x += larg[k] + 60;
    }
    gfx_cor((GfxRect){ x, 300, 180, 180 }, 0.5f, .45f, .6f, .75f, 1);   // cartao redondo
    badges_check_cartao((GfxRect){ x, 300, 180, 180 }, 1, 1.0f);
    if (i == 59) {
      unsigned char *pix = malloc(1920 * 1080 * 4);
      SDL_Surface *s;
      int y;
      assert(pix);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      assert(s);
      for (y = 0; y < 1080; y++)
        memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
      assert(SDL_SaveBMP(s, saida) == 0);
      SDL_FreeSurface(s);
      free(pix);
    }
    SDL_GL_SwapWindow(w);
  }
  printf("captura: %s\n", saida);
  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  return 0;
}
