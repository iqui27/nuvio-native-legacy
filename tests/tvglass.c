// Real sidebar draws, settings reloads and GL counters; no network or app data.
#if defined(NV_TVGLASS_LIBRARY)
#include "../src/biblioteca.c"
void tvglass_library(void) {
  modo = MODO_LISTAS;
  animOrig = 1;
  desenhaOrigens();
}
#elif defined(NV_TVGLASS_GUIDE)
#include "../src/guia.c"
void tvglass_guide(void) {
  static GCat category;
  static int visible, count = 12;
  catN = &count;
  cats = &category; nCats = 1;
  category.nome = "Sports";
  vis = &visible; nVis = 1;
  catAberto = 1; catAnim = 1;
  desenharPainelCategorias(1);
}
#else
#include "../src/menu.c"
#include "dados.h"
#include "tex_cache.h"
#include "gl_compat.h"
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <sys/stat.h>

void tvglass_library(void);
void tvglass_guide(void);
static int failures;
static GLuint backdrop;

static void settings(int layout, int mode) {
  char path[1024];
  snprintf(path, sizeof path, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
  FILE *f = fopen(path, "w"); assert(f);
  fprintf(f, "idioma 1\nidiomaAutoLocal 0\nhomeLayoutLocal %d\nmodernSidebar 0\n"
             "menuTopoLocal 0\ncollapseSidebar 1\nvidroLocal %d\n"
             "vidroFoscoLocal %d\nvidroQualidadeLocal 2\nvidroOpacLocal 2\n"
             "vidroOpacVersaoLocal 1\nperformanceAutoVersaoLocal 1\n", layout, mode == 2, mode == 1);
  fclose(f);
  ajustes_dir(getenv("NUVIO_DADOS"));
  assert(ajustes_vidro() == (mode != 2));
  assert(ajustes_vidro_fosco() == (mode == 1));
}

static void frame(int surface) {
  txt_novo_quadro(); tex_novo_quadro(); tex_bombear(16); gfx_novo_quadro();
  glClearColor(.1f, .1f, .1f, 1); glClear(GL_COLOR_BUFFER_BIT);
  gfx_rect((GfxRect){0, 0, 1920, 1080}, backdrop, GFX_SNAP, 0, 0, 0, 0, 1, 1, 1, 1);
  if (surface == 1) tvglass_library();
  else if (surface == 2) tvglass_guide();
  else {
    menu_atualizar(1.0f / 60, SDL_GetTicks());
    menu_desenhar(SDL_GetTicks());
  }
  glFinish();
}

static void capture(const char *dir, const char *name) {
  char path[1024];
  unsigned char *pixels = malloc(1920 * 1080 * 4); assert(pixels);
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32); assert(s);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  for (int y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pixels + (1079-y)*1920*4, 1920*4);
  snprintf(path, sizeof path, "%s/%s.png", dir, name);
  assert(IMG_SavePNG(s, path) == 0);
  SDL_FreeSurface(s); free(pixels);
}

static void modes(const char *out, const char *label, int layout, int surface) {
  const int sequence[] = {1, 0, 2, 1, 2, 0};
  const char *names[] = {"glass", "frost", "off"};
  for (int i = 0; i < 6; i++) {
    int mode = sequence[i];
    settings(layout, mode); // Keep sidebar open and initialized throughout switches.
    frame(surface);
    int glass = gfx_vidro_chapados + gfx_vidro_com_fonte;
    int ok = mode == 2 ? glass == 0 : mode == 1 ? gfx_vidro_com_fonte > 0 :
             gfx_vidro_chapados > 0 && gfx_vidro_com_fonte == 0;
    printf("%s %s %s: flat=%d frost=%d\n", ok ? "PASS" : "FAIL", label,
           names[mode], gfx_vidro_chapados, gfx_vidro_com_fonte);
    if (!ok) failures++;
    if (i < 3) {
      char name[128]; snprintf(name, sizeof name, "%s-%s", label, names[mode]);
      capture(out, name);
    }
  }
}

int main(int argc, char **argv) {
  setvbuf(stdout, NULL, _IOLBF, 0);
  assert(argc == 2 && getenv("NUVIO_DADOS"));
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *win = SDL_CreateWindow("Sidebar material regression", 0, 0, 1920, 1080,
                                    SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(win && SDL_GL_CreateContext(win));
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar() && txt_iniciar("deploy/app", 1));
  dados_iniciar(getenv("NUVIO_DADOS")); tex_iniciar(64); gfx_icones_dir("deploy/app/art");
  // Broad color bands reveal translucency; sharp stripes reveal Frost blur.
  unsigned char *pixels = malloc(1920 * 1080 * 4); assert(pixels);
  for (int y = 0; y < 1080; y++) for (int x = 0; x < 1920; x++) {
    unsigned char *p = pixels + (y*1920+x)*4;
    p[0] = 40 + (x/180%3)*60; p[1] = 40 + (y/180%3)*60;
    p[2] = 60 + (x%16 < 8 ? 120 : 0); p[3] = 255;
  }
  glGenTextures(1, &backdrop); glBindTexture(GL_TEXTURE_2D, backdrop);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  free(pixels); gfx_tex_esquecer(0);
  const char *layouts[] = {"modern", "lateral", "appletv"};
  const char *pages[] = {"home", "library", "live-tv"};
  const int destinations[] = {MENU_INICIO, MENU_BIBLIOTECA, MENU_GUIA};
  for (int layout = 0; layout < 3; layout++) {
    settings(layout, 1); menu_iniciar();
    for (int page = 0; page < 3; page++) for (int open = 0; open < 2; open++) {
      menu_definir_destino(destinations[page]); menu_pilula_mostrar(1);
      if (open) menu_abrir(); else menu_fechar();
      for (int j = 0; j < 100; j++) frame(0); // Let existing springs settle.
      char label[128]; snprintf(label, sizeof label, "%s-%s-%s", layouts[layout], pages[page], open ? "open" : "closed");
      modes(argv[1], label, layout, 0);
    }
  }
  modes(argv[1], "library-origins", 2, 1);
  modes(argv[1], "live-tv-categories", 2, 2);
  printf("%s: runtime sidebar materials (%d failures)\n", failures ? "FAIL" : "PASS", failures);
  tex_encerrar(); txt_encerrar(); gfx_encerrar();
  SDL_DestroyWindow(win); SDL_Quit();
  return failures ? 1 : 0;
}
#endif
