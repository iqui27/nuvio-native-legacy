// Two zoomed layers in the same frame (Settings at 80% and the menu pill at its
// fixed 0.9) must not evict each other's fonts and lines. Before 04/10 the
// single "camada ampliada" closed everything whenever the factor changed, so
// every frame re-rasterized both layers and, on a slow TV, the per-frame text
// budget ran out before the last lines (empty pills and key hints in
// Settings). Needs GL (Mac graphics context).
#include "gfx.h"
#include "text.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>

static void desenhaEm(float esc, const char *a, const char *b) {
  float ant = gfx_escala();
  gfx_escala_sair(esc);
  txt_linha(TXT_BODY, a, 255, 255, 255, 255);
  txt_linha(TXT_CAPTION, b, 255, 255, 255, 255);
  gfx_escala_sair(ant);
}

int main(void) {
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *win = SDL_CreateWindow("t", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(win);
  SDL_GLContext gl = SDL_GL_CreateContext(win); assert(gl);
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  int q, r0 = 0;
  for (q = 0; q < 6; q++) {
    txt_novo_quadro();
    r0 = txt_rasterizadas;
    desenhaEm(0.8f, "Source selection", "options · 5 advanced");
    desenhaEm(0.9f, "Settings", "Back");
    desenhaEm(1.0f, "10:34", "Home");
    desenhaEm(1.2f, "Interface", "Bigger");
  }
  printf("ultimo quadro: %d linhas rasterizadas (esperado 0)\n", txt_rasterizadas - r0);
  assert(txt_rasterizadas - r0 == 0);
  // Library frame on a TV where "Tamanho da interface" resolves to 85% (>= 75"
  // panels) with the default 85% text scale: header 1.02, grid 0.85, menu 0.78
  // and the ESCALA_INI layers (toast, island, context menu) 0.723 are four live
  // factors in ONE frame. With three slots they evicted each other every frame
  // and the "Watched" badge text never fit the raster budget on the C9.
  gfx_escala_ui_definir(0.85f); gfx_escala_txt_definir(0.85f);
  for (q = 0; q < 6; q++) {
    txt_novo_quadro();
    r0 = txt_rasterizadas;
    desenhaEm(1.02f, "Library", "14 titles");
    desenhaEm(0.85f, "Watched", "Anatomy of a Fall");
    desenhaEm(0.78f, "Settings", "Back");
    desenhaEm(0.723f, "10:34", "Home");
  }
  printf("biblioteca 85%%: %d linhas rasterizadas no ultimo quadro (esperado 0)\n", txt_rasterizadas - r0);
  assert(txt_rasterizadas - r0 == 0);
  gfx_escala_ui_definir(1.0f); gfx_escala_txt_definir(1.0f);
  // A fifth factor reuses the least used slot and still renders.
  txt_novo_quadro();
  { float ant = gfx_escala(); gfx_escala_sair(1.5f);
    TxtLinha l = txt_linha(TXT_BODY, "Nova escala", 255, 255, 255, 255);
    gfx_escala_sair(ant);
    assert(l.w > 0 && l.tex); }
  puts("ok");
  return 0;
}
