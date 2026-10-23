// Offline home render: real home, A27 chrome, clock, moving focus and backdrop.
// Budget: at most two five-pass captures per animated frame on webOS and .tpk
// (intervening backdrop draws invalidate sharing), zero while static. Same kernel
// and resolution preserve the look.
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
// Render the real consent modal offline, without enabling its Tizen startup gate.
#define telemetria_primeira_vez consent_unused_startup
#define telemetria_aberto consent_unused_open
#define telemetria_evento consent_unused_event
#define telemetria_atualizar consent_unused_update
#define telemetria_desenhar consent_draw
#define telemetria_teste_foco consent_unused_focus
#include "../src/telemetria.c"
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int width, height, failures;
static float fade = .001f;   // backdrop alpha step per frame while in motion
static void check(int ok, const char *label) {
  printf("%s: %s\n", ok ? "PASS" : "FAIL", label);
  failures += !ok;
}
static void frame(int n, int motion, int trailer) {
  Uint32 now = 10000u + 16u * n;
  txt_novo_quadro(); tex_novo_quadro(); gfx_novo_quadro();
  glClearColor(.05f, .05f, .05f, 1); glClear(GL_COLOR_BUFFER_BIT);
  home_atualizar(1.0f / 60, now);
  home_shot_trailer(trailer);
  // Change the backdrop hash before the first glass, including hero buttons.
  // Native trailer holes are checked separately below.
  if (motion) gfx_cor((GfxRect){0, 0, 1920, 1080}, 0, .12f, .18f, .3f, .02f + fade * (n % 20));
  home_desenhar(now);
  menu_pilula_mostrar(1);
  menu_atualizar(1.0f / 60, now); menu_desenhar(now);
  ilha_desenhar(now);
}
static void shot(const char *dir, const char *name) {
  char path[1024];
  unsigned char *pixels = malloc((size_t)width * height * 4);
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
  assert(pixels && s);
  glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  for (int y = 0; y < height; y++)
    memcpy((char *)s->pixels + y * s->pitch, pixels + (size_t)(height - 1 - y) * width * 4, width * 4);
  snprintf(path, sizeof path, "%s/%s-%d.png", dir, name, width);
  assert(IMG_SavePNG(s, path) == 0);
  SDL_FreeSurface(s); free(pixels);
}
int main(int argc, char **argv) {
  assert(argc == 4);
  width = atoi(argv[1]); height = width * 9 / 16;
  int webos = atoi(argv[3]);
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("glass C9", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w && SDL_GL_CreateContext(w));
  GLuint target, fbo;
  glGenTextures(1, &target); glBindTexture(GL_TEXTURE_2D, target);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, width, height); gfx_tamanho_alvo(width, height);
  assert(gfx_iniciar() && txt_iniciar("deploy/app", 1));
  dados_iniciar("deploy/app/art");
  char path[1024]; snprintf(path, sizeof path, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
  FILE *f = fopen(path, "w"); assert(f);
  fputs("homeLayoutLocal 0\nvidroLocal 0\nmenuTopoLocal 1\nidioma 0\ntrailerHero 1\nrelogioTelaLocal 0\n", f);
  fclose(f); ajustes_dir(getenv("NUVIO_DADOS"));
  setenv("NUVIO_SHOT_VIDRO_FOSCO", "1", 1); ajustes_teste_vidro_env();
  assert(ajustes_vidro() && ajustes_vidro_fosco() && ajustes_menu_topo());
  // No disk cache: remote texture requests cannot enter the network worker.
  tex_iniciar(64); gfx_icones_dir("deploy/app/art");
  artehero_definir_falhou(tex_falhou);
  assert(home_iniciar("deploy/app/art")); menu_iniciar(); ilha_relogio_visivel(1);
  AddonRemoto addon = {.nome = "Offline", .url = "local", .ativo = 1};
  assert(addons_definir_lista(&addon, 1));
  static CatItem items[8];
  CatFileira row = {.chave = "popular_movie", .titulo = "Popular", .tipo = "movie", .n = 8};
  for (int i = 0; i < 8; i++) {
    snprintf(items[i].imdb, sizeof items[i].imdb, "tt90%05d", i);
    snprintf(items[i].tipo, sizeof items[i].tipo, "movie");
    snprintf(items[i].sinopse, sizeof items[i].sinopse, "An offline story beneath the glass chrome.");
    snprintf(items[i].titulo, sizeof items[i].titulo, "Offline movie %d", i + 1);
    snprintf(items[i].backdrop, sizeof items[i].backdrop, "deploy/app/art/%02d.jpg", i);
    snprintf(items[i].backdropCatalogo, sizeof items[i].backdropCatalogo, "%s", items[i].backdrop);
    snprintf(items[i].poster, sizeof items[i].poster, "deploy/app/art/poster/%02d.jpg", i);
  }
  cat_definir_tudo(items, 8, &row, 1);
  for (int i = 0; i < 150; i++) { tex_bombear(16); frame(i, 0, 0); SDL_Delay(2); }
  assert(tex_obter_hero(items[0].backdrop));
  frame(150, 0, 0); shot(argv[2], "home");
  int captures = 0, passes = 0, maximum = 0;
  double fill = 0, copy = 0;
  menu_abrir();
  for (int i = 0; i < 20; i++) {
    if (i % 5 == 0) { SDL_Event e = {.type = SDL_KEYDOWN}; e.key.keysym.sym = SDLK_RIGHT; menu_evento(&e); }
    frame(151 + i, 1, 0);
    captures += gfx_vidro_capturas; passes += gfx_vidro_passadas;
    fill += gfx_vidro_fill; copy += gfx_vidro_copia;
    if (gfx_vidro_capturas > maximum) maximum = gfx_vidro_capturas;
  }
  printf("tier=%s drawable=%dx%d animated/focus: captures/frame=%.2f passes/frame=%.2f blur-fill/frame=%.6f copy/frame=%.6f blur-pixels/frame=%.0f max-captures=%d\n",
         strrchr(argv[2], '/') ? strrchr(argv[2], '/') + 1 : argv[2], width, height, captures / 20.0, passes / 20.0, fill / 20, copy / 20, fill / 20 * width * height, maximum);
  check(captures > 0 && maximum <= 2, "capture budget across all chrome regions");
  check(captures <= 40, "animated capture budget");
  // Blur budget (gfx.c vidPreparar/vidCapturar): targets capped at the 1080p
  // chain, so 4K pays the 1080p blur pixels; 1080p goes to 1/4 in one pass.
  check(passes == captures * (width > 1920 ? 4 : 3), "blur passes per capture");
  check(captures && fill * width * height / captures <= 1.5e6, "blur pixels per capture capped at the 1080p chain");
  // Keep a full backdrop copy for region misses; blur only the glass regions.
  check(captures && copy / captures <= 1.0, "at most one full backdrop copy per capture");
  check(captures && fill * width * height / captures <= 2.5e5, "blur only the glass regions");
  check(fill / 20 <= (webos ? 1.4 : 1.0), "blur fill budget with intervening backdrop invalidation");
  shot(argv[2], "focus");
  // Upload invalidation must override temporal reuse; sample age must not be
  // renewed by reuse, or animation would freeze indefinitely.
  gfx_vidro_sujar(); frame(172, 1, 0);
  check(gfx_vidro_capturas >= 1, "texture upload invalidates capture immediately");
  menu_fechar();
  for (int i = 0; i < 150; i++) frame(220, 0, 0);
  captures = passes = 0;
  for (int i = 0; i < 5; i++) {
    frame(220, 0, 0); captures += gfx_vidro_capturas; passes += gfx_vidro_passadas;
  }
  printf("static: captures=%d passes=%d over five frames\n", captures, passes);
  check(captures == 0 && passes == 0,
        "static backdrop reuse budget");
  // Home captures first; the consent veil must enter the later modal capture,
  // including on subsequent frames while the modal remains open: those may
  // reuse that capture (its own slot), so they must match the first frame.
  entrada = 1.0f;
  size_t bytes = (size_t)width * height * 4;
  unsigned char *first = malloc(bytes), *again = malloc(bytes);
  assert(first && again);
  for (int i = 0; i < 3; i++) {
    frame(220, 0, 0);
    int before = gfx_vidro_capturas, diff = 0;
    consent_draw(13520);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, i ? again : first);
    for (size_t p = 0; i && p < bytes; p++) diff |= first[p] != again[p];
    check(i ? !diff : gfx_vidro_capturas > before, "consent veil invalidates Home backdrop before modal glass");
  }
  free(first); free(again);
  shot(argv[2], "modal");
  for (int i = 0; i < 5; i++) frame(380 + i, 0, 1);
  shot(argv[2], "trailer");
  // A changing backdrop must never reuse the previous frame's source.
  fade = .02f; captures = 0;
  for (int i = 0; i < 10; i++) { frame(221 + i, 1, 0); captures += gfx_vidro_capturas; }
  printf("fade: captures=%d over ten frames\n", captures);
  check(captures == 10, "fade recaptures each changed frame");
  {
    size_t bytes = (size_t)width * height * 4;
    unsigned char *late = malloc(bytes), *fresh = malloc(bytes);
    assert(late && fresh);
    frame(230, 1, 0); frame(230, 1, 0);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, late);
    gfx_vidro_sujar(); frame(230, 1, 0);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, fresh);
    check(!memcmp(late, fresh, bytes), "the frame after a fade catches up");
    free(late); free(fresh);
  }
  check(glGetError() == GL_NO_ERROR, "GL state after trailer and chrome");
  tex_encerrar(); SDL_Quit();
  return failures ? 1 : 0;
}
