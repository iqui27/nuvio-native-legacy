// "Qualidade do vidro": Alta / Desempenho / Automatica and its governor.
//   bash tests/vidro_qualidade.sh
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

static const int LW = 1920, LH = 1080;
static int falhas;

static void confere(int ok, const char *nome) {
  printf("%s: %s\n", ok ? "PASSA" : "FALHA", nome);
  falhas += !ok;
}

// Home frame: a page under a bar, the clock island and a glass panel, the
// backdrop colour changing every frame so frosted glass has to capture.
static float fundo = 0.05f;
static void quadro(Uint32 agora) {
  int i;
  gfx_novo_quadro();
  glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
  gfx_vidro_sujar();   // changing art: forces the backdrop hash off last frame's
  gfx_cor((GfxRect){ 0, 0, 1920, 1080 }, 0, fundo, fundo, fundo * 1.2f, 1.0f);
  for (i = 0; i < 12; i++)
    gfx_cor((GfxRect){ 0, i * 90.0f - 80.0f, 1920, 90 }, 0, .1f + .06f * i, .2f, .5f - .03f * i, 1.0f);
  menu_atualizar(0.016f, agora);
  menu_desenhar(agora);
  ilha_desenhar(agora);
  gfx_vidro_ou_solido((GfxRect){ 1240, 300, 560, 600 }, 36.0f / 600.0f, 0.80f, 1.0f, .12f, .13f, .15f, .95f);
}

// Frames with a changing backdrop; returns the worst capture/pass counts and
// totals the frames' frosted / smoked glasses.
static int cap, pas, fonte, chapado;
static double copiatot;
static void roda(int n, Uint32 *t) {
  int k;
  cap = pas = fonte = chapado = 0; copiatot = 0.0;
  for (k = 0; k < n; k++) {
    fundo = 0.05f + 0.01f * ((k % 20) + 1);
    quadro((*t)++ * 16u);
    if (gfx_vidro_capturas > cap) cap = gfx_vidro_capturas;
    copiatot += gfx_vidro_copia;
    if (gfx_vidro_passadas > pas) pas = gfx_vidro_passadas;
    fonte += gfx_vidro_com_fonte; chapado += gfx_vidro_chapados;
  }
}

// One window of the governor: GOV_JANELA frames, each `ms` long, target 16.7.
static void janela(double ms) { int i; for (i = 0; i < 150; i++) gfx_vidro_governar(ms, 16.7); }

// The workload the next governor windows see: n glasses of `area` px^2; in a
// trial the glass is flat, in the baseline it is frosted.
static void cena(int n, int area, int prova) {
  gfx_vidro_com_fonte = prova ? 0 : n; gfx_vidro_chapados = prova ? n : 0; gfx_vidro_area = area;
}
// One window that alternates two scenes (every frame differs from the last).
static void janela_mista(double ms, int n1, int n2) {
  int i;
  for (i = 0; i < 150; i++) { cena(i & 1 ? n1 : n2, 900000, 0); gfx_vidro_governar(ms, 16.7); }
}

