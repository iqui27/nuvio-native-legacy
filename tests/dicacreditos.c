// Render the real islands and inspect their actual material/hint draw bounds.
#include "plrui.h"
#include "ajustes.h"
#include "idiomacod.h"
#include "tex_cache.h"
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GfxRect island;
static int failures, hints, language, kind;
static float scale;
static void recordMaterial(GfxRect r, float radius, float alpha);
static float recordHints(const char *const *keys, const char *const *labels,
                         int n, float x, float y, int right, float alpha);
#define plrui_material_video recordMaterial
#define plrui_dicas_video recordHints
#ifndef NV_POSPLAY_SOURCE
#define NV_POSPLAY_SOURCE "../src/posplay.c"
#endif
#include NV_POSPLAY_SOURCE
#undef plrui_material_video
#undef plrui_dicas_video

static void recordMaterial(GfxRect r, float radius, float alpha) {
  island = r;
  plrui_material_video(r, radius, alpha);
}
static float recordHints(const char *const *keys, const char *const *labels,
                         int n, float x, float y, int right, float alpha) {
  float width = plrui_dicas_video(keys, labels, n, x, y, right, alpha);
  if (alpha > 0 && x >= 0) {
    float left = right ? x - width : x;
    hints++;
    if (left < island.x + 28 - .01f || left + width > island.x + island.w - 28 + .01f ||
        y - 15 < island.y || y + 15 > island.y + island.h - 28 + .01f ||
        island.x < 40 || island.x + island.w > NV_VTELA_W - 40 + .01f) {
      fprintf(stderr, "FAIL lang=%d island=%d scale=%.2f hints-right=%.1f limit=%.1f\n",
              language, kind, scale, left + width, island.x + island.w - 28);
      failures++;
    }
  }
  return width;
}
static void savePng(const char *dir, const char *phase) {
  char path[1024];
  unsigned char *pixels = malloc(1920 * 1080 * 4);
  SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(pixels && surface);
  glFinish();
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  for (int y = 0; y < 1080; y++)
    memcpy((char *)surface->pixels + y * surface->pitch, pixels + (1079 - y) * 1920 * 4, 1920 * 4);
  snprintf(path, sizeof path, "%s/%s-%s-%s%s.png", dir, phase,
           kind ? "series" : "movie", language == IDIOMA_EN ? "en" : "de",
           scale > 1 ? "-ui150" : "");
  assert(IMG_SavePNG(surface, path) == 0);
  printf("PNG %s\n", path);
  SDL_FreeSurface(surface);
  free(pixels);
}
int main(int argc, char **argv) {
  assert(argc == 3);
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *window = SDL_CreateWindow("Post-play hints proof", 0, 0, 1920, 1080,
                                        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(window);
  SDL_GLContext context = SDL_GL_CreateContext(window);
  assert(context && gfx_iniciar() && txt_iniciar("deploy/app", 1));
  gfx_tamanho_alvo(1920, 1080);
  glViewport(0, 0, 1920, 1080);
  gfx_icones_dir("deploy/app/art");
  tex_iniciar(16);
  assert(ajustes_shot_valor("vidroLocal", 1));
  CatItem item = {0};
  CatEp episode = {0};
  snprintf(item.imdb, sizeof item.imdb, "tt0000001");
  snprintf(item.titulo, sizeof item.titulo, "Offline post-play fixture");
  cat_definir(&item, 1);
  episode.temporada = 1; episode.episodio = 2;
  snprintf(episode.duracao, sizeof episode.duracao, "56");
  snprintf(episode.nome, sizeof episode.nome, "The road ahead");
  snprintf(episode.sinopse, sizeof episode.sinopse, "A new journey begins as the story continues.");
  cat_definir_episodios(0, &episode, 1);
  const float scales[] = {1, 1.3f, 1.5f};
  for (unsigned s = 0; s < sizeof scales / sizeof *scales; s++) {
    scale = scales[s]; gfx_escala_ui_definir(scale);
    for (language = 0; language < IDIOMA_N; language++) {
      assert(ajustes_shot_valor("idioma", language + 1));
      assert(ajustes_idioma() == language);
      // Movie credits, next episode, and series credits without another episode.
      for (kind = 0; kind < 3; kind++) {
        posplay_shot(0, kind != 0, kind == 2 ? -1 : 1, kind == 2 ? 0 : 2, 0);
        aguardaCena = kind != 1;
        txt_novo_quadro(); tex_novo_quadro(); gfx_novo_quadro();
        glClearColor(.14f, .18f, .22f, 1); glClear(GL_COLOR_BUFFER_BIT);
        hints = 0;
        posplay_desenhar(1000, 0);
        assert(hints == 1);
        if (s != 1 && kind < 2 && (language == IDIOMA_EN || language == IDIOMA_DE))
          savePng(argv[1], argv[2]);
      }
    }
  }
  printf("%s: 31 languages, 3 islands/states, 3 UI scales; %d violations\n",
         failures ? "FAIL" : "PASS", failures);
  tex_encerrar(); txt_encerrar(); gfx_encerrar();
  SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
  return failures ? 1 : 0;
}
