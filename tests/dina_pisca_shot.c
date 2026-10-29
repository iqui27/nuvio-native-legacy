// The owner's flicker: "A imagem so pisca no layout Apple TV com o frost
// ligado" — Home layout Dinamica (HOME_LAYOUT_DINAMICA) + frosted glass, on
// Android where the Dynamic background is deferred to end of frame behind a
// destination-alpha coverage pass (dinAdiado, gfx.c).
//
// Reproduction: any screen capture taken while the deferred background is
// still pending reads the transparent framebuffer instead of the gradient,
// so the frosted chrome's blurred backdrop holds black/garbage until the
// NEXT frame's capture replaces it — frame-to-frame alternation.
//
// Asserts, over ~120 frames with focus moves and a scroll, that the read
// alpha under every glass region is 1 (background resolved before the
// frost capture) and that two reads of the same settled frame match. FAILs
// before the fix, PASSes after. Harness only: no production timing macro.
#include "gfx.h"
#include "ajustes.h"
#include "addons.h"
#include "artehero.h"
#include "catalogo.h"
#include "dados.h"
#include "home.h"
#include "layout.h"
#include "ilha.h"
#include "layout.h"
#include "menu.h"
#include "text.h"
#include "tex_cache.h"
#include "corviva.h"
#include "gl_compat.h"
extern int gfx_vidro_copia_com_din_adiado;
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 1920
#define H 1080

static int failures;
static void check(int ok, const char *label) {
  printf("%s: %s\n", ok ? "PASS" : "FAIL", label);
  failures += !ok;
}

static SDL_Window *win;

static void frame(Uint32 now, int trailer) {
  txt_novo_quadro(); tex_novo_quadro(); gfx_novo_quadro();
  gfx_ambiente_preparar();
  glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  corviva_quadro(1.0f / 60, ajustes_cor_viva(), ajustes_cor_logo(), ajustes_animacoes_reduzidas());
  gfx_ambiente(1.0f);
  home_atualizar(1.0f / 60, now);
  home_trailer_passo(1, 1.0f / 60, now);
  home_shot_trailer(trailer);
  home_desenhar(now);
  menu_pilula_mostrar(1);
  menu_atualizar(1.0f / 60, now);
  menu_desenhar(now);
  ilha_desenhar(now);
  gfx_ambiente_descarregar();
}

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k; home_evento(&e);
  e.type = SDL_KEYUP; home_evento(&e);
}

// Alpha must be 1 everywhere the glasses read once the frame settles: the
// deferred background resolves to alpha 1 (its own blend writes ONE into
// alpha over the gradient area) and every foreground draw keeps coverage.
// alpha < 1 under a glass = the frost capture read the still-transparent
// framebuffer (deferred background not yet painted). A video hole
// legitimately reads 0; `furo` skips those pixels.
static int alpha_min(int furo) {
  unsigned char *row = malloc(W * 4);
  int y, x, lo = 255;
  assert(row);
  for (y = 0; y < H; y += 16) {
    glReadPixels(0, y, W, 1, GL_RGBA, GL_UNSIGNED_BYTE, row);
    for (x = 0; x < W; x += 16)
      if (!furo && row[x * 4 + 3] < lo) lo = row[x * 4 + 3];
      else if (furo && row[x * 4 + 3] > 0 && row[x * 4 + 3] < lo) lo = row[x * 4 + 3];
  }
  free(row);
  return lo;
}

