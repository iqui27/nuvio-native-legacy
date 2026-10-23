// Real source-sheet drawing and selection, without a window, fonts or GL setup.
#include "text.h"
#include "gfx.h"
#include <string.h>
// Keep layout deterministic while the real sheet executes every CPU branch.
static TxtLinha headlessLine(TxtEstilo e, const char *s, int r, int g, int b, int a) {
  (void)e; (void)r; (void)g; (void)b; (void)a;
  TxtLinha l = {0}; l.w = s ? (int)strlen(s) * 8 : 0; l.h = 24; return l;
}
static TxtLinha headlessCut(TxtEstilo e, const char *s, int r, int g, int b, int a, float w) {
  TxtLinha l = headlessLine(e, s, r, g, b, a); if (l.w > w) l.w = w; return l;
}
static float headlessBlock(TxtEstilo e, const char *s, int r, int g, int b,
    float x, float y, float w, float leading, float a, int lines) {
  (void)e; (void)r; (void)g; (void)b; (void)x; (void)y; (void)w; (void)a; (void)lines;
  return s && *s ? leading : 0;
}
#define txt_linha headlessLine
#define txt_linha_corta headlessCut
#define txt_bloco headlessBlock
#define txt_bloco_corta headlessBlock
#define txt_desenhar_alpha(...) ((void)0)
#define txt_desenhar_tom(...) ((void)0)
#define txt_tracking(e,s,r,g,b,x,y,a,t) ((float)strlen(s) * 8)
#define gfx_recorte(...) ((void)0)
#define gfx_sem_recorte() ((void)0)
#include "addons.h"
static unsigned addonRevision;
static unsigned testAddonRevision(void) { return addons_parcial_rev() + addonRevision; }
#define addons_parcial_rev testAddonRevision
#include "../src/streams.c"
#include "dados.h"
#include <assert.h>
#include <unistd.h>

