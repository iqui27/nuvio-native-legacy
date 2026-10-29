// Native 1080p design exploration. Private switch exists only in this binary.
#define NV_FICHA_EXPLORATION 1
#include "../src/home.c"
#include "nvtmp.h"
#include "gl_compat.h"
#include <SDL2/SDL_image.h>
#include <assert.h>

static SDL_Window *window;
static const char *output;
static void capture(const char *name) {
  char path[1024];
  unsigned char *pixels = malloc(1920 * 1080 * 4);
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(pixels && s);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  for (int y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pixels + (1079 - y) * 1920 * 4, 1920 * 4);
  snprintf(path, sizeof path, "%s/%s.png", output, name);
  assert(IMG_SavePNG(s, path) == 0);
  SDL_FreeSurface(s); free(pixels);
}
static void frames(int n) {
  for (int i = 0; i < n; i++) {
    Uint32 now = SDL_GetTicks();
    SDL_PumpEvents(); tex_bombear(16);
    home_atualizar(1.0f / 60, now);
    corviva_quadro(1.0f / 60, ajustes_cor_viva(), ajustes_cor_logo(), 0);
    txt_novo_quadro(); tex_novo_quadro(); gfx_novo_quadro(); gfx_ambiente_preparar();
    glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    gfx_ambiente(1); home_desenhar(now);
    SDL_GL_SwapWindow(window); SDL_Delay(8);
  }
}
// Draw and capture before swap; reading a swapped back buffer is undefined.
static void shot(const char *name) {
  txt_novo_quadro(); tex_novo_quadro(); gfx_novo_quadro(); gfx_ambiente_preparar();
  glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1);
  glClear(GL_COLOR_BUFFER_BIT); gfx_ambiente(1); home_desenhar(SDL_GetTicks());
  capture(name);
}
static void key(SDL_Keycode k) {
  SDL_Event e = {0};
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k; home_evento(&e);
  e.type = SDL_KEYUP; home_evento(&e);
}
static void focusRow(void) {
  for (int i = 0; i < 14; i++) key(SDLK_UP);
  for (int i = 0; i < 10 && !strstr(home_rastro_foco(), "Top Rated"); i++) {
    key(SDLK_DOWN); frames(4);
  }
  assert(strstr(home_rastro_foco(), "Top Rated"));
  frames(120);
}
int main(int argc, char **argv) {
  static CatItem items[12]; static CatFileira rows[2];
  assert(argc == 2); output = argv[1];
  dados_iniciar("deploy/app/art");
  assert(strstr(dados_dir(), "nuvio-fileirarepensar-shot"));
  char path[1024]; snprintf(path, sizeof path, "%s/ajustes.txt", dados_dir());
  FILE *settings = fopen(path, "w"); assert(settings);
  fprintf(settings, "idioma 1\nhomeLayoutLocal 0\nvidroLocal 0\nfonteInterface 0\n"
                    "modernLandscapePostersEnabled 1\nmodernHeroFullScreenBackdropEnabled 1\n"
                    "selected_theme 0\ntrailerHero 1\nsocialLocal 1\nbordaFocoCartaz 1\n");
  fclose(settings); ajustes_dir(dados_dir()); ajustes_definir_vidro(1);
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  window = SDL_CreateWindow("Nuvio row exploration", SDL_WINDOWPOS_CENTERED,
           SDL_WINDOWPOS_CENTERED, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(window && SDL_GL_CreateContext(window)); SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar()); assert(txt_iniciar("deploy/app", 1));
  gfx_escala_txt_definir(1.0f); // TV reference canvas, no desktop auto scaling.
  tex_iniciar(192); gfx_borrao_iniciar(480, 270);
  snprintf(path, sizeof path, "%s/cache", dados_dir()); tex_cache_dir(path);
  gfx_icones_dir("deploy/app/art"); artehero_definir_falhou(tex_falhou);
  artehero_fundo_addon(ajustes_fundo_addon());
  posterprov_preferir_addon(ajustes_poster_addon());
  assert(home_iniciar("deploy/app/art"));
  AddonRemoto addon = {.nome="Fixture", .url="http://127.0.0.1:1/manifest.json", .ativo=1};
  assert(addons_definir_lista(&addon, 1));
#include "fixtures/fileirarepensar/catalog.inc"
  // Exact copy visible in the owner's reference, rather than today's synopsis.
  snprintf(items[0].sinopse, sizeof items[0].sinopse,
    "The story of New Jersey-based Italian-American mobster Tony Soprano and the difficulties "
    "he faces as he tries to balance the conflicting requirements of his home life and the criminal organization he heads.");
  for (int i = 0; i < 2; i++) {
    snprintf(rows[i].chave, sizeof rows[i].chave, i ? "study.a24" : "study.top_rated");
    snprintf(rows[i].titulo, sizeof rows[i].titulo, i ? "A24" : "Top Rated");
    snprintf(rows[i].tipo, sizeof rows[i].tipo, "series");
    rows[i].ini = 0; rows[i].n = 12;
  }
  cat_definir_tudo(items, 12, rows, 2);
  frames(60); // Register the published catalog before choosing its style.
  assert(fil_definir_tipo("study.top_rated", FIL_TIPO_FICHA));
  assert(fil_definir_tipo("study.a24", FIL_TIPO_CARTAZ));
  frames(120); focusRow();
  assert(foco.coluna == 0 && fileiras[foco.fileira].tipo == FILEIRA_FICHA);
  shot("current");
  for (int variant = 1; variant <= 3; variant++) {
    fichaExploration = variant;
    // Rebuild geometry through the same home invalidation path as row styles.
    fil_definir_tipo("study.top_rated", FIL_TIPO_CARTAZ); frames(1);
    fil_definir_tipo("study.top_rated", FIL_TIPO_FICHA); frames(1);
    focusRow();
    key(SDLK_RIGHT); frames(120);
    assert(foco.coluna == 1);
    char name[64]; snprintf(name, sizeof name, "v%d-frame0", variant); shot(name);
    key(SDLK_LEFT); frames(2);
    assert(foco.coluna == 0);
    snprintf(name, sizeof name, "v%d-frame1", variant); shot(name);
    printf("[study] v%d midpoint focus=%.3f\n", variant, animFoco[foco.fileira][0]);
    frames(118); // Full spring and hero transition finish; never truncate motion.
    assert(animFoco[foco.fileira][0] > .999f);
    GfxRect b; assert(home_ancora_foco(&b));
    assert(b.x >= 0 && b.y >= 0 && b.x+b.w <= 1920 && b.y+b.h <= 1080);
    snprintf(name, sizeof name, "v%d", variant); shot(name);
    printf("[study] v%d settled %.0f,%.0f %.0fx%.0f\n", variant, b.x,b.y,b.w,b.h);
  }
  SDL_Quit(); return 0;
}
