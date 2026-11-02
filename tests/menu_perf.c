// Offscreen menu-only frame cost, including GPU completion. No vsync or network.
#ifndef MENU_SOURCE
#define MENU_SOURCE "../src/menu.c"
#endif
#include MENU_SOURCE
#ifdef MENU_BASELINE
int menu_ocupado(Uint32 agora) { (void)agora; return 0; }
#endif
#include "dados.h"
#include "gl_compat.h"
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

static double samples[360];
static int compare(const void *a, const void *b) {
  double x = *(const double *)a, y = *(const double *)b;
  return (x > y) - (x < y);
}
static void key(SDL_Keycode k) {
  SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = k; menu_evento(&e);
}
static void frame(int n) {
  txt_novo_quadro(); tex_novo_quadro(); gfx_novo_quadro();
  glClearColor(.12f, .16f, .22f, 1); glClear(GL_COLOR_BUFFER_BIT);
  menu_atualizar(1.f / 60, 1000 + n * 16);
  menu_desenhar(1000 + n * 16);
  glFinish();
}
int main(void) {
  const char *dir = getenv("NUVIO_DADOS");
  char path[1024];
  assert(dir);
  setvbuf(stdout, NULL, _IOLBF, 0);
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER));
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *win = SDL_CreateWindow("menu perf", 0, 0, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(win && SDL_GL_CreateContext(win));
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar() && txt_iniciar("deploy/app", 1));
  dados_iniciar("deploy/app/art"); tex_iniciar(64); gfx_icones_dir("deploy/app/art");
  snprintf(path, sizeof path, "%s/ajustes.txt", dir);
  for (int mode = 0; mode < 3; mode++) {
    FILE *f = fopen(path, "w"); assert(f);
    fprintf(f, "homeLayoutLocal %d\nmenuTopoLocal %d\nidioma 0\n", mode == 2 ? 0 : mode ? 2 : 0, mode == 2);
    fclose(f); ajustes_dir(dir); menu_iniciar();
    // Warm shaders and idle assets; first focus on each row remains measured.
    for (int n = 0; n < 60; n++) frame(n);
    int moves = 0, raster = 0;
#ifndef MENU_BASELINE
    menu_abrir();
    assert(menu_ocupado(SDL_GetTicks()));
    assert(!menu_ocupado(SDL_GetTicks() + 151));
    menu_fechar();
#endif
    for (int n = 0; n < 360; n++) {
      int rasterBefore = txt_rasterizadas;
      Uint64 start = SDL_GetPerformanceCounter();
      if (n % 120 == 0) menu_abrir();
      if (n % 120 == 90) menu_fechar();
      if (menu_aberto() && n % 6 == 0) {
        key(mode == 2 ? (n % 120 < 48 ? SDLK_RIGHT : SDLK_LEFT)
                      : (n % 120 < 48 ? SDLK_DOWN : SDLK_UP));
        moves++;
      }
      frame(n + 60);
      samples[n] = (SDL_GetPerformanceCounter() - start) * 1000.0 / SDL_GetPerformanceFrequency();
      raster += txt_rasterizadas - rasterBefore;
      assert(menu_destino() == MENU_INICIO && !menu_mudou_destino());
    }
    qsort(samples, 360, sizeof *samples, compare);
#ifndef MENU_BASELINE
    menu_abrir();
    for (int n = 0; n < 90; n++) frame(n + 500);
    unsigned char *pixels = malloc(1920 * 1080 * 4); assert(pixels);
    SDL_Surface *image = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
    assert(image);
    glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    for (int y = 0; y < 1080; y++)
      memcpy((char *)image->pixels + y * image->pitch, pixels + (1079 - y) * 1920 * 4, 1920 * 4);
    char png[1024];
    const char *out = getenv("MENU_PERF_OUT");
    snprintf(png, sizeof png, "%s/menu-%d.png", out ? out : "/tmp/nuvio-menu-perf-proof", mode);
    assert(!IMG_SavePNG(image, png)); SDL_FreeSurface(image); free(pixels);
#endif
    printf("menu-perf mode=%s frames=360 moves=%d open-close=3 p50=%.3fms p95=%.3fms max=%.3fms raster=%d\n",
           mode == 2 ? "top" : mode ? "tv" : "sidebar", moves, samples[179], samples[341], samples[359], raster);
    assert(moves == 45);
    assert(samples[341] < 33.0);
#ifndef MENU_BASELINE
    // Same visibility predicate drives drawing, pointer targets and focus order.
    FILE *hidden = fopen(path, "a"); assert(hidden);
    fputs("menuEsportesLocal 1\n", hidden); fclose(hidden);
    ajustes_dir(dir); assert(!ajustes_menu_esportes() && !mostra(MENU_ESPORTES));
    int order[NV_MENU_FOCOS_TV], count = mode == 2 ? topoLista(order) : tvOrdem(order);
    for (int i = 0; i < count; i++) assert(order[i] != MENU_ESPORTES);
    ajustes_dir(dir); assert(!ajustes_menu_esportes());
    hidden = fopen(path, "a"); assert(hidden);
    fputs("menuEsportesLocal 0\n", hidden); fclose(hidden);
    ajustes_dir(dir); assert(!mostra(MENU_ESPORTES)); // Sports lives inside Guide on TV.
#endif
  }
  return 0;
}
