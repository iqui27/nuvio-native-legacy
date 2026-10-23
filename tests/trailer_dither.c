// Read the compositor's input alpha, not RGB blended over an opaque GL image.
#include "gfx.h"
#include "ajustes.h"
#include "layout.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 1920
#define H 1080
static unsigned char pixels[W * H * 4], previous[W * H * 4];

int main(void) {
  const char *dir = getenv("NUVIO_DADOS");
  char path[1024];
  GLuint tex, fbo;
  GfxRect rect = {0, 0, W, H};
  GfxModo modes[] = {GFX_HERO, GFX_HERO_CHEIO, GFX_DETALHE, GFX_VEU_TOPO, GFX_VEU_BAIXO};
  assert(dir);
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *window = SDL_CreateWindow("alpha dither", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(window && SDL_GL_CreateContext(window));
  glGenTextures(1, &tex); glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  gfx_tamanho_alvo(W, H); assert(gfx_iniciar());
  ajustes_dir(dir);
  snprintf(path, sizeof path, "%s/ajustes.txt", dir);
  FILE *file = fopen(path, "w"); assert(file);
  fprintf(file, "idioma 0\nsuavizarDegradesLocal 0\n"); fclose(file);
  ajustes_iniciar();
  int option = ajustes_rapido_op("suavizarDegradesLocal");
  assert(option >= 0);
  for (int setting = 0; setting < 3; setting++) {
    if (setting) assert(ajustes_rapido_passo(option, 1));
    assert(ajustes_suavizar_degrades() == (setting == 2 ? 0.0f : setting == 1 ? 2.0f : 1.0f));
    for (int light = 0; light < 2; light++) {
      gfx_definir_efeitos_leves(light);
      for (unsigned m = 0; m < sizeof modes / sizeof *modes; m++) {
        long changes = 0, bands = 0;
        for (int repeat = 0; repeat < 2; repeat++) {
          glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, W, H);
          gfx_novo_quadro(); gfx_furo(rect);
          gfx_rect(rect, 0, modes[m], 1, 1, 0, 0, 0, 0, 0, 1);
          glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
          assert(glGetError() == GL_NO_ERROR);
          if (!repeat) memcpy(previous, pixels, sizeof pixels);
          else assert(memcmp(previous, pixels, sizeof pixels) == 0);
        }
        // Right half excludes the hero's horizontal ramp. Every row here is
        // geometrically flat; only dither may vary alpha between neighbours.
        for (int y = 1; y < H - 1; y++) {
          int a = pixels[(y * W + W - 2) * 4 + 3];
          if (a < 8 || a > 247) continue;
          bands++;
          for (int x = W * 9 / 10; x < W - 2; x++)
            changes += pixels[(y * W + x) * 4 + 3] != pixels[(y * W + x + 1) * 4 + 3];
        }
        printf("setting=%d light=%d mode=%d bands=%ld adjacent alpha changes=%ld\n", setting, light, modes[m], bands, changes);
        fflush(stdout);
        assert(bands > 20);
        assert(setting == 2 ? changes == 0 : changes > bands * 10);
      }
    }
  }
  puts("trailer_dither: ok");
  return 0;
}
