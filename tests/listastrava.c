// Profile real Library update/event handlers with an Android-sized library.
#define NV_ANDROID
#include "catalogo.h"
#undef NV_ANDROID
const CatItem *perf_item(int i);
int perf_n(void);
int perf_find(const char *id);
#define cat_item perf_item
#define cat_n perf_n
#define cat_indice_por_imdb perf_find
#define lst_pedir sources_request
#define sync_ultimo_ok sources_sync_ok
#define main capture_main
#include "listasux_shot.c"
#undef main
void sources_request(int source) { (void)source; }
unsigned sources_sync_ok(void) { return 1; }
static CatItem *large;
const CatItem *perf_item(int i) { return i >= 0 && i < 4000 ? &large[i] : NULL; }
int perf_n(void) { return large ? 4000 : 0; }
int perf_find(const char *id) {
  int i = -1; if (sscanf(id, "tt%d", &i) == 1 && i >= 0 && i < 4000) return i;
  return -1;
}
static double elapsed(Uint64 start) { return (SDL_GetPerformanceCounter() - start) * 1000.0 / SDL_GetPerformanceFrequency(); }
int main(int argc, char **argv) {
  assert(argc == 2);
  large = calloc(4000, sizeof *large); assert(large);
  for (int i = 0; i < 4000; i++) {
    snprintf(large[i].imdb, sizeof large[i].imdb, "tt%07d", i);
    snprintf(large[i].titulo, sizeof large[i].titulo, "Title %04d", i);
    strcpy(large[i].tipo, "movie"); strcpy(large[i].origem, "fixture-addon");
    large[i].naLista = 1; large[i].fontes = CAT_FONTE_TRAKT | CAT_FONTE_NUVIO;
  }
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  window = SDL_CreateWindow("Lists freeze regression", 0, 0, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(window && SDL_GL_CreateContext(window)); SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1)); tex_iniciar(64); gfx_icones_dir("deploy/app/art");
  dados_iniciar(getenv("NUVIO_DADOS"));
  setenv("NUVIO_RAIL", "classica", 1); setenv("NUVIO_SHOT_PT", "1", 1); rail_shot_aplicar();
  biblioteca_iniciar();
  assert(contaModo[MODO_PARAVER] == 4000 && nCelulas == 4000);
  double worst = 0;
  for (int m = 0; m < BIB_N_MODOS; m++) {
    foco.fileira = BIB_FIL_MODO; foco.coluna = m;
    Uint64 t = SDL_GetPerformanceCounter(); key(SDLK_RETURN); double ms = elapsed(t);
    printf("4000 items tab=%d event=%.3fms\n", m, ms); if (ms > worst) worst = ms;
    for (int s = 0; s < BIB_N_ORIG && m != MODO_LISTAS; s++) {
      focoOrigem = origensAbertas = 1; origemSel = s;
      t = SDL_GetPerformanceCounter(); key(SDLK_RETURN); ms = elapsed(t);
      printf("4000 items tab=%d source=%d event=%.3fms\n", m, s, ms); if (ms > worst) worst = ms;
    }
    for (int frame = 0; frame < 3; frame++) {
      t = SDL_GetPerformanceCounter(); biblioteca_atualizar(1.f / 60, SDL_GetTicks()); ms = elapsed(t);
      printf("4000 items tab=%d update[%d]=%.3fms\n", m, frame, ms); if (ms > worst) worst = ms;
    }
  }
  modo = MODO_LISTAS; listaTab = 0; fonte = FONTE_TRAKT; remapear(0);
  foco.fileira = BIB_FIL_MODO; foco.coluna = MODO_LISTAS;
  shot(argv[1], "lists-header");
  printf("4000 items worst=%.3fms budget=16ms %s\n", worst, worst < 16 ? "PASS" : "FAIL");
  return worst < 16 ? 0 : 1;
}
