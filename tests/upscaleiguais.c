// Production Settings and GL passes: same 720p input, real drawable constraints.
#include "gfx.h"
#include "gpunivel.h"
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/ajustes.c"

static const char *names[] = {"automatic", "sgsr", "sgsr-sharp", "fsr1", "lanczos", "bilinear"};
enum { CROP = 384, BYTES = CROP * CROP * 4 };
static unsigned char pixels[6][BYTES];

static void settings(int active) {
  valor[AJ_RESOLUCAO] = RES_720;
  valor[AJ_AMPLIAR_UI] = 1;
  valor[AJ_AMPLIACAO_ALGO] = 2;
  valor[AJ_AVANCADAS] = 1;
  montarTela();
  int row = -1;
  for (int i = 0; i < AJ_N_TELA; i++)
    if (TELA[i].tipo == IT_OPC && TELA[i].op == AJ_AMPLIACAO_ALGO) row = i;
  assert(row >= 0 && visivel(row));
  assert(inativa(AJ_AMPLIACAO_ALGO) == !active);
  assert(inativa(AJ_AMPLIACAO_NITIDEZ) == !active);
  if (!active) assert(strstr(ajudaOpcaoBase(AJ_AMPLIACAO_ALGO), "Sem ampliação ativa"));
  valor[AJ_AMPLIAR_UI] = 0;
  assert(visivel(row) && inativa(AJ_AMPLIACAO_ALGO));
  assert(strstr(ajudaOpcaoBase(AJ_AMPLIACAO_ALGO), "Reinicie"));
  valor[AJ_AMPLIAR_UI] = 1;
  valor[AJ_RESOLUCAO] = RES_AUTO;
  assert(inativa(AJ_AMPLIACAO_ALGO));
  assert(strstr(ajudaOpcaoBase(AJ_AMPLIACAO_ALGO), "Automática ou 4K"));
  valor[AJ_RESOLUCAO] = RES_720;
}

static void render(int algo, int sharp, unsigned char *dst) {
  gpun_ampliacao(algo, sharp);
  gfx_novo_quadro();
  gpun_quadro_inicio();
  glClearColor(.15f, .19f, .23f, 1);
  glClear(GL_COLOR_BUFFER_BIT);
  glEnable(GL_SCISSOR_TEST);
  for (int y = 0; y < 720; y++) {
    glScissor(0, y, 1280, 1);
    glClearColor(.1f + y / 1100.f, .15f + y / 1400.f, .3f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    for (int x = 0; x < 1280; x += 11) {
      glScissor((x + y / 3) % 1280, y, 3, 1);
      glClearColor(.8f, .7f, .4f, 1);
      glClear(GL_COLOR_BUFFER_BIT);
    }
  }
  // Transparent video plane must survive every UI filter unchanged.
  glScissor(180, 180, 40, 40);
  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT);
  glDisable(GL_SCISSOR_TEST);
  gpun_quadro_fim();
  glReadPixels(400, 400, CROP, CROP, GL_RGBA, GL_UNSIGNED_BYTE, dst);
  assert(glGetError() == GL_NO_ERROR);
}

static void save(const char *out, int algo) {
  SDL_Surface *image = SDL_CreateRGBSurfaceWithFormat(0, CROP, CROP, 32, SDL_PIXELFORMAT_RGBA32);
  assert(image);
  for (int y = 0; y < CROP; y++)
    memcpy((char *)image->pixels + y * image->pitch,
           pixels[algo] + (CROP - y - 1) * CROP * 4, CROP * 4);
  char path[1024];
  snprintf(path, sizeof path, "%s/%s.png", out, names[algo]);
  assert(IMG_SavePNG(image, path) == 0);
  SDL_FreeSurface(image);
}

int main(int argc, char **argv) {
  assert(argc == 2);
  setenv("NUVIO_PANEL_SIZE", "3840x2160", 1);
  assert(SDL_Init(SDL_INIT_VIDEO) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
  SDL_Window *window = SDL_CreateWindow("Upscaling regression", 0, 0, 3840, 2160,
                                       SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(window);
  SDL_GLContext context = SDL_GL_CreateContext(window);
  assert(context);
  int width, height;
  SDL_GL_GetDrawableSize(window, &width, &height);
  assert(width == 3840 && height == 2160);
  gfx_tamanho_alvo(width, height);
  assert(gfx_iniciar());
  gpun_iniciar(width, height);
  gpun_forcar_720();
  gpun_ampliar_app(1);
  gpun_quadro_inicio();
  assert(gpun_ampliacao_ativa());
  settings(1);
  gpun_quadro_fim();
  for (int a = 0; a < 6; a++) {
    render(a, 3, pixels[a]);
    save(argv[1], a);
    int transparent = 0, opaque = 0;
    for (int p = 3; p < BYTES; p += 4) {
      transparent |= pixels[a][p] == 0;
      opaque |= pixels[a][p] == 255;
    }
    assert(transparent && opaque);
    for (int b = 0; b < a; b++) {
      assert(memcmp(pixels[a], pixels[b], BYTES));
      for (int p = 3; p < BYTES; p += 4) assert(pixels[a][p] == pixels[b][p]);
      printf("PASS 720p->4K %s differs from %s; alpha preserved\n", names[a], names[b]);
    }
  }
  unsigned char copy[BYTES];
  render(2, 0, copy);
  assert(!memcmp(copy, pixels[1], BYTES)); // No RCAS at zero sharpness.
  gpun_ampliar_app(0);
  assert(!gpun_ampliacao_ativa());
  render(0, 3, pixels[0]);
  for (int a = 1; a < 6; a++) {
    render(a, 3, copy);
    assert(!memcmp(copy, pixels[0], BYTES)); // TV scaling ignores algorithm.
  }
  settings(0);
  SDL_SetWindowSize(window, 1280, 720);
  SDL_GL_GetDrawableSize(window, &width, &height);
  assert(width == 1280 && height == 720);
  gpun_iniciar(width, height);
  gpun_forcar_720(); gpun_ampliar_app(1); gpun_quadro_inicio();
  assert(!gpun_ampliacao_ativa());
  settings(0); // Native 720p output has no internal upscale.
  gpun_quadro_fim();
  SDL_SetWindowSize(window, 1920, 1080);
  SDL_GL_GetDrawableSize(window, &width, &height);
  assert(width == 1920 && height == 1080);
  gpun_iniciar(width, height);
  gpun_ampliar_app(1); gpun_quadro_inicio();
  assert(!gpun_ampliacao_ativa());
  settings(0); // C9: 1080p UI on granted 1080p output.
  gpun_quadro_fim();
  gpun_forcar_720();
  render(0, 3, pixels[0]);
  assert(gpun_ampliacao_ativa());
  settings(1); // C9: 720p UI can upscale to granted 1080p output.
  render(1, 3, pixels[1]);
  assert(!memcmp(pixels[0], pixels[1], BYTES)); // Fractional Automatic = SGSR.
  for (int a = 2; a < 6; a++) {
    render(a, 3, pixels[a]);
    for (int b = 1; b < a; b++) assert(memcmp(pixels[a], pixels[b], BYTES));
  }
  puts("PASS output limits, disabled reasons, live app toggle, fractional Automatic, zero sharpness");
  gfx_encerrar();
  SDL_GL_DeleteContext(context);
  SDL_DestroyWindow(window); SDL_Quit();
  return 0;
}