static int counting, failed;
static unsigned queues, automaticReads;
void __attribute__((no_instrument_function)) __cyg_profile_func_enter(void *fn, void *caller) {
  (void)caller;
  if (counting && fn == (void *)filaDoPlay) queues++;
  if (counting && fn == (void *)automaticaDaFolha) automaticReads++;
}
void __attribute__((no_instrument_function)) __cyg_profile_func_exit(void *fn, void *caller) {
  (void)fn; (void)caller;
}
static void check(int ok, const char *message) {
  printf("%s: %s\n", ok ? "PASS" : "FAIL", message);
  if (!ok) failed = 1;
}
static void settings(const char *dir, int addonOrder) {
  char path[700]; snprintf(path, sizeof path, "%s/ajustes.txt", dir);
  FILE *f = fopen(path, "w"); assert(f);
  fprintf(f, "fonteTextoLocal 0\nfonteOrdemLocal %d\nvidroLocal 0\n", addonOrder);
  fclose(f); ajustes_dir(dir);
}
static void frame(void) {
  txt_novo_quadro();
  stream_folha_desenhar(SDL_GetTicks());
}
static void focusProof(void) {
  stream_preferir(24);
  stream_folha_abrir();
  int before = foco; float scroll = rolagem;
  assert(before > 0 && scroll > 0 && filtrado(before) == 24);
  stream_folha_servidor(-1);
  check(foco == before && rolagem == scroll && grupo == 1,
        "title sheet keeps automatic focus and scroll after server(-1)");
  // A valid server filter must locate the automatic source in its own rows.
  for (int i = 0; i < n; i++)
    snprintf(lista[i].bingeGroup, sizeof lista[i].bingeGroup, "%s", i % 2 ? "plex|fixture" : "jellyfin|fixture");
  atomic_fetch_add(&folhaRevisao, 1);
  stream_folha_abrir();
  stream_folha_servidor(1);
  check(nOrdem == n / 2 && foco > 0 && filtrado(foco) == 24 && rolagem > 0,
        "server filter focuses automatic source in filtered rows");
  for (int i = 0; i < n; i++) lista[i].bingeGroup[0] = 0;
  atomic_fetch_add(&folhaRevisao, 1);
  stream_preferir(-1);
}
static void cacheProof(void) {
  stream_folha_abrir();
  int best = stream_play_candidata();
  counting = 1; queues = 0;
  for (int i = 0; i < 60; i++) assert(stream_play_candidata() == best);
  counting = 0;
  check(queues == 0, "unchanged candidate never rebuilds Play queue");
  addonRevision++;
  counting = 1; queues = 0;
  assert(stream_play_candidata() == best);
  assert(stream_play_candidata() == best);
  counting = 0;
  check(queues == 1, "addon tab revision invalidates candidate once");
  stream_preferir(24);
  check(stream_play_candidata() == 24, "preferred source invalidates candidate");
  stream_automatico_excluir(24);
  check(stream_play_candidata() != 24, "worker revision invalidates candidate");
  stream_preferir(-1);
  assert(fontepref_guardar("tt0021210", &lista[26]));
  check(stream_play_candidata() == 26, "saved preference revision invalidates candidate");
  Stream extra = lista[0];
  stream_lista_acrescentar(&extra, 1, 0);
  counting = 1; queues = 0;
  (void)stream_play_candidata(); counting = 0;
  check(queues == 1, "appending sources invalidates candidate once");
}
static void benchmark(int aio) {
  AddonRemoto addon = { .ativo = 1 };
  snprintf(addon.nome, sizeof addon.nome, "%s", aio ? "AIOStreams" : "Fixture");
  snprintf(addon.url, sizeof addon.url, "https://fixture.invalid/%d/manifest.json", aio);
  addons_definir_lista(&addon, 1);
  assert(!!temAio() == aio);
  stream_folha_abrir(); anim = 1;
  for (int i = 0; i < 3; i++) frame();
  queues = automaticReads = 0; counting = 1;
  Uint64 start = SDL_GetPerformanceCounter();
  for (int i = 0; i < 60; i++) frame();
  double ms = (SDL_GetPerformanceCounter() - start) * 1000.0 / SDL_GetPerformanceFrequency() / 60;
  counting = 0;
  printf("3000 sources, 60 frames, aio=%d: %.3f ms/frame, queues=%u, automatic reads=%u\n",
         aio, ms, queues, automaticReads);
  check(ms < 2, "headless source sheet stays below 2 ms/frame");
  check(queues == 0, "steady drawing never rebuilds Play queue");
  check(automaticReads == 60, "automatic source read once per drawn frame");
}
int main(void) {
  char dir[] = "/tmp/nuvio-streams-cache-XXXXXX";
  assert(mkdtemp(dir)); assert(!setenv("NUVIO_DADOS", dir, 1));
  dados_iniciar(dir); settings(dir, 1);
  gfx_opacidade_grupo = 0; // Execute production drawing; suppress GPU submissions.
  assert(!SDL_Init(SDL_INIT_TIMER));
  stream_definir_alvo("tt0021210");
  Stream *sources = calloc(3000, sizeof *sources); assert(sources);
  for (int i = 0; i < 3000; i++) {
    sources[i].altura = 1080; sources[i].mp4 = 1; sources[i].fileIdx = -1;
    snprintf(sources[i].url, sizeof sources[i].url, "https://fixture.invalid/%d.mp4", i);
    snprintf(sources[i].provedor, sizeof sources[i].provedor, "AIOStreams");
    snprintf(sources[i].rotulo, sizeof sources[i].rotulo, "Source %d", i);
  }
  stream_definir_lista(sources, 3000); free(sources);
  focusProof();
  cacheProof();
  // Restore exactly 3000 sources after the append invalidation check.
  Stream *copy = malloc(3000 * sizeof *copy); assert(copy);
  memcpy(copy, lista, 3000 * sizeof *copy);
  stream_definir_lista(copy, 3000); free(copy);
  benchmark(0); benchmark(1);
  stream_definir_lista(NULL, 0); SDL_Quit();
  return failed;
}
