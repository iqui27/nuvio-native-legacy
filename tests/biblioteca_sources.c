// Exercise the real Library handlers and renderer, with offline provider data.
#define lst_pedir sources_request
#define sync_ultimo_ok sources_sync_ok
#define trakt_ativo sources_connected
#define simkl_ativo sources_connected
#define main listasux_capture_main
#include "listasux_shot.c"
#undef main
#undef lst_pedir
#undef sync_ultimo_ok
#undef trakt_ativo
#undef simkl_ativo
unsigned sources_sync_ok(void) { return 1; }
int sources_connected(void) { return 1; }
void sources_request(int source) { (void)source; }
static int failures;
#define CHECK(test, message) do { if (!(test)) { fprintf(stderr, "FAIL: %s\n", message); failures++; } } while (0)
static int rows(int i, const char **name, const char **sub, int *count) {
  static const char *names[] = {"Plex", "Jellyfin", "Emby"};
  if (i < 0 || i >= 3) return 0;
  *name = names[i]; *sub = "Connected"; *count = 1; return 1;
}
static int contains(const CatItem *item, int provider) {
  return !strcmp(item->origem, provider == 0 ? "plex" : provider == 1 ? "jellyfin" : "emby");
}
static void fixture_sources(void) {
  CatItem *items = calloc(8, sizeof *items); assert(items);
  const char *names[] = {"Trakt title", "Simkl title", "Account title", "Add-on title", "Plex title", "Jellyfin title", "Emby title", "Trakt duplicate"};
  for (int i = 0; i < 8; i++) {
    snprintf(items[i].imdb, sizeof items[i].imdb, "tt%07d", i == 7 ? 0 : i);
    snprintf(items[i].titulo, sizeof items[i].titulo, "%s", names[i]);
    snprintf(items[i].poster, sizeof items[i].poster, "deploy/app/art/poster/%02d.jpg", 18 + i);
    strcpy(items[i].tipo, "movie");
    if (i < 4 || i == 7) {
      items[i].naLista = 1;
      items[i].listaFontes = 1u << (i == 0 ? AJ_SALVOS_TRAKT : i == 1 || i == 7 ? AJ_SALVOS_SIMKL : AJ_SALVOS_LOCAL);
      items[i].fontes = i == 0 ? CAT_FONTE_TRAKT : i == 1 || i == 7 ? CAT_FONTE_SIMKL : CAT_FONTE_NUVIO;
    }
    if (i == 3) strcpy(items[i].origem, "AIOMetadata");
    if (i >= 4 && i <= 6) strcpy(items[i].origem, i == 4 ? "plex" : i == 5 ? "jellyfin" : "emby");
    items[i].naColecao = 1;
  }
  cat_definir(items, 8); free(items);
}
static void reset_picker(int mode) {
  modo = mode; temAberta = 0; origem = ORIG_TODAS; serverFilter = -1;
  origensAbertas = focoOrigem = 0; focoBotaoOrig = 1;
  sair = 0; pickSel = 0; foco.fileira = BIB_FIL_PICK; foco.coluna = 0;
  reconstruir();
  foco.fileira = BIB_FIL_PICK; foco.coluna = 0;
}
int main(int argc, char **argv) {
  assert(argc == 2 && getenv("NUVIO_DADOS"));
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0); IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  window = SDL_CreateWindow("Library sources proof", 0, 0, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(window && SDL_GL_CreateContext(window)); SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1)); tex_iniciar(64); gfx_icones_dir("deploy/app/art");
  dados_iniciar(getenv("NUVIO_DADOS"));
  setenv("NUVIO_RAIL", "dinamica", 1); setenv("NUVIO_SHOT_PT", "1", 1); setenv("NUVIO_SHOT_RELOGIO", "1", 1);
  rail_shot_aplicar(); fixture_sources(); biblioteca_iniciar();
  biblioteca_origens_servidores(rows, biblioteca_servidor_filtrar);
  biblioteca_servidores_contem(contains);
  reset_picker(MODO_PARAVER); shot(argv[1], "library-closed");
  CHECK(nCelulas == 7, "All must include every source, independently of save destination");
  key(SDLK_RETURN); shot(argv[1], "library-open");
  CHECK(origensAbertas, "Sources opens");
  key(SDLK_DOWN); CHECK(focoOrigem, "Down enters picker after drawing");
  key(SDLK_ESCAPE);
  CHECK(!origensAbertas && !focoOrigem && focoBotaoOrig && !sair, "Back closes Library picker and restores button");
  foco.fileira = BIB_FIL_PICK; focoBotaoOrig = 1;
  key(SDLK_RETURN); key(SDLK_RETURN);
  CHECK(!origensAbertas && focoBotaoOrig && !sair, "Sources toggles closed on second OK");
  for (int m = MODO_PARAVER; m <= MODO_COLECOES; m++) {
    // Same titles in each state; source membership must survive the state filter.
    for (int i = 0; i < 8; i++) {
      CatItem *item = (CatItem *)cat_item(i);
      item->progresso = m == MODO_ANDAMENTO ? 50 : 0;
      cat_historico_definir_id(item->imdb, item->tipo, m == MODO_ASSISTIDOS);
    }
    reset_picker(m);
    CHECK(botaoOrigens(), "Every Library tab has the shared Sources button");
    for (int source = 0; source < 8; source++) {
      origem = ORIG_TODAS; serverFilter = -1; reconstruir();
      foco.fileira = BIB_FIL_PICK; focoBotaoOrig = 1;
      origensAbertas = focoOrigem = 0;
      key(SDLK_RETURN); key(SDLK_DOWN);
      CHECK(focoOrigem, "Down reaches source rows");
      // Reach every provider using arrows, without injecting selection.
      for (int n = 0; n < source; n++) key(SDLK_DOWN);
      key(SDLK_RETURN);
      CHECK(!origensAbertas && !focoOrigem && focoBotaoOrig && !sair, "Selecting a source closes picker and restores button");
      int expected[] = {7, 1, 2, 2, 1, 1, 1, 1};
      CHECK(nCelulas == expected[source], "Selected source filters real grid, including duplicate memberships");
      CHECK(source >= BIB_N_ORIG || contaOrigem[source] == expected[source], "Source counts match each provider, including duplicate memberships");
      if (m == MODO_PARAVER && source == 1) shot(argv[1], "library-selected");
      if (m == MODO_ASSISTIDOS && source == 7) shot(argv[1], "watched-emby");
    }
  }
  reset_picker(MODO_ANDAMENTO);
  for (int i = 0; i < 8; i++) ((CatItem *)cat_item(i))->progresso = 0;
  reconstruir(); CHECK(nCelulas == 0, "Empty provider grid stays empty");
  foco.fileira = BIB_FIL_PICK; focoBotaoOrig = 1;
  key(SDLK_RETURN); CHECK(origensAbertas, "Sources remains usable from empty state");
  key(SDLK_ESCAPE); CHECK(!origensAbertas && !sair, "Empty-state Back closes picker");
  reset_picker(MODO_LISTAS); fonte = FONTE_TRAKT; listaTab = 0;
  char *s = fixture("tests/fixtures/trakt_minhas_listas.json"); lst_ler_trakt(s, 0); free(s);
  remapear(0); foco.fileira = BIB_FIL_PICK; focoBotaoOrig = 1; posters(); shot(argv[1], "lists-selected");
  key(SDLK_RETURN); key(SDLK_DOWN); key(SDLK_ESCAPE);
  CHECK(!origensAbertas && !focoOrigem && focoBotaoOrig && modo == MODO_LISTAS, "Lists Back still restores Sources button");
  key(SDLK_ESCAPE); CHECK(modo == MODO_PARAVER && !sair, "Lists Back returns to Library after picker closes");
  key(SDLK_ESCAPE); CHECK(sair, "Library Back exits only after picker closes");
  fprintf(stdout, "Library source navigation/rendering: %s (%d failures)\n", failures ? "FAIL" : "PASS", failures);
  return failures ? 1 : 0;
}
