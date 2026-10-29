// Sample the real island spring and menu renderer, including nested GL clips.
#include "gfx.h"
#include "ilha.h"
#include "ajustes_ux.h"
#include <SDL2/SDL_image.h>
#include <assert.h>
static void observeRequest(float w, float h, IlhaCorpo fn, void *u);
#define ilha_corpo observeRequest
#ifndef QUICKANIM_CENTRAL
#define QUICKANIM_CENTRAL "../src/central.c"
#endif
#include QUICKANIM_CENTRAL
#undef ilha_corpo

static unsigned char before[1920*1080*4], after[sizeof before];
static int failures, samples, escapes, early;
static float maxAlpha;
static void observeBody(GfxRect r, float a, void *u) {
  float p = anim_clamp(ilha_corpo_t(), 0, 1);
  samples++;
  if (a > maxAlpha) maxAlpha = a;
  if (a > p + .001f || (p <= .60f && a > .001f)) early++;
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, before);
  corpo(r, a, u);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, after);
  GfxRect panel = ilha_corpo_rect();
  float scale = gfx_escala();
  panel.x *= scale; panel.y *= scale; panel.w *= scale; panel.h *= scale;
  for (int y = 0; y < 1080; y++) for (int x = 0; x < 1920; x++) {
    // Allow one pixel for GL scissor rounding at fractional spring bounds.
    if (x >= floorf(panel.x)-1 && x < ceilf(panel.x+panel.w)+1 &&
        y >= floorf(panel.y)-1 && y < ceilf(panel.y+panel.h)+1) continue;
    int k = ((1079-y)*1920+x)*4;
    if (memcmp(before+k, after+k, 4)) {
      if (!escapes) printf("escape p=%.4f pixel=%d,%d panel=%.2f,%.2f,%.2f,%.2f\n", p, x, y, panel.x, panel.y, panel.w, panel.h);
      escapes++; return;
    }
  }
}
static void observeRequest(float w, float h, IlhaCorpo fn, void *u) {
  (void)fn;
  ilha_corpo(w, h, observeBody, u);
}
static void check(int ok, const char *name) {
  printf("%s: %s\n", ok ? "PASS" : "FAIL", name);
  failures += !ok;
}
int main(int argc, char **argv) {
  assert(argc == 2);
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *window = SDL_CreateWindow("Quick-menu animation", 0, 0, 64, 64,
                                        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(window && SDL_GL_CreateContext(window));
  GLuint texture, target;
  glGenTextures(1, &texture); glBindTexture(GL_TEXTURE_2D, texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &target); glBindFramebuffer(GL_FRAMEBUFFER, target);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar() && txt_iniciar("deploy/app", 1));
  tex_iniciar(64); gfx_icones_dir("deploy/app/art");
  ajustes_recursos("deploy/app/art");
  char path[900];
  snprintf(path, sizeof path, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
  FILE *config = fopen(path, "w"); assert(config);
  fprintf(config, "idioma 1\nvidroLocal 0\n"); fclose(config);
  ajustes_dir(getenv("NUVIO_DADOS"));
  central_teste_lista("qualidade\ndolbyVision\ndolbyAtmos\nescolherFonteManual\ntrailerAuto\ncorDestaque\nesmaecerLocal\nvidroLocal\n");
  SDL_Surface *strip = SDL_CreateRGBSurfaceWithFormat(0, 600*5, 540, 32, SDL_PIXELFORMAT_RGBA32);
  assert(strip);
  const int frames[] = {5, 12, 16, 22, 60};
  Uint32 now = 100000;
  int shot = 0;
  for (int frame = -60; frame <= 120; frame++) {
    if (frame == 0) { central_teste_foco(0, 0); centralPassoFoco(0); }
    if (frame == 61) central_fechar();
    now += 16;
    txt_novo_quadro(); tex_novo_quadro(); tex_bombear(8); gfx_novo_quadro();
    glBindFramebuffer(GL_FRAMEBUFFER, target); glViewport(0, 0, 1920, 1080);
    glClearColor(.025f, .025f, .03f, 1); glClear(GL_COLOR_BUFFER_BIT);
    central_atualizar(.016f, now);
    ilha_relogio_visivel(1); ilha_posicionar(1); ilha_desenhar(now);
    if (shot < 5 && frame == frames[shot]) {
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, after);
      for (int y = 0; y < 540; y++)
        memcpy((char *)strip->pixels + y*strip->pitch + shot*600*4,
               after + ((1079-y)*1920+1320)*4, 600*4);
      printf("frame=%d ms=%d panel_progress=%.4f\n", frame, (frame+1)*16, ilha_corpo_t());
      shot++;
    }
  }
  assert(IMG_SavePNG(strip, argv[1]) == 0); SDL_FreeSurface(strip);
  check(samples > 20, "sampled opening and closing callbacks");
  check(maxAlpha > .99f, "content completes reveal");
  check(early == 0, "content alpha <= panel progress; reveal begins after 60%");
  check(escapes == 0, "no content pixels outside animated panel bounds");
  check(ilha_corpo_t() == 0, "closing spring completes");
  return failures ? 1 : 0;
}
