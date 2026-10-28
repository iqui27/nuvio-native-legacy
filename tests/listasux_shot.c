// Deterministic native screenshots. Real drawing, real spring frames, local posters.
#ifndef BIB_SOURCE
#define BIB_SOURCE "../src/biblioteca.c"
#endif
#ifndef SHOT_PHASE
#define SHOT_PHASE "after"
#endif
#include BIB_SOURCE
#include "rail_shot.h"
#include <SDL2/SDL_image.h>
#include <assert.h>
extern void listasux_trails_fixture(void);
static SDL_Window *window;
static void key(SDL_Keycode code) {
  SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = code;
  biblioteca_evento(&e);
}
static char *fixture(const char *path) {
  FILE *f = fopen(path, "rb"); assert(f); fseek(f, 0, SEEK_END);
  long n = ftell(f); rewind(f); char *s = calloc((size_t)n + 1, 1);
  assert(fread(s, 1, (size_t)n, f) == (size_t)n); fclose(f); return s;
}
static void posters(void) {
  CatItem v[4] = {0};
  for (int j = 0; j < 4; j++) snprintf(v[j].poster, sizeof v[j].poster, "deploy/app/art/poster/%02d.jpg", 18 + j);
  for (int i = 0; i < lst_n(); i++) { LstLista l = *lst_lista(i); lst_previa_registrar(&l, v, 4); }
}
static void shot(const char *dir, const char *name) {
  for (int i = 0; i < 100; i++) {
    SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(8);
    biblioteca_atualizar(1.f / 60, SDL_GetTicks());
    glClearColor(.05f, .05f, .05f, 1); glClear(GL_COLOR_BUFFER_BIT);
    biblioteca_desenhar(SDL_GetTicks()); rail_shot_desenhar(MENU_BIBLIOTECA);
    SDL_GL_SwapWindow(window); SDL_Delay(5);
  }
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  glReadBuffer(GL_FRONT); glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, s->pixels);
  unsigned char *row = malloc((size_t)s->pitch);
  for (int y = 0; y < 540; y++) {
    unsigned char *a = (unsigned char *)s->pixels + y * s->pitch;
    unsigned char *b = (unsigned char *)s->pixels + (1079-y) * s->pitch;
    memcpy(row,a,s->pitch); memcpy(a,b,s->pitch); memcpy(b,row,s->pitch);
  }
  free(row); char path[768]; snprintf(path,sizeof path,"%s/%s-%s.png",dir,name,SHOT_PHASE);
  assert(IMG_SavePNG(s,path)==0); SDL_FreeSurface(s);
}
int main(int argc, char **argv) {
  assert(argc == 2 && getenv("NUVIO_DADOS"));
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER)==0); IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  window=SDL_CreateWindow("Lists proof",0,0,1920,1080,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN); assert(window);
  assert(SDL_GL_CreateContext(window)); SDL_GL_SetSwapInterval(0);
  glViewport(0,0,1920,1080); gfx_tamanho_alvo(1920,1080); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app",1)); tex_iniciar(64); gfx_icones_dir("deploy/app/art");
  dados_iniciar(getenv("NUVIO_DADOS"));
  setenv("NUVIO_RAIL","dinamica",1); setenv("NUVIO_SHOT_EN","1",1); setenv("NUVIO_SHOT_RELOGIO","1",1);
  rail_shot_aplicar();
  if (strcmp(ajustes_idioma_nome(), "English")) ajustes_idioma_passo(1);
  biblioteca_iniciar();
  modo=MODO_LISTAS; listaTab=0; fonte=FONTE_TRAKT; origensAbertas=0; animOrig=0;
  char *s=fixture("tests/fixtures/trakt_minhas_listas.json"); lst_ler_trakt(s,0); free(s);
  remapear(0); foco.fileira=BIB_FIL_PICK; foco.coluna=pickSel=0; posters(); shot(argv[1],"my-lists");
  s=fixture("tests/fixtures/trakt_busca_listas.json"); lst_ler_trakt(s,1); free(s);
  listaTab=2; fonte=FONTE_PUB; exibicao=VIS_CARTAZ; remapear(0);
  foco.fileira=BIB_FIL_PICK; foco.coluna=pickSel=2; posters(); shot(argv[1],"explore");
  listasux_trails_fixture(); listaTab=3; lst_pedir_trilhas(); remapear(0);
  foco.fileira=BIB_FIL_PICK; foco.coluna=pickSel=3; posters(); shot(argv[1],"trails");
#ifdef LISTASUX_BEFORE
  origensAbertas=1; focoOrigem=1; origemSel=0; shot(argv[1],"sources");
#else
  focoBotaoOrig=1; foco.fileira=BIB_FIL_PICK; key(SDLK_RETURN); shot(argv[1],"sources");
  key(SDLK_DOWN); assert(focoOrigem); key(SDLK_ESCAPE);
  assert(!origensAbertas && focoBotaoOrig && modo==MODO_LISTAS);
#endif
  puts("Native screenshots and rendered navigation: PASS"); return 0;
}
