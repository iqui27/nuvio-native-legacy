// GRADE DO VIDRO PADRAO: cada familia de componente na parte de baixo desenhada
// pelo helper compartilhado (gfx_vidro_material / gfx_vidro_ou_solido) e, em
// cima, o desenho antigo (retangulo escuro proprio), os dois sobre a mesma arte.
// Compile com -DNV_VIDRO_TESTE. Nao entra na suite: precisa de janela GL.
//
//   bash tests/vidro_padrao_shot.sh /Volumes/ExternalSSD/tmp/rev/vidropadrao/grade.bmp
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "ajustes.h"
#include "layout.h"
#include "nvtmp.h"
#include "shot_arte.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// -DNV_VIDRO_TESTE compila fora estes tres acessores de ajustes.c.
int ajustes_fundo(void) { return 0; }
int ajustes_vidro_contorno(void) { return 1; }
int ajustes_addons_do_principal(void) { return 0; }

typedef struct { const char *nome; float w, h, raioPx; int modal, pilula; float sr, sg, sb, sa; } Tile;
static const Tile T[] = {
  { "Dialog / popup",    300, 190, 28, 1, 0, 0.055f, 0.058f, 0.068f, 0.94f },
  { "Novidades card",    300, 190, 32, 1, 0, 0.050f, 0.053f, 0.062f, 0.96f },
  { "Avisos panel",      300, 190, 28, 1, 0, 0.055f, 0.058f, 0.068f, 0.94f },
  { "Island piece",      300, 120, 28, 0, 0, 0.070f, 0.073f, 0.082f, 0.90f },
  { "Toast pill",        300,  60, 30, 0, 1, 0.050f, 0.055f, 0.067f, 0.80f },
  { "Hint / tooltip",    240,  44, 22, 0, 1, 0.130f, 0.140f, 0.180f, 0.97f },
  { "Friends pill",      260,  44, 22, 0, 1, 0.030f, 0.035f, 0.045f, 0.70f },
  { "Side strip",         96, 190,  0, 0, 0, 0.039f, 0.043f, 0.055f, 0.86f },
};
#define NT ((int)(sizeof T / sizeof T[0]))

static void linha(const char *s, float x, float y) {
  TxtLinha l = txt_linha(TXT_CAPTION, s, 240, 240, 244, 255);
  txt_desenhar_alpha(l, x, y, 1.0f);
}
static void fileira(float y0, int novo, const char *titulo) {
  float x = 60.0f, y = y0;
  int i;
  linha(titulo, 60.0f, y0 - 40.0f);
  for (i = 0; i < NT; i++) {
    GfxRect r = { x, y, T[i].w, T[i].h };
    if (x + T[i].w > 1860.0f) { x = 60.0f; y += 220.0f; r.x = x; r.y = y; }
    if (novo) {
      if (T[i].pilula) gfx_vidro_ou_solido(r, 0.5f, GFX_VIDRO_DENS, 1.0f, T[i].sr, T[i].sg, T[i].sb, T[i].sa);
      else gfx_vidro_ou_solido(r, T[i].raioPx / r.h, T[i].modal ? GFX_VIDRO_DENS_MODAL : GFX_VIDRO_DENS, 1.0f,
                               T[i].sr, T[i].sg, T[i].sb, T[i].sa);
    } else {
      gfx_cor(r, T[i].pilula ? 0.5f : T[i].raioPx / r.h, T[i].sr, T[i].sg, T[i].sb, T[i].sa);
    }
    linha(T[i].nome, r.x + 16.0f, r.y + (r.h - 20.0f) * 0.5f);
    x += T[i].w + 40.0f;
  }
}
int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nuvio-vidro-padrao.bmp");
  SDL_Window *w; SDL_GLContext gl; int i;
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("vidro padrao", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1920, 1080,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(w);
  gl = SDL_GL_CreateContext(w); assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  for (i = 0; i < 8; i++) {
    txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6);
    glClearColor(0.05f, 0.05f, 0.06f, 1.0f); glClear(GL_COLOR_BUFFER_BIT);
    shot_arte_desenhar(.30f);
    fileira(120.0f, 0, "BEFORE: own dark gfx_cor rect");
    fileira(620.0f, 1, "AFTER: shared glass helper (backdrop shared, flat when nested)");
    if (i == 7) {
      unsigned char *pix = malloc(1920 * 1080 * 4);
      SDL_Surface *s; int y;
      assert(pix);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32); assert(s);
      for (y = 0; y < 1080; y++) memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
      assert(SDL_SaveBMP(s, saida) == 0);
      SDL_FreeSurface(s); free(pix);
    }
    SDL_GL_SwapWindow(w);
  }
  tex_encerrar(); txt_encerrar(); gfx_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(w); SDL_Quit();
  printf("PASS: %s\n", saida);
  return 0;
}
