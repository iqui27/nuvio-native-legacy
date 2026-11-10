/* Offline state/dispatch proof. No key events or device interaction. */
#define SPORTS_V5_MAIN unused_sports_main
#include "esportes_v5_shot.c"
#undef SPORTS_V5_MAIN

void sports_sidebar_menu_test(void);

int main(int argc, char **argv) {
  assert(argc == 2 && getenv("NUVIO_DADOS"));
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER));
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *window = SDL_CreateWindow("Sports state proof", 0, 0, 1920, 1080,
    SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(window && SDL_GL_CreateContext(window));
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar() && txt_iniciar("deploy/app", 1));
  tex_iniciar(64); gfx_icones_dir("deploy/app/art");
  dados_iniciar(getenv("NUVIO_DADOS"));
  sports_sidebar_menu_test();
  ajustes_definir_vidro(0); context(); semear(); fixture();

  /* The Guide's production tab dispatch still opens the complete Sports hub. */
  g5ui_open("grade"); activate(1102); quadro();
  assert(!strcmp(screen, "esportes-ligas") && gs_count == 9 && gs_n > 0);
  assert(gs_live(-1, NULL, NULL, NULL) > 0);
  for (int i = 0; i < 6; i++) {
    focus = 2070 + i; gs_activate();
    assert(!strcmp(screen, gs_pages[i]));
    gs_back(); assert(!strcmp(screen, "esportes-ligas") && focus == 2070 + i);
  }
  focus = 2076; gs_activate(); assert(!strcmp(screen, "esportes-canais"));
  gs_back(); assert(focus == 2076);
  gs_shelf_n[1] = 0; focus = 3400; gs_hub_activate();
  assert(!strcmp(screen, "esportes-modalidades") && gs_choosing);
  gs_back(); gs_collect();

  /* Reordering a live feed keeps the same match selected in its shelf. */
  focus = 3801;
  char selected[100]; strcpy(selected, gs_games[gs_shelf_ids[2][1]].id);
  rowCache[0].eventos[2].inicio += 86400;
  gs_collect();
  assert(!strcmp(selected, gs_games[gs_shelf_ids[2][(focus - 3000) % 400]].id));
  /* Shrinking a feed keeps its empty action visible and its offset valid. */
  focus = 5008; gs_shelf_offset[5] = 8; nNoticias = 0; gs_collect();
  assert(focus == 5000 && gs_shelf_offset[5] == 0);
  nNoticias = 1; gs_collect();
  focus = 3000; capture(window, argv[1], "hub-solid");
  ajustes_definir_vidro(1); capture(window, argv[1], "hub-glass"); ajustes_definir_vidro(0);

  /* Feed-specific loading/errors must survive unrelated requests. */
  esp_fixture_busy = 1; noticiasEstado = -1;
  assert(gs_empty_state("esportes-noticias") == -1);
  assert(gs_empty_state("esportes-canais") == 0);
  esp_fixture_busy = 0; noticiasEstado = 1; erroFav = 1;
  assert(gs_empty_state("esportes-noticias") == 0);
  assert(gs_empty_state("esportes-favoritos") == -1);
  erroFav = 0; noticiasEstado = 0;
  assert(gs_empty_state("esportes-noticias") == 1);
  assert(gs_empty_state("esportes-favoritos") == 0);
  nNoticias = 0; gs_collect(); focus = 5000;
  capture(window, argv[1], "news-loading"); assert(focus == 5000);
  noticiasEstado = -1; capture(window, argv[1], "news-error");
  assert(focus == 5000);
  gs_activate(); assert(!strcmp(screen, "esportes-ligas") && focus == 5000);
  assert(noticiasPedido && favoritoAtivo && catalogoAtualizar);
  /* Requests are only scheduled; no update loop or network worker runs. */
  noticiasEstado = 1; capture(window, argv[1], "news-empty");
  nNoticias = 1; gs_collect(); focus = 3000; gs_hub_activate();
  assert(!strcmp(screen, "esportes-evento") && gs_selected.nParticipantes == 2);
  capture(window, argv[1], "event"); gs_back(); assert(focus == 3000);

  /* Entering a fresh session must start every spring at its initial state. */
  gs_scroll = gs_scroll_v = gs_grid_row = gs_grid_v = 4;
  strcpy(gs_grid_page, "esportes-liga");
  for (int i = 0; i < 6; i++) gs_shelf_scroll[i] = gs_shelf_velocity[i] = gs_shelf_offset[i] = 4;
  gs_reset();
  assert(!gs_scroll && !gs_scroll_v && !gs_grid_row && !gs_grid_v && !gs_grid_page[0]);
  for (int i = 0; i < 6; i++) assert(!gs_shelf_scroll[i] && !gs_shelf_velocity[i] && !gs_shelf_offset[i]);
  puts("PASS Sports: Home visibility, Guide access, sections/channels/event/Back, refresh focus, scoped loading/error/empty, retry, session motion reset; no input events");
  return 0;
}
