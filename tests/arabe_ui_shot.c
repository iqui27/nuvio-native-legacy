// Offline native rendering proof, using only packaged fallback fonts.
#include "ajustes.h"
#include "addons.h"
#include "catalogo.h"
#include "home.h"
#include "idioma.h"
#include "idiomaauto.h"
#include "linguas.h"
#include "menu.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern int ajustes_teste_focar_opcao(int op);
extern int ajustes_teste_op_por_chave(const char *chave);

static void capture(SDL_Window *window, const char *directory, const char *name, int settings) {
  for (int frame = 0; frame < 90; frame++) {
    Uint32 now = SDL_GetTicks();
    SDL_PumpEvents();
    txt_novo_quadro(); tex_novo_quadro(); gfx_novo_quadro(); tex_bombear(6);
    glClearColor(0.025f, 0.025f, 0.03f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    if (settings) {
      ajustes_atualizar(1.0f / 60, now);
      ajustes_desenhar(now);
    } else {
      home_atualizar(1.0f / 60, now);
      home_desenhar(now);
      menu_atualizar(1.0f / 60, now);
      menu_desenhar(now);
    }
    if (frame == 89) {
      char path[1024];
      unsigned char *pixels = malloc(1920 * 1080 * 4);
      SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      assert(pixels && surface);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
      for (int y = 0; y < 1080; y++)
        memcpy((char *)surface->pixels + y * surface->pitch, pixels + (1079 - y) * 1920 * 4, 1920 * 4);
      snprintf(path, sizeof path, "%s/%s.png", directory, name);
      assert(IMG_SavePNG(surface, path) == 0);
      puts(path);
      SDL_FreeSurface(surface); free(pixels);
    }
    SDL_GL_SwapWindow(window);
  }
}

int main(int argc, char **argv) {
  assert(argc == 2);
  const char *directory = getenv("NUVIO_DADOS");
  char path[1024], visual[1024], code[16];
  assert(directory && *directory);
  snprintf(path, sizeof path, "%s/ajustes.txt", directory);
  FILE *file = fopen(path, "w"); assert(file);
  fprintf(file, "idioma %d\n", IDIOMA_AR);
  fclose(file);
  ajustes_dir(directory);
  ajustes_iniciar();
  assert(ajustes_idioma() == IDIOMA_AR);
  assert(!strcmp(ajustes_tmdb_idioma(), "ar-SA"));
  assert(!strcmp(ling_nome("ar-SA"), "Árabe"));
  ling_normalizar("العربية", code, sizeof code);
  assert(ling_casa(code, "ar"));

  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *window = SDL_CreateWindow("Arabic UI proof", 0, 0, 1920, 1080,
                                        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(window && SDL_GL_CreateContext(window));
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar() && txt_iniciar("deploy/app", 1));
  tex_iniciar(64); gfx_icones_dir("deploy/app/art");

  // The real UI text path must join letters even with an Inter/Montserrat UI.
  for (int family = 0; family < TXT_FAMILIA_N; family++) {
    int runs = 0;
    assert(txt_visual_da_linha(family, TXT_DET_META, "سلام", visual, sizeof visual, &runs) == 0);
    assert(!strcmp(visual, "\uFEE1\uFEFC\uFEB3"));
    assert(txt_visual_da_linha(family, TXT_DET_META, "العربية Nuvio 2026", visual, sizeof visual, &runs) == 0);
    assert(strstr(visual, "Nuvio 2026") && runs >= 2);
  }
  // Every translated menu/settings row must have glyphs, including joined forms.
  static const char *const translations[] = {
#define T(key, value) value
#include "idioma_ar.h"
#undef T
  };
  for (unsigned i = 0; i < sizeof translations / sizeof *translations; i++) {
    int runs;
    char output[16384], logical[4096], *next;
    snprintf(logical, sizeof logical, "%s", translations[i]);
    // UI wraps first; newline is a separator, never a glyph.
    for (char *line = strtok_r(logical, "\n", &next); line; line = strtok_r(NULL, "\n", &next)) {
      int missing = txt_visual_da_linha(TXT_FAMILIA_INTER, TXT_DET_META,
                                         line, output, sizeof output, &runs);
      if (missing) fprintf(stderr, "Missing glyphs at Arabic row %u: %s\n", i, line);
      assert(missing == 0);
    }
  }
  txt_novo_quadro();
  assert(txt_tracking(TXT_AJ_CAPS13, "الإعدادات", 255, 255, 255, -1, 0, 1, 2.1f) ==
         txt_largura(TXT_AJ_CAPS13, "الإعدادات"));
  assert(home_iniciar("deploy/app/art"));
  AddonRemoto addon = { .nome = "Fixture", .url = "http://127.0.0.1:1/manifest.json", .ativo = 1 };
  assert(addons_definir_lista(&addon, 1));
  static CatItem items[6];
  CatFileira row = { .chave = "arabic_movies", .titulo = "أفلام مشهورة", .tipo = "movie", .n = 6 };
  for (int i = 0; i < 6; i++) {
    snprintf(items[i].titulo, sizeof items[i].titulo, "رحلة إلى عالم السينما");
    snprintf(items[i].tipo, sizeof items[i].tipo, "movie");
    snprintf(items[i].imdb, sizeof items[i].imdb, "tt900000%d", i);
    snprintf(items[i].meta, sizeof items[i].meta, "2026 · دراما");
    snprintf(items[i].sinopse, sizeof items[i].sinopse,
             "رحلة جديدة تجمع الأصدقاء في مغامرة مليئة بالأمل والمفاجآت.");
    snprintf(items[i].backdrop, sizeof items[i].backdrop, "deploy/app/art/%02d.jpg", i);
    snprintf(items[i].poster, sizeof items[i].poster, "deploy/app/art/poster/%02d.jpg", i);
  }
  cat_definir_tudo(items, 6, &row, 1);
  menu_iniciar(); menu_abrir();
  capture(window, argv[1], "home", 0);
  assert(ajustes_teste_focar_opcao(ajustes_teste_op_por_chave("idioma")));
  capture(window, argv[1], "settings", 1);
  assert(ajustes_teste_focar_opcao(ajustes_teste_op_por_chave("tmdb_language")));
  capture(window, argv[1], "settings-metadata", 1);
  unlink(path);
  puts("Arabic settings selection, packaged glyphs, joining and mixed text: PASS");
  return 0;
}
