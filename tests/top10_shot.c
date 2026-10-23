// Reuse hidden-window capture and frame stepping with local catalogue artwork.
#define main top10_unused_scroll_main
#include "home_scroll_shot.c"
#undef main

int main(int argc, char **argv) {
  assert(argc == 2);
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  window = SDL_CreateWindow("Top 10 artwork", 0, 0, 1920, 1080,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(window && SDL_GL_CreateContext(window));
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar() && txt_iniciar("deploy/app", 1));
  dados_iniciar("deploy/app/art");
  tex_iniciar(64); gfx_icones_dir("deploy/app/art");
  artehero_definir_falhou(tex_falhou);
  char path[1024];
  snprintf(path, sizeof path, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
  FILE *file = fopen(path, "w"); assert(file);
  fprintf(file, "homeLayoutLocal 0\nvidroLocal 1\nanimacoes 0\n"
                "modernLandscapePostersEnabled 0\nheroSectionEnabled 1\n"
                "trailerHero 1\nsocialLocal 1\nidioma 1\n");
  fclose(file); ajustes_dir(getenv("NUVIO_DADOS"));
  assert(ajustes_posteres_deitados() && !ajustes_hero_ligado());
  assert(home_iniciar("deploy/app/art"));
  AddonRemoto addon = {.nome = "Offline", .url = "local", .ativo = 1};
  assert(addons_definir_lista(&addon, 1));
  CatFileira row = {.ini = 0, .n = 10};
  snprintf(row.chave, sizeof row.chave, "top10_movie");
  snprintf(row.titulo, sizeof row.titulo, "Top 10 movies");
  snprintf(row.tipo, sizeof row.tipo, "movie");
  fil_registrar(row.chave, row.titulo, "Offline", "Movies", 10);
  assert(fil_definir_tipo(row.chave, FIL_TIPO_RANKING));
  static CatItem items[10];
  for (int i = 0; i < 10; i++) {
    snprintf(items[i].titulo, sizeof items[i].titulo, "Movie %d", i + 1);
    snprintf(items[i].tipo, sizeof items[i].tipo, "movie");
    snprintf(items[i].poster, sizeof items[i].poster, "deploy/app/art/poster/%02d.jpg", i + 3);
    snprintf(items[i].backdrop, sizeof items[i].backdrop, "deploy/app/art/%02d.jpg", i + 3);
  }
  cat_definir_tudo(items, 10, &row, 1);
  frames(160);
  assert(nFileiras == 1 && fileiras[0].tipo == FILEIRA_TOP10_NUM);
  focoHero = 0; foco.fileira = foco.coluna = 0;
  frames(100);
  HomeItem displayed;
  assert(home_item_focado(&displayed));
  assert(displayed.rect.w < displayed.rect.h);
  assert(displayed.arte && strstr(displayed.arte,
         getenv("TOP10_BEFORE") ? "/art/03.jpg" : "/poster/03.jpg"));
  shot(argv[1], 0, "numbered");
  printf("PASS numbered portrait geometry; selected artwork: %s\n", displayed.arte);
  SDL_Quit();
  return 0;
}
