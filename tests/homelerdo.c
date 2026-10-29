// Offline Home perf probe (Moderna, Frost on, Android gfx tier): what the GPU
// pays per frame idle and while moving between rows. TCL log 10:46: home idle
// 40 fps at fill 1.63x, row change 33 fps with swap 48-62 ms.
#include "gfx.h"
#include "ajustes.h"
#include "addons.h"
#include "artehero.h"
#include "catalogo.h"
#include "dados.h"
#include "home.h"
#include "ilha.h"
#include "menu.h"
#include "text.h"
#include "tex_cache.h"
#include "perftv.h"
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern void ajustes_teste_tema(int tema, int vidro);

int hl_gl[8];
static int failures, nframe;
static void check(int ok, const char *label) {
  printf("%s: %s\n", ok ? "PASS" : "FAIL", label);
  failures += !ok;
}
typedef struct { double fill, gpu, blur, copy; int cheias, capt, pass, n, gl[8], up; double upd, des; double modo[GFX_NMODOS]; } Soma;
static void frame(Soma *s) {
  // Real clock, paced near 60 Hz: the hero carousel and easing read SDL_GetTicks.
  static Uint32 ant;
  SDL_Delay(ant && SDL_GetTicks() - ant < 16 ? 16 - (SDL_GetTicks() - ant) : 0);
  Uint32 now = SDL_GetTicks(); ant = now; nframe++;
  int gl0[8], up0 = perftv_uploads; memcpy(gl0, hl_gl, sizeof gl0);
  txt_novo_quadro(); tex_novo_quadro(); gfx_novo_quadro();
  glClearColor(.05f, .05f, .05f, 1); glClear(GL_COLOR_BUFFER_BIT);
  Uint64 t0 = SDL_GetPerformanceCounter();
  home_atualizar(1.0f / 60, now);
  menu_atualizar(1.0f / 60, now);
  Uint64 t1 = SDL_GetPerformanceCounter();
  home_desenhar(now);
  menu_desenhar(now);
  ilha_desenhar(now);
  Uint64 t2 = SDL_GetPerformanceCounter();
  glFinish();
  if (!s) return;
  double f = 1000.0 / SDL_GetPerformanceFrequency();
  s->upd += (t1 - t0) * f; s->des += (t2 - t1) * f;
  for (int k = 0; k < 8; k++) s->gl[k] += hl_gl[k] - gl0[k];
  s->up += perftv_uploads - up0;
  s->fill += gfx_fill; s->gpu += gfx_fill_gpu; s->blur += gfx_vidro_fill; s->copy += gfx_vidro_copia;
  if (getenv("HL_TRACE")) printf("frame %d: up=%d capt=%d pass=%d copy=%.2f fbo=%d 12=%.2f 38=%.2f 3=%.2f 37=%.2f fill=%.2f\n", nframe, perftv_uploads - up0, gfx_vidro_capturas, gfx_vidro_passadas, gfx_vidro_copia, hl_gl[1] - gl0[1], gfx_fill_modo[12], gfx_fill_modo[38], gfx_fill_modo[3], gfx_fill_modo[37], gfx_fill);
  s->cheias += gfx_n_cheio; s->capt += gfx_vidro_capturas; s->pass += gfx_vidro_passadas; s->n++;
  for (int k = 0; k < GFX_NMODOS; k++) s->modo[k] += gfx_fill_modo[k];
}
static void relato(const char *nome, const Soma *s) {
  double n = s->n;
  printf("%s: cpu upd=%.2fms des=%.2fms\n", nome, s->upd / n, s->des / n);
  printf("%s: fill=%.2f gpu=%.2f full=%.2f captures=%.2f passes=%.2f blur=%.3f copy=%.3f |",
         nome, s->fill / n, s->gpu / n, s->cheias / n, s->capt / n, s->pass / n, s->blur / n, s->copy / n);
  for (int k = 0; k < GFX_NMODOS; k++) if (s->modo[k] / n > 0.02) printf(" %d=%.2f", k, s->modo[k] / n);
  printf(" | read=%.2f fbo=%.2f copytex=%.2f clear=%.2f draw=%.1f mip=%.2f upload=%.2f\n", s->gl[0] / n, s->gl[1] / n,
         s->gl[2] / n, s->gl[3] / n, s->gl[4] / n, s->gl[5] / n, s->up / n);
}
static void shot(const char *dir, const char *name) {
  if (!dir || !*dir) return;
  char path[1024];
  unsigned char *pixels = malloc(1920 * 1080 * 4);
  SDL_Surface *o = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(pixels && o);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  for (int y = 0; y < 1080; y++)
    memcpy((char *)o->pixels + y * o->pitch, pixels + (size_t)(1079 - y) * 1920 * 4, 1920 * 4);
  snprintf(path, sizeof path, "%s/%s.png", dir, name);
  assert(IMG_SavePNG(o, path) == 0);
  SDL_FreeSurface(o); free(pixels);
}
static void tecla(SDL_Keycode k) {
  SDL_Event e = {.type = SDL_KEYDOWN}; e.key.keysym.sym = k; home_evento(&e);
  SDL_Event u = {.type = SDL_KEYUP}; u.key.keysym.sym = k; home_evento(&u);
}
int main(int argc, char **argv) {
  assert(argc == 2);
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("homelerdo", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w && SDL_GL_CreateContext(w));
  GLuint target, fbo;
  glGenTextures(1, &target); glBindTexture(GL_TEXTURE_2D, target);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar() && txt_iniciar("deploy/app", 1));
  dados_iniciar("deploy/app/art");
  char path[1024]; snprintf(path, sizeof path, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
  FILE *f = fopen(path, "w"); assert(f);
  fputs("homeLayoutLocal 0\nvidroLocal 0\nmenuTopoLocal 1\nidioma 0\ntrailerHero 0\nrelogioTelaLocal 0\n", f);
  fclose(f); ajustes_dir(getenv("NUVIO_DADOS"));
  setenv("NUVIO_SHOT_VIDRO_FOSCO", "1", 1); ajustes_teste_vidro_env();
  ajustes_teste_tema(12, 1);   // "Da arte" (AJ_TEMA_DINAMICA): cor-viva=1 as on the TCL
  assert(ajustes_vidro() && ajustes_vidro_fosco() && ajustes_cor_viva() == 1);
  perftv_enabled = 1; tex_iniciar(64); gfx_icones_dir("deploy/app/art");
  artehero_definir_falhou(tex_falhou);
  assert(home_iniciar("deploy/app/art")); menu_iniciar(); ilha_relogio_visivel(1);
  AddonRemoto addon = {.nome = "Offline", .url = "local", .ativo = 1};
  assert(addons_definir_lista(&addon, 1));
  // A real account: many rows of 20 (TCL home: CAT_FIL_MAX 224 on Android).
  enum { NR = 24, NI = 20 };
  static CatItem items[NR * NI];
  static CatFileira rows[NR];
  for (int r = 0; r < NR; r++) {
    snprintf(rows[r].chave, sizeof rows[r].chave, "row_%d_movie", r);
    snprintf(rows[r].titulo, sizeof rows[r].titulo, "Row %d", r + 1);
    snprintf(rows[r].tipo, sizeof rows[r].tipo, "movie");
    rows[r].ini = r * NI; rows[r].n = NI;
  }
  for (int i = 0; i < NR * NI; i++) {
    snprintf(items[i].imdb, sizeof items[i].imdb, "tt90%05d", i);
    snprintf(items[i].tipo, sizeof items[i].tipo, "movie");
    snprintf(items[i].sinopse, sizeof items[i].sinopse, "An offline story beneath the glass chrome.");
    snprintf(items[i].titulo, sizeof items[i].titulo, "Offline movie %d", i + 1);
    snprintf(items[i].backdrop, sizeof items[i].backdrop, "deploy/app/art/%02d.jpg", i % 8);
    snprintf(items[i].backdropCatalogo, sizeof items[i].backdropCatalogo, "%s", items[i].backdrop);
    snprintf(items[i].poster, sizeof items[i].poster, "deploy/app/art/poster/%02d.jpg", i % 8);
  }
  cat_definir_tudo(items, NR * NI, rows, NR);
  for (int i = 0; i < 150; i++) { tex_bombear(16); frame(NULL); }
  Soma idle = {0}, rows_ = {0};
  for (int i = 0; i < 60; i++) frame(&idle);
  relato("idle", &idle);
  shot(argv[1], "home-idle");
  // Leave the hero for Row 1, then walk down: each new card swaps the hero art.
  tecla(SDLK_DOWN);
  for (int i = 0; i < 60; i++) { tex_bombear(16); frame(NULL); }
  for (int r = 0; r < 4; r++) {
    tecla(SDLK_DOWN);
    int nq = getenv("HL_LONG") && r == 1 ? atoi(getenv("HL_LONG")) : 45;
    for (int i = 0; i < nq; i++) { tex_bombear(16); frame(&rows_); }
    if (r == 0) shot(argv[1], "home-row2");
  }
  relato("rows", &rows_);
  check(idle.capt == 0 && idle.pass == 0, "idle Home reuses the glass backdrop");
  check(idle.gpu / idle.n <= 2.0, "idle Home GPU fill <= 2.0 screens");
  check(rows_.capt / (double)rows_.n <= 0.5, "row change recaptures the glass backdrop at most every other frame");
  check(rows_.gpu / rows_.n <= 2.5, "row change GPU fill <= 2.5 screens");
  // Each hero crossfade frame recaptures; a full-screen copy there doubled the
  // bandwidth of a frame that already reads the whole hero twice.
  check(rows_.capt && rows_.copy / rows_.capt <= 0.25, "row change copies only the glass regions per capture");
  tex_encerrar(); SDL_Quit();
  return failures ? 1 : 0;
}