int main(void) {
  static char dir[] = "/tmp/nuvio-vidro-qual-XXXXXX";
  char cam[700];
  FILE *f;
  int i;
  Uint32 t = 0;
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
  setenv("NUVIO_SHOT_VIDRO_FOSCO", "1", 1);

  menu_iniciar();
  menu_pilula_mostrar(1.0f);
  ilha_relogio_visivel(1);

  // Alta: the current frost, a capture per frame while the backdrop changes.
  setenv("NUVIO_SHOT_VIDRO_QUAL", "2", 1); ajustes_teste_vidro_env();
  assert(ajustes_vidro() && ajustes_vidro_fosco() && ajustes_vidro_qualidade() == 2);
  roda(150, &t); roda(20, &t);
  printf("Alta: cap<=%d pas<=%d fonte=%d chapado=%d\n", cap, pas, fonte, chapado);
  confere(cap == 1 && fonte > 0, "Alta: frost unchanged (one capture per frame)");

  // Desempenho: smoked glass everywhere, no capture and no blur pass, same
  // frames and same backdrop.
  setenv("NUVIO_SHOT_VIDRO_QUAL", "1", 1); ajustes_teste_vidro_env();
  assert(ajustes_vidro_qualidade() == 1);
  roda(150, &t); roda(20, &t);
  printf("Desempenho: cap<=%d pas<=%d fonte=%d chapado=%d\n", cap, pas, fonte, chapado);
  confere(cap == 0 && pas == 0 && fonte == 0 && chapado > 0, "Desempenho: Home draws no blur (smoked glass only)");

  // Automatica: frost until the governor trips. 3 janky windows are not
  // enough, and a clean window in between resets the count.
  setenv("NUVIO_SHOT_VIDRO_QUAL", "0", 1); ajustes_teste_vidro_env();
  assert(ajustes_vidro_qualidade() == 0);
  roda(150, &t); roda(20, &t);
  confere(cap == 1 && fonte > 0 && !gfx_vidro_degradado(), "Automatica: frost at first");
  for (i = 0; i < 3; i++) janela(40.0);
  janela(16.7);
  for (i = 0; i < 3; i++) janela(40.0);
  confere(!gfx_vidro_degradado(), "governor: 3 janky windows, a clean one, 3 more: no drop");
  janela(20.0);   // 20 ms < 1.5 x 16.7: not janky either
  for (i = 0; i < 3; i++) janela(40.0);
  confere(!gfx_vidro_degradado(), "governor: a window of normal frames resets the count");
  janela(40.0);   // the 4th janky window in a row: starts the trial
  confere(gfx_vidro_degradado(), "governor: 4 janky windows in a row start the smoked trial");
  roda(1, &t);   // the switch frame may still run the capture that was queued before
  roda(20, &t);
  janela(8.0);   // the trial window is much faster: the drop stays
  confere(gfx_vidro_degradado(), "governor: trial improved the frame time, the drop stays");
  printf("depois do governador: cap<=%d pas<=%d fonte=%d chapado=%d\n", cap, pas, fonte, chapado);
  confere(cap == 0 && pas == 0 && fonte == 0 && chapado > 0, "after the drop: smoked glass, no capture");

  // Never flips back: a long run of perfect frames, and the Alta setting too
  // (the drop is only for Automatica; Alta stays frost).
  for (i = 0; i < 10; i++) janela(8.0);
  roda(20, &t);
  confere(gfx_vidro_degradado() && cap == 0 && fonte == 0, "governor: fast frames never bring the frost back");
  setenv("NUVIO_SHOT_VIDRO_QUAL", "2", 1); ajustes_teste_vidro_env();
  roda(20, &t);
  confere(cap == 1 && fonte > 0, "Alta ignores the governor (frost)");

  // Slowness the glass did not cause: the trial window is as slow as before, so
  // the frost comes back and the governor stops for the session (no second trial).
  gfx_vidro_governar_zerar();
  setenv("NUVIO_SHOT_VIDRO_QUAL", "0", 1); ajustes_teste_vidro_env();
  roda(20, &t);
  for (i = 0; i < 4; i++) janela(40.0);
  confere(gfx_vidro_degradado(), "governor: trial started");
  roda(1, &t); roda(20, &t);
  janela(40.0);
  confere(!gfx_vidro_degradado(), "governor: no improvement without glass, the frost is restored");
  roda(1, &t); roda(20, &t);
  confere(cap == 1 && fonte > 0, "governor: frost drawn again after the failed trial");
  for (i = 0; i < 10; i++) janela(40.0);
  confere(!gfx_vidro_degradado(), "governor: never trips again after the trial");

  // Workload changes (review P1). The governor only judges EQUIVALENT workloads.
  // (a) baseline: 3 janky windows on a busy screen, then a 4th on another screen
  //     does not start a trial.
  gfx_vidro_governar_zerar();
  cena(6, 3000000, 0);
  for (i = 0; i < 3; i++) janela(40.0);
  cena(2, 400000, 0);
  janela(40.0);
  confere(!gfx_vidro_degradado(), "governor: 3 janky windows + 1 on another screen: no trial");
  janela(40.0); janela(40.0); janela(40.0);
  confere(gfx_vidro_degradado(), "governor: 4 janky windows on the same screen start the trial");
  // (b) trial interrupted by a cheaper scene: 40 -> 25 ms is not the glass.
  cena(1, 100000, 1);
  janela(25.0);
  confere(!gfx_vidro_degradado(), "governor: trial on a cheaper scene is dropped, no permanent Desempenho");
  cena(6, 3000000, 0);
  for (i = 0; i < 6; i++) janela(40.0);
  confere(!gfx_vidro_degradado(), "governor: at most one trial, the interrupted one ends it");
  // (c) the reverse: trial window on a costlier scene must not end the governor.
  cena(9, 5000000, 1);
  janela(60.0);
  confere(!gfx_vidro_degradado(), "governor: trial on a costlier scene is dropped");
  cena(6, 3000000, 0);
  for (i = 0; i < 6; i++) janela(40.0);
  confere(!gfx_vidro_degradado(), "governor: the second interrupted trial stays ended");
  // (d) a window that mixes scenes proves nothing: 4 of them never start a trial.
  gfx_vidro_governar_zerar();
  for (i = 0; i < 6; i++) janela_mista(40.0, 6, 2);
  confere(!gfx_vidro_degradado(), "governor: windows mixing scenes never start a trial");

  // Interface de vidro OFF with frost kept (Alta): no glass consumes a
  // backdrop, so the zero-region fallback must not blur the full screen on
  // every changed frame. Then back ON: frost resumes and captures again.
  setenv("NUVIO_SHOT_VIDRO_QUAL", "2", 1); ajustes_teste_vidro_env();
  ajustes_definir_vidro(0);
  assert(!ajustes_vidro() && ajustes_vidro_fosco() && ajustes_vidro_qualidade() == 2);
  roda(30, &t);
  printf("Vidro desligado: cap<=%d pas<=%d copia=%.2f fonte=%d chapado=%d\n", cap, pas, copiatot, fonte, chapado);
  // <= 1.0 screen: the toggle frame closes last frame's regions once.
  confere(copiatot <= 1.0 && fonte == 0, "glass off: no full-screen blur on a changing backdrop");
  ajustes_definir_vidro(1);
  roda(30, &t);
  confere(cap == 1 && fonte > 0, "glass back on: frost captures the backdrop again");

  unlink(cam);
  { char tt[700]; snprintf(tt, sizeof tt, "%s/ajustes.tmp", dir); unlink(tt); }
  printf("vidro_qualidade: %s\n", falhas ? "FALHA" : "ok");
  return falhas ? 1 : 0;
}
