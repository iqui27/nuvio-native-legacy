// Offline 1080p host evidence: real Home/Settings, glass and live-color matrix.
#include "app.h"
#include "addons.h"
#include "ajustes.h"
#include "artehero.h"
#include "catalogo.h"
#include "corviva.h"
#include "dados.h"
#include "gfx.h"
#include "gpunivel.h"
#include "home.h"
#include "ilha.h"
#include "menu.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void ajustes_teste_tema(int tema, int vidro);
extern int ajustes_teste_op_por_chave(const char *chave);
extern int ajustes_teste_focar_opcao(int op);
static SDL_Window *win;
static int settings, changing, step;
static void frame(void) {
  Uint32 now = 20000;
  txt_novo_quadro(); tex_novo_quadro(); gfx_novo_quadro();
  gfx_ambiente_preparar(); gpun_quadro_inicio();
  gfx_vidro_fundo_passo(changing ? step++ : -1);
  glClearColor(.051f, .051f, .051f, 1); glClear(GL_COLOR_BUFFER_BIT);
  corviva_quadro(1.f / 60, ajustes_cor_viva(), ajustes_cor_logo(), 0);
  gfx_ambiente(1);
  if (settings) { ajustes_atualizar(1.f / 60, now); ajustes_desenhar(now); }
  else { home_atualizar(1.f / 60, now); home_desenhar(now); }
  menu_pilula_mostrar(1); menu_atualizar(1.f / 60, now); menu_desenhar(now);
  ilha_desenhar(now); gfx_ambiente_descarregar(); gpun_quadro_fim();
}
static void capture(const char *path) {
  unsigned char *pixels = malloc(1920 * 1080 * 4);
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(pixels && s);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  for (int y = 0; y < 1080; y++) memcpy((char *)s->pixels + y * s->pitch, pixels + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(IMG_SavePNG(s, path) == 0); free(pixels); SDL_FreeSurface(s);
}
int main(int argc, char **argv) {
  assert(argc == 2);
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
  win = SDL_CreateWindow("GPU 1080 evidence", 0, 0, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(win && SDL_GL_CreateContext(win)); SDL_GL_SetSwapInterval(0);
  printf("HOST %s | %s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080); gpun_iniciar(1920, 1080);
  assert(gfx_iniciar() && txt_iniciar("deploy/app", 1));
  dados_iniciar("deploy/app/art");
  char path[1024]; snprintf(path, sizeof path, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
  FILE *f = fopen(path, "w"); assert(f);
  fputs("homeLayoutLocal 2\nmenuTopoLocal 1\nidioma 1\ntrailerHero 1\nrelogioTelaLocal 0\n", f); fclose(f);
  ajustes_dir(getenv("NUVIO_DADOS"));
  setenv("NUVIO_SHOT_VIDRO_FOSCO", "1", 1); ajustes_teste_vidro_env();
  tex_iniciar(64); gfx_icones_dir("deploy/app/art"); artehero_definir_falhou(tex_falhou);
  AddonRemoto addon = {.nome = "Offline", .url = "local", .ativo = 1};
  assert(addons_definir_lista(&addon, 1));
  assert(home_iniciar("deploy/app/art")); menu_iniciar(); ilha_relogio_visivel(1);
  static CatItem items[8];
  CatFileira row = {.chave = "popular_movie", .titulo = "Popular", .tipo = "movie", .n = 8};
  for (int i = 0; i < 8; i++) {
    snprintf(items[i].imdb, sizeof items[i].imdb, "tt90%05d", i);
    snprintf(items[i].tipo, sizeof items[i].tipo, "movie");
    snprintf(items[i].titulo, sizeof items[i].titulo, "Movie %d", i + 1);
    snprintf(items[i].sinopse, sizeof items[i].sinopse, "A journey through light and shadow.");
    snprintf(items[i].backdrop, sizeof items[i].backdrop, "deploy/app/art/%02d.jpg", i);
    snprintf(items[i].backdropCatalogo, sizeof items[i].backdropCatalogo, "%s", items[i].backdrop);
    snprintf(items[i].poster, sizeof items[i].poster, "deploy/app/art/poster/%02d.jpg", i);
  }
  cat_definir_tudo(items, 8, &row, 1);
  for (int i = 0; i < 180; i++) { tex_bombear(16); frame(); SDL_Delay(2); }
  int timer = strstr((const char *)glGetString(GL_EXTENSIONS), "GL_EXT_timer_query") != NULL;
  GLuint query = 0; if (timer) glGenQueries(1, &query);
  printf("TIMER %s; mean of 120 settled frames; glFinish wall fallback\n", timer ? "GL_EXT_timer_query" : "unavailable");
  for (settings = 0; settings <= 1; settings++) {
    if (settings) {
      assert(ajustes_iniciar());
      assert(ajustes_teste_focar_opcao(ajustes_teste_op_por_chave("resolucaoUi")));
    }
    for (changing = 0; changing <= 1; changing++)
    for (int glass = 0; glass <= 1; glass++) for (int color = 0; color <= 1; color++) for (int level = 0; level <= 2; level++) {
      ajustes_teste_tema(color ? 12 : 0, glass); gpun_definir_nivel(level);
      for (int i = 0; i < 90; i++) frame();
      double ms = 0, fill = 0; int passes = 0, sources = 0;
      glFinish();
      for (int i = 0; i < 120; i++) {
        GLuint64 ns = 0; Uint64 start = SDL_GetPerformanceCounter();
        if (timer) glBeginQuery(GL_TIME_ELAPSED_EXT, query);
        frame();
        if (timer) { glEndQuery(GL_TIME_ELAPSED_EXT); glGetQueryObjectui64vEXT(query, GL_QUERY_RESULT, &ns); }
        else glFinish();
        ms += timer ? ns / 1e6 : (SDL_GetPerformanceCounter() - start) * 1000.0 / SDL_GetPerformanceFrequency();
        fill += gfx_fill; passes += gfx_vidro_passadas; sources += gfx_vidro_com_fonte;
      }
      printf("MEASURE %s changing=%d glass=%d color=%d level=%d ms=%.3f fill=%.3f blur_passes=%.2f glass_sources=%.2f\n", settings ? "ajustes" : "home", changing, glass, color, level, ms / 120, fill / 120, passes / 120., sources / 120.);
      if (!changing && color && (glass || (settings && level == 0))) {
        snprintf(path, sizeof path, "%s/%s-glass%d-level%d.png", argv[1], settings ? "ajustes" : "home", glass, level); capture(path);
      }
    }
  }
  if (query) glDeleteQueries(1, &query);
  return 0;
}
