// #579: interrupt horizontal scrolling with vertical navigation. The old
// row must finish its spring instead of freezing with a nonzero velocity
// (which also permanently disables the Home's idle artwork lookahead).
#include "../src/home.c"
#include <SDL2/SDL_image.h>
#include <assert.h>

static SDL_Window *window;
static Uint32 now = 1000;
static int failures;

static void key(SDL_Keycode code) {
  SDL_Event event = {.type = SDL_KEYDOWN};
  event.key.keysym.sym = code;
  home_evento(&event);
  event.type = SDL_KEYUP;
  home_evento(&event);
}

static void frames(int count) {
  for (int i = 0; i < count; i++) {
    now += 16;
    SDL_PumpEvents();
    tex_bombear(16);
    tex_novo_quadro(); txt_novo_quadro(); gfx_novo_quadro();
    gfx_sem_recorte();
    glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    home_atualizar(1.f / 60, now);
    home_desenhar(now);
    gfx_ambiente_descarregar();
  }
}

static void shot(const char *prefix, int layout, const char *phase) {
  char path[1024];
  unsigned char *pixels = malloc(1920 * 1080 * 4);
  SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(pixels && surface);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  for (int y = 0; y < 1080; y++)
    memcpy((char *)surface->pixels + y * surface->pitch,
           pixels + (size_t)(1079 - y) * 1920 * 4, 1920 * 4);
  snprintf(path, sizeof path, "%s-L%d-%s.png", prefix, layout, phase);
  assert(IMG_SavePNG(surface, path) == 0);
  SDL_FreeSurface(surface);
  free(pixels);
}

static float scrollTarget(void) {
  TXT_INI();
  float target = alvoScrollFil(0, foco.coluna, scrollX[0], 0);
  TXT_FIM();
  return target;
}

static void check(int ok, const char *label, int layout) {
  printf("%s L%d: %s\n", ok ? "PASS" : "FAIL", layout, label);
  failures += !ok;
}

int main(int argc, char **argv) {
  assert(argc == 2);
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  window = SDL_CreateWindow("Home scroll #579", 0, 0, 1920, 1080,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(window && SDL_GL_CreateContext(window));
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar() && txt_iniciar("deploy/app", 1));
  dados_iniciar("deploy/app/art");
  tex_iniciar(64); gfx_icones_dir("deploy/app/art");
  artehero_definir_falhou(tex_falhou);
  assert(home_iniciar("deploy/app/art"));
  AddonRemoto addon = {.nome = "Offline", .url = "local", .ativo = 1};
  assert(addons_definir_lista(&addon, 1));
  enum { ROWS = 3, ITEMS = 24 };
  static CatItem items[ROWS * ITEMS];
  static CatFileira rows[ROWS];
  for (int r = 0; r < ROWS; r++) {
    snprintf(rows[r].chave, sizeof rows[r].chave, "scroll_%d_movie", r);
    snprintf(rows[r].titulo, sizeof rows[r].titulo, "Catalogue %d", r + 1);
    snprintf(rows[r].tipo, sizeof rows[r].tipo, "movie");
    rows[r].ini = r * ITEMS; rows[r].n = ITEMS;
    fil_registrar(rows[r].chave, rows[r].titulo, "Offline", "Filmes", ITEMS);
    assert(fil_definir_tipo(rows[r].chave, FIL_TIPO_CARTAZ));
  }
  for (int i = 0; i < ROWS * ITEMS; i++) {
    snprintf(items[i].imdb, sizeof items[i].imdb, "tt90%05d", i);
    snprintf(items[i].tipo, sizeof items[i].tipo, "movie");
    snprintf(items[i].titulo, sizeof items[i].titulo, "Offline movie %d", i + 1);
    snprintf(items[i].sinopse, sizeof items[i].sinopse, "Local catalogue scroll regression fixture.");
    snprintf(items[i].poster, sizeof items[i].poster, "deploy/app/art/poster/%02d.jpg", i % 8);
    snprintf(items[i].backdrop, sizeof items[i].backdrop, "deploy/app/art/%02d.jpg", i % 8);
  }
  cat_definir_tudo(items, ROWS * ITEMS, rows, ROWS);
  for (int layout = 0; layout < HOME_LAYOUT_N; layout++) {
    char path[1024];
    snprintf(path, sizeof path, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
    FILE *file = fopen(path, "w"); assert(file);
    fprintf(file, "homeLayoutLocal %d\nvidroLocal 1\nanimacoes 0\n"
                  "focusedPosterBackdropExpandEnabled 1\ntrailerHero 1\nsocialLocal 1\nidioma 0\n", layout);
    fclose(file); ajustes_dir(getenv("NUVIO_DADOS"));
    home_ir_topo(); frames(120);
    key(SDLK_DOWN); frames(90);
    assert(nFileiras == ROWS && foco.fileira == 0 && !focoHero);
    for (int i = 0; i < 12; i++) { key(SDLK_RIGHT); frames(2); }
    printf("L%d: rapid right row=%d column=%d type=%d x=%.2f velocity=%.2f\n",
           layout, foco.fileira, foco.coluna, fileiras[0].tipo, scrollX[0], velX[0]);
    fflush(stdout);
    assert(foco.coluna >= 10 && velX[0] > 120);
    int remembered = foco.coluna;
    float target = scrollTarget();
    assert(target - scrollX[0] > 10);
    key(SDLK_DOWN);
    assert(foco.fileira == 1 && foco.colunaLembrada[0] == remembered);
    Uint64 start = SDL_GetPerformanceCounter();
    frames(180);
    printf("L%d: interrupted row x=%.2f target=%.2f velocity=%.2f; %.2f ms/frame\n",
           layout, scrollX[0], target, velX[0],
           (SDL_GetPerformanceCounter() - start) * 1000.0 / SDL_GetPerformanceFrequency() / 180);
    check(fabsf(scrollX[0] - target) < .25f && fabsf(velX[0]) < .25f,
          "departed row settles at remembered column; idle lookahead unblocked", layout);
    shot(argv[1], layout, "departed");
    key(SDLK_UP); frames(18);
    shot(argv[1], layout, "return");
    // Interrupt in the opposite direction too, then alternate rows rapidly.
    frames(90);
    for (int i = 0; i < 7; i++) { key(SDLK_LEFT); frames(2); }
    target = scrollTarget();
    key(SDLK_DOWN); frames(1); key(SDLK_UP); frames(1); key(SDLK_DOWN);
    frames(180);
    check(fabsf(scrollX[0] - target) < .25f && fabsf(velX[0]) < .25f,
          "left scroll survives rapid row switches", layout);
  }
  tex_encerrar(); SDL_Quit();
  return failures ? 1 : 0;
}
