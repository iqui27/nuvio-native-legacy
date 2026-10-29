// Exercise the production result loop, including row reservation and artwork.
#include "gfx.h"
#include "ajustes.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static GfxRect cards[32], tile;
static int ncards;
static GLuint art_texture(const char *url, float w) { (void)url; (void)w; return 1; }
static void card_draw(GfxRect r, GLuint tex, GfxModo modo, float f, float px,
                      float py, float radius, float cr, float cg, float cb, float ca) {
  (void)tex; (void)f; (void)px; (void)py; (void)radius;
  (void)cr; (void)cg; (void)cb; (void)ca;
  if (modo == GFX_CARD && ncards < 32) cards[ncards++] = r;
}
static void tile_draw(GfxRect r, float radius, int mode) {
  (void)radius; (void)mode; tile = r;
}
#ifdef NV_REV3
float nv_layout_w = 1920, nv_layout_h = 1080;
static int mobile;
static int mobile_mode(void) { return mobile; }
static int native_keyboard(void) { return mobile; }
#define NV_TOUCH_UI
#define layout_modo_mobile mobile_mode
#define st_ime_disponivel native_keyboard
#endif
#define tex_obter_larg art_texture
#define gfx_rect card_draw
#define ajustes_ui_ilha tile_draw
#include "../src/busca.c"

#ifdef NV_REV3
int test_mobile_rev3(void) {
  mobile = 1;
  nv_layout_w = 1080; nv_layout_h = 1920;
  nFil = 1; sugestao = 0; painel = 0; scrollY = 0;
  memset(fil, 0, sizeof fil); memset(animRes, 0, sizeof animRes);
  fil[0].titulo = "Titles"; fil[0].n = 3;
  for (int i = 0; i < 3; i++) fil[0].itens[i] = i;
  float widths[2];
  for (int i = 0; i < 2; i++) {
    gfx_escala_txt_definir(i ? 1.3f : 1);
    float old = gfx_escala_txt_entrar();
    ncards = 0;
    desenhaResultados(1000);
    assert(ncards >= 2);
    widths[i] = cards[0].w * gfx_escala();
    gfx_escala_sair(old);
  }
  int bad = fabsf(widths[0] - widths[1]) > .01f;
  printf("%s Mobile Search artwork: %.2f -> %.2f\n", bad ? "FAIL" : "PASS", widths[0], widths[1]);
  mobile = 0; nv_layout_w = 1920; nv_layout_h = 1080;
  return bad;
}
#endif
int test_search_layout(float scale) {
  float old = gfx_escala_txt_entrar();
  nFil = 2; sugestao = 0; painel = 0; scrollY = 0;
  memset(fil, 0, sizeof fil); memset(animRes, 0, sizeof animRes);
  memset(scrollX, 0, sizeof scrollX);
  fil[0].titulo = "Best"; fil[0].melhor = 1; fil[0].n = 1;
  fil[1].titulo = "Titles"; fil[1].n = 5;
  for (int c = 0; c < 5; c++) fil[1].itens[c] = c;
  ncards = 0;
  desenhaResultados(1000);
  assert(ncards >= 3);
  int overlap = 0;
  for (int c = 2; c < ncards; c++)
    overlap |= cards[c - 1].x + cards[c - 1].w > cards[c].x + .01f;
  int escape = cards[0].y < tile.y || cards[0].y + cards[0].h > tile.y + tile.h + .01f;
  int row = cards[1].y < tile.y + tile.h + BU_SECAO_GAP + BU_KICK_H - .01f;
  assert(fabsf(cards[0].h * scale - 186) < .01f);
  assert(fabsf(cards[1].w * scale - 180) < .01f || scale == 1);
  printf("%s search posters %.0f%%: width=%.2f stride=%.2f\n", overlap ? "FAIL" : "PASS",
         scale * 100, cards[1].w * scale, (cards[2].x - cards[1].x) * scale);
  printf("%s search best %.0f%%: art bottom=%.2f tile bottom=%.2f row=%s\n",
         escape || row ? "FAIL" : "PASS", scale * 100,
         (cards[0].y + cards[0].h) * scale, (tile.y + tile.h) * scale, row ? "FAIL" : "PASS");
  gfx_escala_sair(old);
  return overlap || escape || row;
}
