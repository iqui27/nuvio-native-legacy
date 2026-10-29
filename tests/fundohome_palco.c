// Real Home rendering, cold texture cache, local artwork, no detail visit.
#define main homelayouts_fixture_main
#include "homelayouts_shot.c"
#undef main
#include <sys/socket.h>

// The build renames connect in every translation unit: any network attempt fails.
int fundohome_no_connect(int fd, const struct sockaddr *addr, socklen_t len) {
  (void)fd; (void)addr; (void)len;
  fputs("FAIL: unexpected network access\n", stderr);
  abort();
}

static int draws, requests, failures;
static GLuint drawn, requested;
static const char *expected;
static GLuint auditRequest(const char *url) {
  GLuint t = tex_obter_palco(url);
  if (expected && url && !strcmp(url, expected)) { requests++; requested = t; }
  return t;
}
static GLuint auditHeroRequest(const char *url) {
  GLuint t = tex_obter_hero(url);
  if (expected && url && !strcmp(url, expected)) { requests++; requested = t; }
  return t;
}
static void auditRect(GfxRect r, GLuint tex, GfxModo mode, float f,
                      float x, float y, float radius, float red, float green, float blue, float alpha) {
  if (tex && alpha * gfx_opacidade_grupo > .004f &&
      (mode == GFX_VITRINE || mode == GFX_PALCO_ARTE)) {
    draws++; drawn = tex;
  }
  gfx_rect(r, tex, mode, f, x, y, radius, red, green, blue, alpha);
}
#define gfx_rect auditRect
#define tex_obter_palco auditRequest
#define tex_obter_hero auditHeroRequest
#ifdef PALCO_BASELINE
#include PALCO_BASELINE
#else
#include "../src/home.c"
#endif
#undef gfx_rect
#undef tex_obter_palco
#undef tex_obter_hero

static void check(int pass, const char *name) {
  printf("%s: %s\n", pass ? "PASS" : "FAIL", name);
  failures += !pass;
}
static void setting(int layout, int hero, int mode, int source) {
  char extra[240];
  snprintf(extra, sizeof extra,
           "socialLocal 1\nheroSectionEnabled %d\npalcoFundoLocal %d\n"
           "heroFundoLocal %d\nheroDifferentFromCard 1", hero ? 0 : 1, mode, source);
  setenv("NV_AJ", extra, 1);
  ajusta(layout, 0);
  assert(ajustes_palco_fundo() == mode && ajustes_hero_fonte() == source);
  assert(ajustes_hero_ligado() == hero && !ajustes_hero_arte_diferente());
}
static void capture(const char *dir, const char *phase, const char *state, int layout, int hero) {
  char path[1000];
  quadros(140, NULL);
  draws = requests = 0; drawn = requested = 0;
  snprintf(path, sizeof path, "%s/%s-%s-L%d-hero%d.bmp", dir, phase, state, layout, hero);
  quadros(1, path);
}

