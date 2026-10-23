// Reuse the existing hidden-window GL capture harness with local artwork only.
#include "addons.h"
#define main i616_unused_shot_main
#include "estiloanim_shot.c"
#undef main

int main(int argc, char **argv) {
  assert(argc == 2);
  dirDados = getenv("NUVIO_DADOS");
  assert(dirDados);
  dados_iniciar("deploy/app/art");
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  janela = SDL_CreateWindow("#616 picker", 0, 0, 1920, 1080,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(janela && SDL_GL_CreateContext(janela));
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar() && txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  char path[800];
  snprintf(path, sizeof path, "%s/ajustes.txt", dirDados);
  FILE *f = fopen(path, "w"); assert(f);
  fprintf(f, "idioma 1\nhomeLayoutLocal 0\nvidroLocal 0\nmodernLandscapePostersEnabled 0\nfocusedPosterBackdropExpandEnabled 1\n");
  fclose(f);
  ajustes_dir(dirDados);
  assert(home_iniciar("deploy/app/art"));
  AddonRemoto addon = { .nome = "Local fixture", .url = "https://fixture.invalid/manifest.json", .ativo = 1 };
  assert(addons_definir_lista(&addon, 1));
  CatItem items[5] = {0};
  CatFileira row = { .ini = 0, .n = 5 };
  snprintf(row.chave, sizeof row.chave, "i616_movies");
  snprintf(row.titulo, sizeof row.titulo, "Local movies");
  snprintf(row.tipo, sizeof row.tipo, "movie");
  snprintf(row.base, sizeof row.base, "https://fixture.invalid");
  snprintf(row.catId, sizeof row.catId, "movies");
  for (int i = 0; i < 5; i++) {
    snprintf(items[i].titulo, sizeof items[i].titulo, "Movie %d", i + 1);
    snprintf(items[i].tipo, sizeof items[i].tipo, "movie");
    snprintf(items[i].poster, sizeof items[i].poster, "deploy/app/art/poster/%02d.jpg", i + 3);
    snprintf(items[i].backdrop, sizeof items[i].backdrop, "deploy/app/art/%02d.jpg", i + 3);
  }
  cat_definir_tudo(items, 5, &row, 1);
  quadros(60, NULL);
  SDL_Event event = { .type = SDL_KEYDOWN };
  event.key.keysym.sym = SDLK_DOWN;
  home_evento(&event);
  quadros(60, NULL);
  ctx_abrir_fileira(row.chave, row.titulo);
  event.type = SDL_KEYUP; event.key.keysym.sym = SDLK_RETURN; ctx_evento(&event);
  quadros(60, NULL);
  GfxRect start, end;
  float maxDrift = 0;
  assert(ajustes_posteres_deitados());
  assert(ctx_estilo_rect(&start));
  // Traverse landscape and tall preview styles, then return to Poster.
  for (int i = 0; i < 17; i++) {
    event.type = SDL_KEYDOWN; event.key.keysym.sym = i < 9 ? SDLK_DOWN : SDLK_UP;
    ctx_evento(&event);
    event.type = SDL_KEYUP; ctx_evento(&event);
    quadros(30, NULL);
    assert(ctx_estilo_rect(&end));
    float drift = fabsf(start.x - end.x) + fabsf(start.y - end.y);
    if (drift > maxDrift) maxDrift = drift;
    if (i == 8) {
      snprintf(path, sizeof path, "%s-stage.png", argv[1]);
      quadros(1, path);
    }
  }
  quadros(90, argv[1]);
  assert(ctx_estilo_rect(&end));
  assert(fil_tipo(row.chave) == FIL_TIPO_CARTAZ);
  HomeItem displayed;
  assert(home_item_focado(&displayed));
  printf("picker opening=(%.1f,%.1f) final=(%.1f,%.1f) max drift=%.1f; poster=%.1fx%.1f art=%s\n",
         start.x, start.y, end.x, end.y, maxDrift, displayed.rect.w, displayed.rect.h,
         displayed.arte ? displayed.arte : "");
  if (getenv("I616_FIXED")) {
    assert(maxDrift < .01f);
    assert(displayed.rect.w < displayed.rect.h);
    assert(displayed.arte && strstr(displayed.arte, "/poster/"));
  }
  return 0;
}
