// Real tab renderers over a black/white backdrop edge: frost must turn the
// edge into a ramp inside an unfocused tab. No network or user data.
#ifdef SHOT_LIBRARY
#include "../src/biblioteca.c"
#else
#include "../src/vertudo.c"
#endif
#include "dados.h"
#include <SDL2/SDL_image.h>
#include <assert.h>

static int failures;
static GLuint target;

static void check(int ok, const char *name) {
  printf("%s: %s\n", ok ? "PASS" : "FAIL", name);
  failures += !ok;
}
static double sample(int x, int y) {
  unsigned char p[4];
  glReadPixels(x, 1079 - y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, p);
  return (p[0] + p[1] + p[2]) / 3.0;
}
static void save(const char *dir, const char *screen, const char *mode) {
  char path[900];
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  unsigned char *pixels = malloc(1920 * 1080 * 4);
  assert(s && pixels);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  for (int y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pixels + (1079 - y) * 1920 * 4, 1920 * 4);
  snprintf(path, sizeof path, "%s/%s-%s.png", dir, screen, mode);
  assert(IMG_SavePNG(s, path) == 0);
  SDL_FreeSurface(s); free(pixels);
  printf("PNG %s\n", path);
}
int main(int argc, char **argv) {
  const char *dir = argc > 1 ? argv[1] : "/Volumes/ExternalSSD/tmp/vidrotabs/after";
  GLuint color;
  GfxRect track;
  float edge;
#ifdef SHOT_LIBRARY
  const char *screen = "library";
#else
  const char *screen = "catalog";
  static ColFolder folder;
  static ColSource sources[2];
#endif
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("tab frost", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w && SDL_GL_CreateContext(w));
  glGenTextures(1, &color); glBindTexture(GL_TEXTURE_2D, color);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &target); glBindFramebuffer(GL_FRAMEBUFFER, target);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar() && txt_iniciar("deploy/app", 1)); tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  dados_iniciar(getenv("NUVIO_DADOS"));
  dados_gravar("aparencia-20.txt", "1\n");
#ifdef SHOT_LIBRARY
  modo = MODO_SALVOS; animModo[0] = 1;
  contaModo[0] = 119; contaModo[1] = 98;
  track = (GfxRect){hdrX(), BIB_FAIXA_Y, larguraSeletor(), BIB_SEG_H};
  edge = track.x + BIB_SEG_PAD + larguraModo(0) + BIB_SEG_ITEM_GAP + larguraModo(1) * .5f;
#else
  folder.sources = sources; folder.nSources = 2;
  snprintf(folder.title, sizeof folder.title, "Netflix");
  snprintf(folder.group, sizeof folder.group, "Streaming");
  snprintf(sources[0].type, sizeof sources[0].type, "movie");
  snprintf(sources[1].type, sizeof sources[1].type, "series");
  collection = &folder; source = 0; tabFocus = 1; tabCursor = 0; tabAnim[0] = 1;
  track = (GfxRect){96, VT_AB_Y, 600, VT_AB_H};
  edge = track.x + abaTexto(i18n("Filmes"), 420) + 44 + 12 + (abaTexto(i18n("Séries"), 420) + 44) * .5f;
#endif
  for (int mode = 0; mode < 3; mode++) {
    char config[800];
    const char *name = mode == 0 ? "frost" : mode == 1 ? "clear" : "solid";
    snprintf(config, sizeof config, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
    FILE *f = fopen(config, "w"); assert(f);
    fprintf(f, "vidroLocal %d\nvidroFoscoLocal %d\n", mode == 2 ? 1 : 0, mode == 0);
    fclose(f); ajustes_dir(getenv("NUVIO_DADOS"));
#ifndef SHOT_LIBRARY
    ajustes_definir_abas_estilo(VT_AB_MENU);
    assert(cabN() == 3);
#endif
    assert(ajustes_vidro() == (mode != 2));
    assert(ajustes_vidro_fosco() == (mode == 0));
    for (int frame = 0; frame < 38; frame++) {
      glBindFramebuffer(GL_FRAMEBUFFER, target); glViewport(0, 0, 1920, 1080);
      txt_novo_quadro(); gfx_novo_quadro();
      if (frame == 37) gfx_vidro_sujar();
      glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
      gfx_cor((GfxRect){0, 0, 1920, 1080}, 0, 0, 0, 0, 1);
      gfx_recorte(edge, 0, 1920 - edge, 1080);
      gfx_cor((GfxRect){0, 0, 1920, 1080}, 0, .8f, .8f, .8f, 1);
      gfx_sem_recorte();
#ifdef SHOT_LIBRARY
      desenhaCabecalho(); desenhaModos();
#else
      char labels[COL_SOURCE_MAX][180] = {"Filmes", "Séries"};
      abasMenu(track.x, 1500, labels, 1);
#endif
      if (mode == 0 && frame == 37) {
        double v[4], mean = 0, variance = 0;
        int y = (int)track.y + 8;
        glFinish();
        for (int i = 0; i < 4; i++) { v[i] = sample((int)edge - 12 + 8 * i, y); mean += v[i] / 4; }
        for (int i = 0; i < 4; i++) variance += (v[i] - mean) * (v[i] - mean) / 4;
        printf("%s edge %.1f %.1f %.1f %.1f; variance %.2f; sources %d\n",
               screen, v[0], v[1], v[2], v[3], variance, gfx_vidro_com_fonte);
        check(gfx_vidro_com_fonte > 0 && variance > 1 && v[1] > v[0] + 1 && v[3] > v[2] + 1,
              "tab frost blurs backdrop edge into a varied ramp");
        printf("regional fill %.4f; captures %d; passes %d\n", gfx_vidro_fill, gfx_vidro_capturas, gfx_vidro_passadas);
      }
    }
    if (mode == 0) {
      // Force one refresh after regional bounds have settled.
      check(gfx_vidro_capturas == 1 && gfx_vidro_passadas <= 5 && gfx_vidro_fill > 0 && gfx_vidro_fill < .3,
            "tab frost stays within regional fill budget");
    }
    save(dir, screen, name);
  }
  return failures ? 1 : 0;
}
