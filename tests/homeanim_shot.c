// Offline production Home rendering, navigation and frame-counter proof.
#include "../src/home.c"
#include "../src/amigosfil.c"
#include <SDL2/SDL_image.h>
#include <assert.h>

static Uint32 clockMs = 1000;
static int failures, baseline;
static const char *output;
static double drawMs, frameTextMs;
static int frameUploads, frameRaster;

static void check(int ok, const char *label) {
  printf("%s %s\n", ok ? "PASS" : "FAIL", label);
  failures += !ok;
}
static void key(SDL_Keycode code) {
  SDL_Event e = {.type = SDL_KEYDOWN}; e.key.keysym.sym = code; home_evento(&e);
  e.type = SDL_KEYUP; home_evento(&e);
}
static void frame(void) {
  clockMs += 16;
  int uploaded = tex_upl_n;
  SDL_PumpEvents(); tex_bombear(16);
  frameUploads = tex_upl_n - uploaded;
  tex_novo_quadro(); txt_novo_quadro(); gfx_novo_quadro();
  gfx_sem_recorte(); glClearColor(.04f, .04f, .05f, 1); glClear(GL_COLOR_BUFFER_BIT);
  home_atualizar(1.f / 60, clockMs);
  double textBefore = txt_ms; int rasterBefore = txt_rasterizadas;
  Uint64 start = SDL_GetPerformanceCounter();
  home_desenhar(clockMs); gfx_ambiente_descarregar();
  drawMs = (SDL_GetPerformanceCounter() - start) * 1000.0 / SDL_GetPerformanceFrequency();
  frameTextMs = txt_ms - textBefore; frameRaster = txt_rasterizadas - rasterBefore;
  glFinish(); // Finish GPU work outside the production CPU draw measurement.
}
static void frames(int n) { while (n--) frame(); }
static void capture(SDL_Surface *strip, int index) {
  unsigned char *pixels = malloc(1920 * 1080 * 4); assert(pixels);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  // Five full-width frames stacked vertically; timestamps recorded in log.
  for (int y = 0; y < 1080; y++)
    memcpy((char *)strip->pixels + (index * 1080 + y) * strip->pitch,
           pixels + (size_t)(1079 - y) * 1920 * 4, 1920 * 4);
  free(pixels);
}
static int row(const char *keyName) {
  for (int r = 0; r < nFileiras; r++) if (!strcmp(fileiras[r].chave, keyName)) return r;
  assert(!"missing fixture row"); return -1;
}
static void focusRow(int r) {
  home_ir_topo(); frames(100); key(SDLK_DOWN); frames(80);
  while (foco.fileira < r) { key(SDLK_DOWN); frames(80); }
  assert(!focoHero && foco.fileira == r);
}
static void exercise(int r, const char *name) {
  focusRow(r);
  for (int i = 0; i < 8; i++) { key(SDLK_RIGHT); frames(50); }
  check(foco.coluna == 8, "8 Right presses move exactly 8 cards");
  SDL_Surface *strip = SDL_CreateRGBSurfaceWithFormat(0, 1920, 5400, 32, SDL_PIXELFORMAT_RGBA32);
  assert(strip);
  key(SDLK_RIGHT);
  capture(strip, 0);
  int previous = heroEspecialCol[r]; frame();
  if (!baseline && (fileiras[r].tipo == FILEIRA_SOCIAL || fileiras[r].tipo == FILEIRA_CATALOGOS))
    check(heroEspecialCol[r] == previous && heroEspecialAlpha[r] > 0 && heroEspecialAlpha[r] < 1,
          "hero title and metadata fade before changing identity");
  for (int i = 1; i < 5; i++) { frames(i == 1 ? 3 : 4); capture(strip, i); }
  char path[1024]; snprintf(path, sizeof path, "%s/%s-%s.png", output, name, baseline ? "before" : "after");
  assert(IMG_SavePNG(strip, path) == 0); SDL_FreeSurface(strip);
  printf("STRIP %s frames=0,4,8,12,16 dt=1/60\n", path);
  frames(80);
  check(foco.coluna == 9, "strip Right press moves one card");
  if (fileiras[r].tipo == FILEIRA_SOCIAL) {
    key(SDLK_LEFT); frame();
    GfxRect base = afRect(foco.coluna, ultX0 - scroll, 0);
    check(afFoco.w > base.w + .1f && afFoco.w < base.w * 1.06f - .1f,
          "Friends focus growth is partial on first frame");
    printf("FOCUS base=%.3f drawn=%.3f spring=%.3f border=%d\n", base.w, afFoco.w, afFocus[foco.coluna], ajustes_borda_foco());
    frames(80);
  }
  double sum = 0, worst = 0, rects = 0, fills = 0, text = 0;
  int uploads = 0, raster = 0;
  // Warm both directions first; timing excludes artwork arrival.
  for (int pass = 0; pass < 2; pass++) {
    for (int i = 0; i < 5; i++) { key(SDLK_LEFT); frames(24); }
    for (int i = 0; i < 5; i++) { key(SDLK_RIGHT); frames(24); }
  }
  for (int i = 0; i < 120; i++) {
    if (!(i % 24)) key(i < 72 ? SDLK_LEFT : SDLK_RIGHT);
    frame(); sum += drawMs; if (drawMs > worst) worst = drawMs;
    rects += gfx_n_rect; fills += gfx_fill; text += frameTextMs;
    uploads += frameUploads; raster += frameRaster;
  }
  printf("[quadro] phase=%s row=%s host=Mac frames=120 des=%.3fms worst=%.3fms uploads=%d\n",
         baseline ? "before" : "after", name, sum / 120, worst, uploads);
  printf("[quadro-des] row=%s rects=%.1f fill=%.3fx texto=%.3fms raster=%d\n",
         name, rects / 120, fills / 120, text / 120, raster);
  check(!uploads && !raster, "warm movement reuses textures and text layout");
  frames(80);
  int c = foco.coluna;
  float left, right;
  if (fileiras[r].tipo == FILEIRA_SOCIAL) {
    GfxRect a = afRect(c, ultX0 - scroll, 0); left = a.x; right = a.x + a.w;
  } else { left = homeFileiraX(r, c); right = left + larguraFil(r); }
  check(left >= homeConteudoX() - 1 && right <= homeConteudoDireita() + 1, "settled focus visible");
  for (int i = 0; i < 5; i++) { key(SDLK_RIGHT); frames(2); }
  float x = fileiras[r].tipo == FILEIRA_SOCIAL ? scroll : scrollX[r];
  key(SDLK_DOWN); frames(12);
  float next = fileiras[r].tipo == FILEIRA_SOCIAL ? scroll : scrollX[r];
  check(next > x + 1, "horizontal animation continues during vertical exit");
  check(fabsf(velY) > .1f, "vertical exit uses Home spring");
  frames(100); key(SDLK_UP); frame();
  check(fabsf(velY) > .1f && animFocoFil[r] > 0 && animFocoFil[r] < .999f,
        "vertical return animates row focus and position");
  frames(100);
}
int main(int argc, char **argv) {
  assert(argc >= 2); output = argv[1]; baseline = argc > 2;
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER)); IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *window = SDL_CreateWindow("Home animation proof", 0, 0, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(window && SDL_GL_CreateContext(window));
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar() && txt_iniciar("deploy/app", 1)); dados_iniciar("deploy/app/art");
  tex_iniciar(96); gfx_icones_dir("deploy/app/art"); artehero_definir_falhou(tex_falhou);
  assert(home_iniciar("deploy/app/art"));
  char path[1024]; snprintf(path, sizeof path, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
  FILE *prefs = fopen(path, "w"); assert(prefs);
  fputs("homeLayoutLocal 0\nanimacoes 0\nbordaFocoCartaz 1\nsocialLocal 0\nfriendsRowStyleLocal 0\ntrailerHero 1\nfocusedPosterBackdropExpandEnabled 0\n", prefs);
  fclose(prefs); ajustes_dir(getenv("NUVIO_DADOS"));
  AddonRemoto addon = {.nome = "Offline", .url = "local", .ativo = 1}; assert(addons_definir_lista(&addon, 1));
  enum { ITEMS = 20 };
  static CatItem items[ITEMS]; static SvEvento events[ITEMS];
  CatFileira normal = {.chave = "normal_movie", .titulo = "Normal posters", .tipo = "movie", .n = ITEMS};
  fil_registrar(normal.chave, normal.titulo, "Offline", "Filmes", ITEMS); fil_definir_tipo(normal.chave, FIL_TIPO_CARTAZ);
  for (int i = 0; i < ITEMS; i++) {
    CatItem *it = &items[i]; SvEvento *e = &events[i];
    snprintf(it->imdb, sizeof it->imdb, "tt90%05d", i); snprintf(it->tipo, sizeof it->tipo, "movie");
    snprintf(it->titulo, sizeof it->titulo, "Movie %02d", i + 1);
    snprintf(it->poster, sizeof it->poster, "deploy/app/art/poster/%02d.jpg", i % 8);
    snprintf(it->backdrop, sizeof it->backdrop, "deploy/app/art/%02d.jpg", i % 8);
    snprintf(e->pessoaId, sizeof e->pessoaId, "friend-%02d", i / 2);
    snprintf(e->pessoaNome, sizeof e->pessoaNome, "Friend %02d", i / 2 + 1);
    snprintf(e->imdb, sizeof e->imdb, "%s", it->imdb); snprintf(e->tipo, sizeof e->tipo, "movie");
    snprintf(e->titulo, sizeof e->titulo, "%s", it->titulo); snprintf(e->poster, sizeof e->poster, "%s", it->poster);
    snprintf(e->arte, sizeof e->arte, "%s", it->backdrop);
    e->acao = SV_FIM; e->pct = -1; e->quando = 1900000000 - i;
  }
  cat_definir_tudo(items, ITEMS, &normal, 1); socialvis_definir_feed(events, ITEMS);
  static ColFolder folders[ITEMS];
  for (int i = 0; i < ITEMS; i++) {
    snprintf(folders[i].id, sizeof folders[i].id, "country-%d", i);
    snprintf(folders[i].title, sizeof folders[i].title, "Country %d", i + 1);
    snprintf(folders[i].group, sizeof folders[i].group, "Volta ao mundo");
    snprintf(folders[i].groupId, sizeof folders[i].groupId, "world");
    snprintf(folders[i].cover, sizeof folders[i].cover, "%s", items[i].poster);
    folders[i].forma = COL_FORMA_POSTER;
  }
  assert(col_extra_definir_grupo("world", folders, ITEMS) == ITEMS);
  home_ir_topo(); frames(100);
  int world = row("collection_world");
  exercise(row("normal_movie"), "normal");
  exercise(row("social_activity"), "friends");
  // Add a real trailing row so Down can interrupt the world row.
  Fileira trailing = fileiras[0];
  fileiras[nFileiras] = trailing; snprintf(fileiras[nFileiras].chave, sizeof trailing.chave, "trailing");
  foco.nColunas[nFileiras] = trailing.n; nFileiras++; foco.nFileiras = nFileiras;
  exercise(world, "world");
  // Ring mode: incoming and outgoing cards share the same animated focus values.
  assert(ajustes_shot_valor("bordaFocoCartaz", 0));
  focusRow(row("social_activity")); key(SDLK_RIGHT); frame();
  check(afFocus[1] > 0 && afFocus[1] < .999f && afFocus[0] > 0,
        "focus ring crossfades between Friends cards");
  printf("%s home animation: %d failures\n", failures ? "FAIL" : "PASS", failures);
  tex_encerrar(); SDL_Quit(); return failures ? 1 : 0;
}