int main(int argc, char **argv) {
  const char *out = argc > 1 ? argv[1] : "/tmp/dina";
  char path[1024];
  int i;
  (void)argc;
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  // The deferred Dynamic background needs the window alpha channel it reads
  // for coverage; without it the driver path stays off (dinAdiadoOk).
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
  win = SDL_CreateWindow("dina", 0, 0, W, H, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(win && SDL_GL_CreateContext(win));
  GLint bitsA = 0;
  glGetIntegerv(GL_ALPHA_BITS, &bitsA);
  printf("[dina] alpha bits: %d\n", (int)bitsA);
  glViewport(0, 0, W, H); gfx_tamanho_alvo(W, H);
  assert(gfx_iniciar() && txt_iniciar("deploy/app", 1));
  dados_iniciar("deploy/app/art");
  snprintf(path, sizeof path, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
  FILE *f = fopen(path, "w"); assert(f);
  // Dinamica ("Apple TV") + frosted glass + top menu (the chrome that frosts).
  fputs("homeLayoutLocal 2\nvidroLocal 0\nmenuTopoLocal 1\nidioma 0\ntrailerHero 1\nrelogioTelaLocal 0\n", f);
  fclose(f); ajustes_dir(getenv("NUVIO_DADOS"));
  setenv("NUVIO_SHOT_VIDRO_FOSCO", "1", 1); ajustes_teste_vidro_env();
  assert(ajustes_vidro() && ajustes_vidro_fosco() && ajustes_home_layout() == HOME_LAYOUT_DINAMICA);
  tex_iniciar(64); gfx_icones_dir("deploy/app/art");
  artehero_definir_falhou(tex_falhou);
  assert(home_iniciar("deploy/app/art")); menu_iniciar(); ilha_relogio_visivel(1);
  AddonRemoto addon = {.nome = "Offline", .url = "local", .ativo = 1};
  assert(addons_definir_lista(&addon, 1));
  static CatItem items[8];
  CatFileira row = {.chave = "popular_movie", .titulo = "Popular", .tipo = "movie", .n = 8};
  for (i = 0; i < 8; i++) {
    snprintf(items[i].imdb, sizeof items[i].imdb, "tt90%05d", i);
    snprintf(items[i].tipo, sizeof items[i].tipo, "movie");
    snprintf(items[i].sinopse, sizeof items[i].sinopse, "An offline story beneath the glass chrome.");
    snprintf(items[i].titulo, sizeof items[i].titulo, "Offline movie %d", i + 1);
    snprintf(items[i].backdrop, sizeof items[i].backdrop, "deploy/app/art/%02d.jpg", i);
    snprintf(items[i].backdropCatalogo, sizeof items[i].backdropCatalogo, "%s", items[i].backdrop);
    snprintf(items[i].poster, sizeof items[i].poster, "deploy/app/art/poster/%02d.jpg", i);
  }
  cat_definir_tudo(items, 8, &row, 1);
  int caps = 0, fonte = 0;
  // ~120 frames: settle, focus change + scroll, trailer start (the video
  // hole under the frosted chrome). Track the read alpha (deferred
  // background coverage) and frame-to-frame alternation.
  size_t bytes = (size_t)W * H * 4;
  unsigned char *prev2 = calloc(1, bytes), *prev = calloc(1, bytes), *cur = malloc(bytes);
  int alternations = 0;
  assert(prev2 && prev && cur);
  for (i = 0; i < 160; i++) {
    tex_bombear(16);
    frame(10000 + 16 * i, i > 30 && i < 60);   // trailer live under the chrome
    caps += gfx_vidro_capturas; fonte += gfx_vidro_com_fonte;
    glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, cur);
    if (i >= 2 && i % 4 == 0) {
      long d01 = 0, d12 = 0, d02 = 0; size_t p;
      for (p = 0; p < bytes; p += 4 * 7) {
        d01 += labs(prev2[p] - prev[p]); d12 += labs(prev[p] - cur[p]); d02 += labs(prev2[p] - cur[p]);
      }
      if (d02 < (d01 < d12 ? d01 : d12) * 3 / 5) alternations++;
      printf("[dina] f%03d d01=%ld d12=%ld d02=%ld caps=%d\n",
             i, d01, d12, d02, gfx_vidro_capturas);
    }
    memcpy(prev2, prev, bytes); memcpy(prev, cur, bytes);
  }
  printf("[dina] settle: captures=%d glass-with-source=%d alternations=%d\n",
         caps, fonte, alternations);
  int lo0 = alpha_min(1);
  // Focus change + scroll, the exact combo that flickers on the TV.
  caps = fonte = 0;
  for (i = 0; i < 8; i++) { tecla(SDLK_RIGHT); frame(12000 + 16 * i, 0);
    caps += gfx_vidro_capturas; fonte += gfx_vidro_com_fonte; }
  for (i = 0; i < 6; i++) { tecla(SDLK_DOWN);  frame(13000 + 16 * i, 0);
    caps += gfx_vidro_capturas; fonte += gfx_vidro_com_fonte; }
  int lo1 = alpha_min(1);
  printf("[dina] move: captures=%d glass-with-source=%d\n", caps, fonte);
  for (i = 0; i < 4; i++) { tecla(SDLK_UP);    frame(14000 + 16 * i, 0); }
  int lo2 = alpha_min(1);
  if (lo1 < lo2) lo2 = lo1; if (lo0 < lo2) lo2 = lo0;
  free(prev2); free(prev); free(cur);
  printf("[dina] alpha min=%d alternations=%d copies-under-deferred=%d\n",
         lo2, alternations, gfx_vidro_copia_com_din_adiado);
  check(gfx_vidro_copia_com_din_adiado == 0,
        "frost backdrop copy never reads the still-transparent deferred background");
  check(lo2 == 255,
        "deferred Dynamic background resolved before the frost capture");
  check(alternations == 0, "no frame-to-frame alternation");
  check(glGetError() == GL_NO_ERROR, "GL state");
  // Keep one PNG as evidence of the final frame.
  { unsigned char *px = malloc((size_t)W * H * 4);
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_RGBA32);
    int y;
    assert(px && s);
    glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px);
    for (y = 0; y < H; y++)
      memcpy((char *)s->pixels + y * s->pitch, px + (size_t)(H - 1 - y) * W * 4, W * 4);
    snprintf(path, sizeof path, "%s/dina-final.png", out);
    assert(IMG_SavePNG(s, path) == 0);
    SDL_FreeSurface(s);
    free(px); }
  tex_encerrar(); SDL_Quit();
  return failures ? 1 : 0;
}
