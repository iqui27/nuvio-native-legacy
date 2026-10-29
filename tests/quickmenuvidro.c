// Exercise the real tile renderer and capture the complete quick-menu grid.
#include "gfx.h"
static int frostCalls;
static void focusFrost(GfxRect r, float radius, float alpha);
#define gfx_vidro_folha focusFrost
#include "../src/central.c"
#undef gfx_vidro_folha
#include "ajustes_ux.h"
#include <SDL2/SDL_image.h>
#include <assert.h>

static int failures;
static void focusFrost(GfxRect r, float radius, float alpha) {
  frostCalls++;
  assert(r.w == 144 && r.h == CC_BOTAO_H);
  assert(radius == CC_BOTAO_R / r.h && alpha > 0);
  gfx_vidro_folha(r, radius, alpha);
}
static void check(int ok, const char *name) {
  printf("%s: %s\n", ok ? "PASS" : "FAIL", name);
  failures += !ok;
}
static void save(const char *dir, int glass, const char *state) {
  char path[900];
  unsigned char *pixels = malloc(1920 * 1080 * 4);
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s && pixels);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  for (int y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pixels + (1079-y)*1920*4, 1920*4);
  snprintf(path, sizeof path, "%s/glass-%s-%s.png", dir, glass ? "on" : "off", state);
  assert(IMG_SavePNG(s, path) == 0);
  SDL_FreeSurface(s); free(pixels);
  printf("PNG %s\n", path);
}
int main(int argc, char **argv) {
  assert(argc == 2);
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *window = SDL_CreateWindow("Quick-menu frost", 0, 0, 64, 64,
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
  ajustes_recursos("deploy/app/art"); ajustes_ui_arte(21);
  char path[800];
  snprintf(path, sizeof path, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
  FILE *config = fopen(path, "w"); assert(config);
  fprintf(config, "idioma 0\nselected_theme 2\nvidroLocal 1\nvidroFoscoLocal 1\ndolbyVision 0\n");
  fclose(config); ajustes_dir(getenv("NUVIO_DADOS"));
  for (int glass = 0; glass <= 1; glass++) {
    ajustes_definir_vidro(glass);
    assert(ajustes_vidro() == glass);
    for (int state = 0; state < 3; state++) {
      const char *name = state == 0 ? "focused" : state == 1 ? "active-focused" : "edit-focused";
      central_teste_lista("qualidade\ndolbyVision\nescolherFonteManual\nvidroLocal\n");
      central_teste_foco(state == 2 ? -1 : state, 0);
      centralPassoFoco(0);
      for (int frame = 0; frame < 40; frame++) {
        glBindFramebuffer(GL_FRAMEBUFFER, target); glViewport(0, 0, 1920, 1080);
        txt_novo_quadro(); tex_novo_quadro(); tex_bombear(8); gfx_novo_quadro();
        glClearColor(.025f, .025f, .03f, 1); glClear(GL_COLOR_BUFFER_BIT);
        ajustes_ui_fundo();
        gfx_cor((GfxRect){0, 0, 1920, 1080}, 0, 0, 0, 0, .42f);
        GfxRect panel = {1320, 60, CC_W, altura(1)};
        if (glass) gfx_vidro_material(panel, 24 / panel.h, GFX_VIDRO_DENS, 1);
        else gfx_cor(panel, 24 / panel.h, .055f, .058f, .068f, 1);
        frostCalls = 0;
        corpo(panel, 1, NULL);
        TxtLinha clock = txt_linha(TXT_ILHA_HORA, "12:00", 243, 242, 239, 255);
        txt_desenhar_alpha(clock, panel.x + panel.w - clock.w - 20, panel.y + 15, 1);
        if (frame == 39) {
          check(frostCalls == 1, name);
          check(gfx_vidro_capturas == 0 && gfx_vidro_passadas == 0,
                "settled menu reuses blur cache");
          save(argv[1], glass, name);
        }
      }
    }
    // Explicit teal accent: settings migration can replace selected_theme.
    assert(ajustes_rapido_cor_definir(14));
    central_teste_foco(1, 0); centralPassoFoco(0);
    gfx_novo_quadro();
    glClearColor(.025f, .025f, .03f, 1); glClear(GL_COLOR_BUFFER_BIT);
    ajustes_ui_fundo();
    gfx_cor((GfxRect){0, 0, 1920, 1080}, 0, 0, 0, 0, .42f);
    GfxRect panel = {1320, 60, CC_W, altura(1)};
    if (glass) gfx_vidro_material(panel, 24 / panel.h, GFX_VIDRO_DENS, 1);
    else gfx_cor(panel, 24 / panel.h, .055f, .058f, .068f, 1);
    corpo(panel, 1, NULL);
    save(argv[1], glass, "teal-active-focused");
    // Also exercise active focus directly, independent of toggle defaults.
    GfxRect tile = {1340, 120, 144, CC_BOTAO_H};
    frostCalls = 0;
    botao(tile, "aj_sparkles", "Dolby Vision", "", 1, 1, 1, 1);
    check(frostCalls == 1, "active focused tile draws frost beneath accent");
    unsigned char pixel[4];
    glReadPixels(1440, 1079 - 165, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    check(pixel[1] > pixel[0] + 25 && pixel[2] > pixel[0] + 25,
          "focused active tile retains teal chroma");
    assert(ajustes_rapido_cor_definir(0));
    frostCalls = 0;
    botao(tile, "aj_sparkles", "Dolby Vision", "", 0, 0, 0, 1);
    check(frostCalls == 0, "unfocused tile adds no frost");
  }
  return failures ? 1 : 0;
}