int main(int argc, char **argv) {
  assert(argc == 5);
  const char *dir = argv[1], *phase = argv[2];
  int layout = atoi(argv[3]), hero = atoi(argv[4]);
  static CatItem items[12];
  static CatFileira rows[2];
  dados_iniciar("deploy/app/art");
  dirDados = dados_dir();
  assert(dirDados && strstr(dirDados, "nuvio-homelayouts-shot"));
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  janela = SDL_CreateWindow("Cold Home backdrop regression", SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, 1920, 1080,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(janela);
  SDL_GLContext gl = SDL_GL_CreateContext(janela);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar() && txt_iniciar("deploy/app", 1));
  // Exercise full effects and minimum effects, independently of backdrop demand.
  gfx_definir_efeitos_minimos(!hero);
  tex_iniciar(192);
  gfx_borrao_iniciar(480, 270);
  artehero_definir_falhou(tex_falhou);
  gfx_icones_dir("deploy/app/art");
  setting(layout, hero, PALCO_ARTE, ARTEHERO_TMDB);
  assert(home_iniciar("deploy/app/art"));
  AddonRemoto addon = { .nome = "Offline fixture", .url = "http://127.0.0.1:1/manifest.json", .ativo = 1 };
  assert(addons_definir_lista(&addon, 1));
  for (int r = 0; r < 2; r++) {
    snprintf(rows[r].chave, sizeof rows[r].chave, "%s", r ? "stage_movie" : "pop_movie");
    snprintf(rows[r].titulo, sizeof rows[r].titulo, "%s", r ? "Trending - Movie" : "Popular - Movie");
    snprintf(rows[r].tipo, sizeof rows[r].tipo, "movie");
    rows[r].ini = r * 6; rows[r].n = 6;
  }
  for (int i = 0; i < 12; i++) {
    CatItem *it = &items[i];
    snprintf(it->imdb, sizeof it->imdb, "tt90%05d", i);
    snprintf(it->tipo, sizeof it->tipo, "movie");
    snprintf(it->titulo, sizeof it->titulo, "%s", NOMES[i]);
    snprintf(it->poster, sizeof it->poster, "deploy/app/art/poster/%02d.jpg", i);
    snprintf(it->logo, sizeof it->logo, "deploy/app/art/logo/%02d.png", i % 10);
    snprintf(it->backdropTmdb, sizeof it->backdropTmdb, "deploy/app/art/%02d.jpg", i);
    snprintf(it->backdropTrakt, sizeof it->backdropTrakt, "deploy/app/art/%02d.jpg", i + 17);
    snprintf(it->meta, sizeof it->meta, "2026 · 112 min");
    snprintf(it->sinopse, sizeof it->sinopse, "A quiet town discovers a mystery.");
    // Cold catalog: selected-source art exists; raw backdrop has not been enriched.
    it->nota = 70;
  }
  cat_definir_tudo(items, 12, rows, 2);
  quadros(1, NULL);
  assert(fil_definir_tipo("stage_movie", FIL_TIPO_PALCO));
  quadros(90, NULL);
  for (int i = 0; i < 8; i++) {
    if (!focoHero && fileiras[foco.fileira].tipo == FILEIRA_PALCO) break;
    tecla(SDLK_DOWN); quadros(30, NULL);
  }
  assert(!focoHero && fileiras[foco.fileira].tipo == FILEIRA_PALCO);
  const CatItem *focused = cat_item_exato(fileiraItemIndice(&fileiras[foco.fileira], foco.coluna));
  assert(focused);
  expected = focused->backdropTmdb;
  capture(dir, phase, "cold-home", layout, hero);
  check(requests > 0 && draws > 0 && drawn == requested,
        "Cold Home requests and draws selected backdrop without detail");
  check(!focused->backdrop[0], "Home does not require catalog enrichment");
  GLuint cold = drawn;

  setting(layout, hero, PALCO_PRETO, ARTEHERO_TMDB);
  quadros(90, NULL);
  setting(layout, hero, PALCO_ARTE, ARTEHERO_TRAKT);
  expected = focused->backdropTrakt;
  capture(dir, phase, "setting-change", layout, hero);
  check(requests > 0 && draws > 0 && drawn != cold && drawn == requested,
        "Background and source changes draw immediately without detail");

  // No usable background remains empty; a ready poster must never fill the screen.
  CatItem missing = *focused;
  snprintf(missing.imdb, sizeof missing.imdb, "custom:missing");
  missing.backdropTmdb[0] = missing.backdropTrakt[0] = 0;
  snprintf(missing.backdrop, sizeof missing.backdrop, "%s", missing.poster);
  check(!palcoArte(&missing), "Poster-only fallback remains excluded");
  printf("layout=%d hero=%d failures=%d\n", layout, hero, failures);
  tex_encerrar(); txt_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(janela); SDL_Quit();
  return failures ? 1 : 0;
}
