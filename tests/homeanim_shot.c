// Offline production Home rendering, navigation and frame-counter proof.
#include "../src/gfx.h"
#include "../src/text.h"
#include "../src/tex_cache.h"
#include <string.h>
static GfxRect roundAvatar;
static float roundCaptionWidth, roundCaptionAlpha, roundDecodeWidth;
static int roundCaptions;
static void recordRect(GfxRect r, GLuint t, GfxModo mode, float f,
                       float x, float y, float radius, float cr, float cg, float cb, float a) {
  if (mode == GFX_AVATAR) roundAvatar = r;
  gfx_rect(r, t, mode, f, x, y, radius, cr, cg, cb, a);
}
static TxtLinha recordCaption(TxtEstilo style, const char *s, int r, int g, int b, int a, float w) {
  if (s && (!strncmp(s, "Round ", 6) || !strncmp(s, "Country ", 8))) {
    roundCaptionWidth = w; roundCaptions++;
  }
  return txt_linha_corta(style, s, r, g, b, a, w);
}
static void recordAlpha(TxtLinha line, float x, float y, float a) {
  if (line.w > 0) roundCaptionAlpha = a;
  txt_desenhar_alpha(line, x, y, a);
}
static GLuint recordDecode(const char *path, float w) {
  roundDecodeWidth = w;
  return tex_obter_larg(path, w);
}
#define gfx_rect recordRect
#define txt_linha_corta recordCaption
#define txt_desenhar_alpha recordAlpha
#define tex_obter_larg recordDecode
#include "../src/home.c"
#undef gfx_rect
#undef txt_linha_corta
#undef txt_desenhar_alpha
#undef tex_obter_larg
#include "../src/amigosfil.c"
#include <SDL2/SDL_image.h>
#include <assert.h>

