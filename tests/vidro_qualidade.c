// "Qualidade do vidro": Alta / Desempenho / Automatica and its governor.
//   bash tests/vidro_qualidade.sh
#include "gfx.h"
#include "gpunivel.h"
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

void gpun_teste_reiniciar(void);
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

// A glass entering a new region must see the same clean backdrop on both
// frames, even if foreground content was drawn before the glass.
static float regiao_fundo = .05f;
static int regiao_veu;
static void regiao_quadro(int direita, unsigned char pixel[4]) {
  gfx_novo_quadro();
  glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
  gfx_cor((GfxRect){0, 0, 1920, 1080}, 0, regiao_fundo, regiao_fundo, regiao_fundo, 1);
  gfx_cor((GfxRect){1200, 250, 500, 500}, 0, 1, 0, 0, 1);
  if (regiao_veu) gfx_cor((GfxRect){0, 0, 1920, 1080}, 0, 0, 0, 1, .5f);   // modal veil after foreground
  gfx_vidro((GfxRect){direita ? 1250 : 100, 300, 300, 300}, .1f, GFX_VIDRO_PAINEL, .1f, .8f, 1);
  glReadPixels(1400, 1080 - 450, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
}
// VIDRO_QUAL_SHOT=<file.bmp>: save the current frame (before/after evidence).
static void regiao_shot(const char *sufixo) {
  const char *dir = getenv("VIDRO_QUAL_SHOT");
  char nome[512];
  unsigned char *pix;
  SDL_Surface *s;
  int y;
  if (!dir) return;
  snprintf(nome, sizeof nome, "%s/%s.bmp", dir, sufixo);
  pix = malloc(1920 * 1080 * 4);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(pix && s);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  for (y = 0; y < 1080; y++) memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(SDL_SaveBMP(s, nome) == 0);
  SDL_FreeSurface(s); free(pix);
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
  {
    unsigned char p[4], first[4], second[4];
    gfx_vidro_sujar();
    for (i = 0; i < 4; i++) regiao_quadro(0, p);
    regiao_quadro(1, first); regiao_shot("auto-new-region-frame1");
    double copy1 = gfx_vidro_copia, fill1 = gfx_vidro_fill;
    int cap1 = gfx_vidro_capturas, pass1 = gfx_vidro_passadas;
    regiao_quadro(1, second); regiao_shot("auto-new-region-frame2");
    printf("new region first=%u,%u,%u second=%u,%u,%u copy=%.3f/%.3f fill=%.3f/%.3f captures=%d/%d passes=%d/%d\n", first[0], first[1], first[2], second[0], second[1], second[2], copy1, gfx_vidro_copia, fill1, gfx_vidro_fill, cap1, gfx_vidro_capturas, pass1, gfx_vidro_passadas);
    confere(abs((int)first[0] - second[0]) <= 1 && abs((int)first[1] - second[1]) <= 1 && abs((int)first[2] - second[2]) <= 1,
            "new glass region keeps clean backdrop across frames");
    confere(copy1 == 0.0 && gfx_vidro_copia == 0.0, "static backdrop: new region reuses the retained copy, no screen copy");
    // Invalidation: the backdrop changes; the stale retained copy must not be
    // read. Once the new backdrop holds still it is retained again.
    unsigned char stale[4]; memcpy(stale, second, 4);
    regiao_fundo = .25f;
    for (i = 0; i < 40; i++) regiao_quadro(0, p);
    regiao_quadro(1, first);
    regiao_quadro(1, second);
    printf("changed region first=%u,%u,%u second=%u,%u,%u stale=%u copy=%.3f\n", first[0], first[1], first[2], second[0], second[1], second[2], stale[0], gfx_vidro_copia);
    confere(abs((int)first[0] - second[0]) <= 1 && abs((int)first[1] - second[1]) <= 1 && first[0] > stale[0] + 5,
            "changed backdrop: retained copy refreshed, never stale");
    // Out-of-hash invalidation (gfx_vidro_sujar): the retained copy is dropped,
    // the new region copies the live screen again.
    regiao_fundo = .05f;
    for (i = 0; i < 40; i++) regiao_quadro(0, p);
    gfx_vidro_sujar();
    regiao_quadro(1, first);
    printf("sujar region copy=%.3f\n", gfx_vidro_copia);
    confere(gfx_vidro_copia > 0.0, "gfx_vidro_sujar drops the retained backdrop");
    // A modal veil after foreground: the glass reads the live screen (veil),
    // not the retained clean backdrop.
    for (i = 0; i < 40; i++) regiao_quadro(0, p);
    regiao_veu = 1; regiao_quadro(1, first); regiao_quadro(1, second); regiao_veu = 0;
    printf("veil region first=%u,%u,%u second=%u,%u,%u\n", first[0], first[1], first[2], second[0], second[1], second[2]);
    confere(first[2] > first[1] + 20 && second[2] > second[1] + 20, "modal veil over glass still read from the live screen");
  }
  {
    unsigned char p[4], first[4], second[4];
    setenv("NUVIO_SHOT_VIDRO_QUAL", "2", 1); ajustes_teste_vidro_env();
    gfx_vidro_sujar();
    for (i = 0; i < 4; i++) regiao_quadro(0, p);
    regiao_quadro(1, first);
    double copy1 = gfx_vidro_copia, fill1 = gfx_vidro_fill;
    regiao_quadro(1, second);
    printf("high region first=%u,%u,%u second=%u,%u,%u copy=%.3f/%.3f fill=%.3f/%.3f\n", first[0], first[1], first[2], second[0], second[1], second[2], copy1, gfx_vidro_copia, fill1, gfx_vidro_fill);
    confere(abs((int)first[0] - second[0]) <= 1 && abs((int)first[1] - second[1]) <= 1 && abs((int)first[2] - second[2]) <= 1,
            "High clean backdrop comparison");
    for (int quality = 0; quality <= 2; quality += 2) {
      double copied = 0, filled = 0;
      int captures = 0, passes = 0;
      setenv("NUVIO_SHOT_VIDRO_QUAL", quality ? "2" : "0", 1); ajustes_teste_vidro_env();
      regiao_fundo = .05f;
      gfx_vidro_sujar();
      for (i = 0; i < 3; i++) regiao_quadro(0, p);
      for (i = 0; i < 8; i++) {
        regiao_fundo = .05f + i * .01f;
        regiao_quadro(0, p);
        copied += gfx_vidro_copia; filled += gfx_vidro_fill;
        captures += gfx_vidro_capturas; passes += gfx_vidro_passadas;
      }
      printf("changing backdrop quality=%d copy=%.3f fill=%.3f captures=%d passes=%d frames=8\n", quality, copied, filled, captures, passes);
    }
    regiao_fundo = .05f;
    setenv("NUVIO_SHOT_VIDRO_QUAL", "0", 1); ajustes_teste_vidro_env();
  }
  roda(1, &t);
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

  // Automatic ladder, rung 1: one Kawase pass less, the glass stays frosted.
  setenv("NUVIO_SHOT_VIDRO_QUAL", "0", 1); ajustes_teste_vidro_env();
  gfx_vidro_governar_zerar();
  gpun_teste_reiniciar();
  roda(30, &t);
  { int completo = pas;
    gpun_relatar(60, 40, 1000.0 / 60, 1, 1);
    for (i = 0; i < 3; i++) gpun_relatar(30, 40, 1000.0 / 60, 1, 1);
    assert(gpun_efeito_ativo(1));
    roda(30, &t);
    printf("Degrau 1: cap<=%d pas<=%d (completo %d) fonte=%d\n", cap, pas, completo, fonte);
    confere(cap == 1 && pas == completo - 1 && fonte > 0, "ladder rung 1: cheaper blur before any glass or frost removal");
  }

  unlink(cam);
  { char tt[700]; snprintf(tt, sizeof tt, "%s/ajustes.tmp", dir); unlink(tt); }
  printf("vidro_qualidade: %s\n", falhas ? "FALHA" : "ok");
  return falhas ? 1 : 0;
}
