// Orcamento do vidro por quadro (a ideia de widgets_glass_budget_tests.rs do
// PlxNative: as superficies de vidro de um quadro convergem numa captura so).
// A barra do topo (trilha + circulo do perfil), a ilha do relogio e um painel
// de vidro, com a pagina por baixo MUDANDO a cada quadro (rolagem). The
// glasses read only the backdrop layer (docs/fosco-nova.md): rows scrolling
// over a static backdrop cost no capture; a changing backdrop costs ONE
// capture and its 5 passes per frame. With nothing moving, none.
//   bash tests/vidro_orcamento.sh
#include "gfx.h"
#include "ajustes.h"
#include "dados.h"
#include "ilha.h"
#include "menu.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define PASSADAS_POR_CAPTURA 5   // 1/2, 1/4, dois Kawase, de volta a 1/2 (vidCapturar)

static const int LW = 1920, LH = 1080;
static int falhas;

static void confere(int ok, const char *nome) {
  printf("%s: %s\n", ok ? "PASSA" : "FALHA", nome);
  falhas += !ok;
}

// Um quadro da home: a pagina (um degrade que desce `rola` px), a barra, a
// ilha e um painel de vidro por cima da pagina.
static float fundo = 0.05f;   // the backdrop layer: a full-screen color
static void quadro(float rola, Uint32 agora) {
  int i;
  gfx_novo_quadro();
  glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
  gfx_cor((GfxRect){ 0, 0, 1920, 1080 }, 0, fundo, fundo, fundo * 1.2f, 1.0f);
  for (i = 0; i < 12; i++)
    gfx_cor((GfxRect){ 0, i * 90.0f - rola, 1920, 90 }, 0, .1f + .06f * i, .2f, .5f - .03f * i, 1.0f);
  menu_atualizar(0.016f, agora);
  menu_desenhar(agora);
  ilha_desenhar(agora);
  gfx_vidro_painel((GfxRect){ 1240, 300, 560, 600 }, 36.0f / 600.0f, 0.80f, 1.0f);
}

int main(void) {
  static char dir[] = "/tmp/nuvio-vidro-orc-XXXXXX";
  char cam[700];
  FILE *f;
  int k, cap, pas;
  Uint64 t0;
  GLuint fbo, tex;
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("vidro", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN); assert(w);
  assert(SDL_GL_CreateContext(w));
  glGenTextures(1, &tex); glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, LW, LH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, LW, LH); gfx_tamanho_alvo(LW, LH); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1)); tex_iniciar(64);

  assert(mkdtemp(dir));
  assert(setenv("NUVIO_DADOS", dir, 1) == 0);
  dados_iniciar(dir);
  snprintf(cam, sizeof cam, "%s/ajustes.txt", dir);
  f = fopen(cam, "w"); assert(f);
  fputs("homeLayoutLocal 0\nmodernSidebar 0\nmenuTopoLocal 1\nidioma 0\n", f); fclose(f);
  ajustes_dir(dir);
  setenv("NUVIO_SHOT_VIDRO_FOSCO", "1", 1); ajustes_teste_vidro_env();
  assert(ajustes_vidro() && ajustes_vidro_fosco() && ajustes_menu_topo());
  menu_iniciar();
  menu_pilula_mostrar(1.0f);
  ilha_relogio_visivel(1);

  // Aquece: o primeiro quadro nao sabe onde estarao os vidros seguintes, e as
  // molas da barra (alfa da capsula) assentam em ~2 s.
  for (k = 0; k < 150; k++) quadro(0.0f, 16u * k);

  // Rolando: the rows change every frame, the backdrop does not: no capture.
  // O ms (com glFinish) so informa: a GPU do Mac nao e a da TV.
  cap = pas = 0;
  glFinish(); t0 = SDL_GetPerformanceCounter();
  for (k = 0; k < 20; k++) {
    quadro(4.0f * (k + 1), 16u * (150 + k));
    if (gfx_vidro_capturas > cap) cap = gfx_vidro_capturas;
    if (gfx_vidro_passadas > pas) pas = gfx_vidro_passadas;
  }
  glFinish();
  printf("rolando: ate %d capturas e %d passadas por quadro, %.2f ms/quadro\n", cap, pas,
         (double)(SDL_GetPerformanceCounter() - t0) * 1000.0 / SDL_GetPerformanceFrequency() / 20.0);
  confere(cap == 0 && pas == 0, "rows scrolling over a static backdrop: no capture");

  // The backdrop itself changes every frame (hero crossfade): ONE capture.
  cap = pas = 0;
  for (k = 0; k < 20; k++) {
    fundo = 0.05f + 0.01f * (k + 1);
    quadro(80.0f, 16u * (170 + k));
    if (gfx_vidro_capturas > cap) cap = gfx_vidro_capturas;
    if (gfx_vidro_passadas > pas) pas = gfx_vidro_passadas;
  }
  printf("fundo mudando: ate %d capturas e %d passadas por quadro\n", cap, pas);
  confere(cap == 1, "barra, perfil, ilha e painel dividem uma captura por quadro");
  confere(pas <= PASSADAS_POR_CAPTURA, "no maximo as 5 passadas de uma captura");

  // Parada: nada atras mudou, o backdrop desfocado do quadro anterior serve.
  quadro(80.0f, 16u * 190);
  cap = pas = 0;
  for (k = 0; k < 5; k++) {
    quadro(80.0f, 16u * 190);
    cap += gfx_vidro_capturas; pas += gfx_vidro_passadas;
  }
  printf("parada: %d capturas, %d passadas em 5 quadros\n", cap, pas);
  confere(cap == 0 && pas == 0, "pagina parada: zero passadas de desfoque");

  unlink(cam);
  { char t[700]; snprintf(t, sizeof t, "%s/ajustes.tmp", dir); unlink(t); }
  printf("vidro_orcamento: %s\n", falhas ? "FALHA" : "ok");
  return falhas ? 1 : 0;
}