static Uint32 clockMs = 1000;
static int failures, baseline, roundShot;
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
static void roundFocus(int r) {
  focusRow(r);
  // Isolate the production drawer at a fixed base size throughout its spring.
  const CatItem *ci = cat_item_exato(fileiraItemIndice(&fileiras[r], 0));
  GLuint texture = tex_obter_larg(ci->poster, larguraFil(r));
  check(texture != 0, "Round fixture artwork loaded");
  roundCaptions = 0;
  GfxRect baseRect = {200, 600, larguraFil(r), alturaFil(r)};
  home_vitrine_cartao(ci, baseRect, baseRect, 1, 1, .5f);
  check(fabsf(roundAvatar.w - larguraFil(r)) < .01f,
        "Round ring stays outside photo without shrinking crop");
  float base = larguraFil(r);
  float unfocusedAlpha;
  home_vitrine_cartao(ci, baseRect, baseRect, 1, 1, 0);
  unfocusedAlpha = roundCaptionAlpha;
  home_vitrine_cartao(ci, baseRect, baseRect, 1, 1, .5f);
  check(roundCaptionAlpha > unfocusedAlpha && roundCaptionAlpha < .98f,
        "Round name fades with shared focus value");
  assert(ajustes_shot_valor("bordaFocoCartaz", 1));
  key(SDLK_RIGHT); frame();
  check(animFoco[r][0] > 0 && animFoco[r][1] > 0 && animFoco[r][1] < .999f,
        "Round incoming and outgoing focus share Home spring");
  GfxRect grown = {197, 597, base * 1.03f, base * 1.03f};
  home_vitrine_cartao(ci, baseRect, grown, 1, 1, .5f);
  check(fabsf(roundCaptionWidth - base) < .01f,
        "Round name layout stays at resting width during focus growth");
  check(fabsf(roundDecodeWidth - base) < .01f,
        "Round texture decode stays at resting width during focus growth");
  check(roundCaptions > 0, "Round names are drawn below photos");
  frames(80);
  assert(ajustes_shot_valor("bordaFocoCartaz", 0));
  check(alturaTotalFil(r) >= alturaFil(r) + NV_POSTER_COPY_H,
        "Round row reserves stable name space");
}
static void entryStrip(int r, const char *name) {
  focusRow(r - 1);
  SDL_Surface *strip = SDL_CreateRGBSurfaceWithFormat(0, 1920, 5400, 32, SDL_PIXELFORMAT_RGBA32);
  assert(strip); capture(strip, 0);
  float previous = scrollY;
  key(SDLK_DOWN);
  for (int i = 1; i < 5; i++) { frames(4); capture(strip, i); }
  check(foco.fileira == r && scrollY > previous && fabsf(velY) > .1f,
        "Down enters next row through shared vertical spring");
  char path[1024];
  snprintf(path, sizeof path, "%s/%s-down-%s.png", output, name, baseline ? "before" : "after");
  assert(IMG_SavePNG(strip, path) == 0); SDL_FreeSurface(strip);
  printf("STRIP %s frames=0,4,8,12,16 dt=1/60\n", path);
  frames(100);
}
static void exercise(int r, const char *name) {
  focusRow(r);
  for (int i = 0; i < 8; i++) { key(SDLK_RIGHT); frames(50); }
  check(foco.coluna == 8, "8 Right presses move exactly 8 cards");
  SDL_Surface *strip = SDL_CreateRGBSurfaceWithFormat(0, 1920, 5400, 32, SDL_PIXELFORMAT_RGBA32);
  assert(strip);
  key(SDLK_RIGHT);
  capture(strip, 0);
  float startX = scrollX[r];
  float scaleBefore = gfx_escala_txt_entrar();
  float targetX = alvoScrollFil(r, foco.coluna, startX, 0);
  gfx_escala_sair(scaleBefore);
  float expectedVelocity = velX[r];
  float expectedX = anim_mola2_reduzida(&expectedVelocity, startX, targetX,
                                      1.f / 60, NV_MOLA2_SCROLL, 0);
  int previous = heroEspecialCol[r]; frame();
  if (fileiras[r].tipo != FILEIRA_SOCIAL) {
    printf("SCROLL row=%s start=%.3f target=%.3f expected=%.3f actual=%.3f\n",
           name, startX, targetX, expectedX, scrollX[r]);
    check(fabsf(scrollX[r] - expectedX) < .01f && scrollX[r] > startX && scrollX[r] < targetX,
          "Right scroll uses official Home spring without snapping");
  }
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
  int rapidStart = foco.coluna;
  int presses = foco.nColunas[r] - 1 - rapidStart;
  if (presses > 5) presses = 5;
  for (int i = 0; i < presses; i++) { key(SDLK_RIGHT); frames(2); }
  check(presses > 0 && foco.coluna == rapidStart + presses,
        "N rapid Right presses move exactly N cards before row end");
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
  assert(argc >= 2); output = argv[1]; baseline = argc > 2 && !strcmp(argv[2], "before");
  roundShot = argc > 3 && !strcmp(argv[3], "round");
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
  CatFileira rows[3] = {normal,
    {.chave = "round_people", .titulo = "Round people", .tipo = "person", .n = ITEMS},
    {.chave = "round_catalog", .titulo = "Round catalog", .tipo = "movie", .n = ITEMS}};
  if (roundShot) {
    for (int r = 1; r < 3; r++) {
      fil_registrar(rows[r].chave, rows[r].titulo, "Offline", "Filmes", ITEMS);
      fil_definir_tipo(rows[r].chave, FIL_TIPO_REDONDA);
    }
    for (int i = 0; i < ITEMS; i++)
      snprintf(items[i].titulo, sizeof items[i].titulo, "Round %02d long portrait name", i + 1);
  }
  cat_definir_tudo(items, ITEMS, rows, roundShot ? 3 : 1); socialvis_definir_feed(events, ITEMS);
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
  if (roundShot) {
    fil_registrar("collection_world", "Round collections", "Offline", "", ITEMS);
    fil_definir_tipo("collection_world", FIL_TIPO_REDONDA);
  }
  home_ir_topo(); frames(100);
  int world = row("collection_world");
  exercise(row("normal_movie"), "normal");
  if (!roundShot) exercise(row("social_activity"), "friends");
  if (roundShot) {
    roundFocus(row("round_people"));
    exercise(row("round_people"), "round-people");
    exercise(row("round_catalog"), "round-catalog");
    entryStrip(row("round_people"), "round-people");
    entryStrip(row("round_catalog"), "round-catalog");
  }
  // Add a real trailing row so Down can interrupt the world row.
  Fileira trailing = fileiras[0];
  fileiras[nFileiras] = trailing; snprintf(fileiras[nFileiras].chave, sizeof trailing.chave, "trailing");
  foco.nColunas[nFileiras] = trailing.n; nFileiras++; foco.nFileiras = nFileiras;
  exercise(world, roundShot ? "round-collection" : "world");
  if (roundShot) {
    entryStrip(world, "round-collection");
    focusRow(world); roundCaptions = 0;
    check(fileiras[world].estiloCol == FILEIRA_REDONDA, "collection fixture uses Round drawer");
    check(alturaTotalFil(world) >= alturaFil(world) + NV_POSTER_COPY_H,
          "Round collection reserves name space before next row");
    desenhaAtalhos(world, 600);
    check(roundCaptions > 0, "Round collections draw their names below photos");
  }
  // Ring mode: incoming and outgoing cards share the same animated focus values.
  assert(ajustes_shot_valor("bordaFocoCartaz", 0));
  focusRow(row("social_activity")); key(SDLK_RIGHT); frame();
  check(afFocus[1] > 0 && afFocus[1] < .999f && afFocus[0] > 0,
        "focus ring crossfades between Friends cards");
  printf("%s home animation: %d failures\n", failures ? "FAIL" : "PASS", failures);
  tex_encerrar(); SDL_Quit(); return failures ? 1 : 0;
}
