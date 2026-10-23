#include <SDL2/SDL.h>
#include <string.h>
#include "gfx.h"
#include "text.h"
#include "dados.h"
#include "catalogo.h"
#include "tex_cache.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static GfxRect texts[256];
static int recording, count;
static FILE *proof;
void rev3_record(int enabled) { recording = enabled; if (enabled) count = 0; }
int rev3_overlaps(void) {
  int n = 0;
  for (int i = 0; i < count; i++) for (int j = i + 1; j < count; j++) {
    GfxRect a = texts[i], b = texts[j];
    if (a.x + a.w > b.x + 1 && b.x + b.w > a.x + 1 &&
        a.y + a.h > b.y + 1 && b.y + b.h > a.y + 1) n++;
  }
  return n;
}
void nvreal_gfx_rect(GfxRect, GLuint, GfxModo, float, float, float, float, float, float, float, float);
void gfx_rect(GfxRect r, GLuint tex, GfxModo m, float f, float px, float py, float radius, float cr, float cg, float cb, float ca) {
  if (proof && m == GFX_TEXTO) fprintf(proof, "T %.2f\n", r.h * gfx_escala());
  if (proof && m == GFX_COR) fprintf(proof, "R %.2f %.2f\n", r.w * gfx_escala(), r.h * gfx_escala());
  if (recording && m == GFX_TEXTO && ca > .01f && count < 256) texts[count++] = r;
  nvreal_gfx_rect(r, tex, m, f, px, py, radius, cr, cg, cb, ca);
}
int test_settings_rev3(void);
int test_mobile_rev3(void);
int main(int argc, char **argv) {
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  SDL_Window *win = SDL_CreateWindow("rev3", 0, 0, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(win && SDL_GL_CreateContext(win));
  gfx_tamanho_alvo(1920, 1080); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1)); tex_iniciar(16);
  CatItem items[3] = {0};
  for (int i = 0; i < 3; i++) { snprintf(items[i].titulo, sizeof items[i].titulo, "Title %d", i); strcpy(items[i].poster, "fixture"); }
  cat_definir_tudo(items, 3, NULL, 0);
  gfx_escala_txt_definir(1.3f);
  int bad = test_settings_rev3();
  bad |= test_mobile_rev3();
  assert(argc == 2);
  const char *names[] = {"base", "scaled", "unscaled-text", "unscaled-button", "missing-button"};
  for (int i = 0; i < 5; i++) {
    char path[1024]; snprintf(path, sizeof path, "%s/%s.log", argv[1], names[i]);
    proof = fopen(path, "w"); assert(proof);
    fputs("F 0\n", proof);
    gfx_escala_sair(i ? 1.3f : 1);
    for (int j = 0; j < 5; j++) {
      if (i == 2 && j == 4) gfx_escala_sair(1);
      TxtLinha t = txt_linha(TXT_V3_LN_B, "Runtime control", 255, 255, 255, 255);
      txt_desenhar(t, 20, 20 + j * 60);
    }
    gfx_escala_sair(i && i != 3 ? 1.3f : 1);
    if (i != 4) gfx_rect((GfxRect){20, 400, 180, 50}, 0, GFX_COR, 0, 0, 0, 0, 1, 1, 1, 1);
    fclose(proof); proof = NULL;
  }
  return bad ? 1 : 0;
}
