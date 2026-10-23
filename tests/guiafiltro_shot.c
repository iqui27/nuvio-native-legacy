// Reuse the Guide offscreen harness and real renderer; no source/network requests.
#define main guide_shot_main
#include "guia_shot.c"
#undef main

static void seed(int mode) {
  static ShotCanal c[] = {
    { "Fixture", "Sports", "X 4K", "", "Live sports in UHD", 1 },
    { "Fixture", "Sports", "X FHD", "", "Live sports in Full HD", 2 },
    { "Fixture", "Sports", "X HD", "", "Live sports in HD", 3 },
    { "Fixture", "Movies", "Y FHD", "", "Movies in Full HD", 4 },
    { "Fixture", "Movies", "Y SD", "", "Movies in SD", 5 },
    { "Fixture", "Hidden", "Z HD", "", "Hidden category fixture", 6 },
  };
  fltPerfil = perfis_ativo();
  memset(&fltLive, 0, sizeof fltLive); fltLive.modo = mode;
  if (mode) guiafiltro_cat_alternar(&fltLive, "Hidden");
  sFltLive = fltLive;
  semearCanais(c, sizeof c / sizeof *c);
  assert(nCanais == (mode == GF_TODAS ? 6 : mode == GF_SO_4K ? 1 : 2));
  aberta = 1; entrada = 1; overlay = 0; modoLista = 0;
  focoLin = focoCol = focoTopo = 0;
}
static void shot(SDL_Window *win, const char *dir, const char *name) {
  for (int q = 0; q < 10; q++) {
    SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); gfx_novo_quadro();
    glClearColor(.03f, .03f, .035f, 1); glClear(GL_COLOR_BUFFER_BIT);
    guia_desenhar(SDL_GetTicks());
    if (q == 9) {
      unsigned char *pixels = malloc(1920 * 1080 * 4);
      SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      char path[1024]; assert(pixels && s);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
      for (int y = 0; y < 1080; y++) memcpy((char *)s->pixels + y * s->pitch, pixels + (1079-y)*1920*4, 1920*4);
      snprintf(path, sizeof path, "%s/%s.png", dir, name);
      assert(!IMG_SavePNG(s, path)); printf("PNG: %s\n", path);
      SDL_FreeSurface(s); free(pixels);
    }
    SDL_GL_SwapWindow(win);
  }
}
int main(int argc, char **argv) {
  assert(argc == 2);
  assert(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER));
  IMG_Init(IMG_INIT_PNG); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *win = SDL_CreateWindow("Live TV filter proof", 0, 0, 1920, 1080,
      SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(win); SDL_GLContext gl = SDL_GL_CreateContext(win); assert(gl);
  SDL_GL_SetSwapInterval(0); glViewport(0,0,1920,1080); gfx_tamanho_alvo(1920,1080);
  dados_iniciar(getenv("NUVIO_DADOS")); ajustes_definir_vidro(0); assert(!ajustes_vidro());
  assert(gfx_iniciar()); assert(txt_iniciar("deploy/app",1)); tex_iniciar(32);
  gfx_icones_dir("deploy/app/art"); badges_carregar("deploy/app/art");
  seed(GF_TODAS); shot(win, argv[1], "grid-before");
  // Navigate through real top-bar and panel events.
  focoTopo = 1; topoCol = G_TOPO_OPCOES;
  SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN;
  guia_evento(&e); assert(opcoesAberto);
  e.key.keysym.sym = SDLK_DOWN; guia_evento(&e);
  assert(opcoesFoco == G_TOPO_FILTROS);
  e.key.keysym.sym = SDLK_RETURN; guia_evento(&e);
  assert(!opcoesAberto && painel && paModo == 1);
  shot(win, argv[1], "panel-quality");
  paFoco = 3; paRol = 200; shot(win, argv[1], "panel-categories");
  filtrosEvento(SDLK_RIGHT, SDL_GetTicks()); assert(fNivel && fN == 3);
  shot(win, argv[1], "panel-channels");
  filtrosEvento(SDLK_RETURN, SDL_GetTicks() + 1000); assert(fltLive.nCan == 1);
  e.key.keysym.sym = SDLK_ESCAPE; guia_evento(&e); assert(!fNivel && painel);
  paFoco = 3; filtrosOk(); assert(guiafiltro_cat_oculta(&fltLive, "Sports"));
  paFoco = GF_PREFERIR_4K; filtrosOk(); assert(fltLive.modo == GF_PREFERIR_4K);
  // Closing without changes stays local; a changed panel queues one reload.
  fSujo = 0; guia_evento(&e); assert(!painel && !paModo);
  filtrosAbrir(); paFoco = GF_SO_4K; filtrosOk(); assert(fSujo);
  fioVivo = 1; guia_evento(&e);
  assert(!painel && cargaCancelada && recarregarPend);
  fioVivo = recarregarPend = cargaCancelada = 0;
  seed(GF_PREFERIR_4K); shot(win, argv[1], "grid-prefer-4k");
  seed(GF_SO_4K); shot(win, argv[1], "grid-only-4k");
  // Raw inventory keeps filtered channels available; use a truly empty category.
  guiafiltro_cat_alternar(&fltLive, "Empty fixture");
  filtrosAbrir();
  for (paFoco = 3; paFoco < 3 + fN && strcmp(fIt[paFoco - 3].nome, "Empty fixture"); paFoco++) {}
  filtrosEvento(SDLK_RIGHT, SDL_GetTicks());
  assert(fNivel && fN == 0); filtrosOk();
  shot(win, argv[1], "panel-empty-channels");
  filtrosVoltar(); assert(!fNivel); painelFechar();
  tex_encerrar(); txt_encerrar(); gfx_encerrar(); SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(win); SDL_Quit(); return 0;
}
