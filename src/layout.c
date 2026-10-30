#include "layout.h"
#include <math.h>

int layout_modo_resolver(int preferencia, int automaticoMobile) {
  if (preferencia == 0) return 0;
  if (preferencia == 1) return 1;
  return automaticoMobile != 0;
}


#ifdef NV_TOUCH_UI
float nv_layout_w = NV_TELA_BASE_W;
float nv_layout_h = NV_TELA_BASE_H;
static int telaW = (int)NV_TELA_BASE_W;
static int telaH = (int)NV_TELA_BASE_H;
static int modoMobile;
#ifdef NV_DESKTOP_UI
static int janelaW = NV_DESKTOP_WINDOW_W, janelaH = NV_DESKTOP_WINDOW_H;
float nv_desktop_sidebar = NV_DESKTOP_SIDEBAR_W;
Uint32 nv_desktop_wheel_until;
int nv_desktop_scroll_active;
#endif
float nv_inset_e, nv_inset_t, nv_inset_d, nv_inset_b;
static float insetFr[4];

static void layoutAtualizar(void) {
#ifdef NV_DESKTOP_UI
  modoMobile = janelaW < NV_DESKTOP_BREAKPOINT;
  if (!modoMobile) {
    float scale = fmaxf(NV_DESKTOP_SCALE, 1200.0f / janelaH);
    nv_layout_w = janelaW * scale;
    nv_layout_h = janelaH * scale;
  } else
#endif
  if (modoMobile) {
    float shortEdge = (float)(telaW < telaH ? telaW : telaH);
    nv_layout_w = NV_TELA_BASE_H * (float)telaW / shortEdge;
    nv_layout_h = NV_TELA_BASE_H * (float)telaH / shortEdge;
  } else {
    nv_layout_w = NV_TELA_BASE_W;
    nv_layout_h = NV_TELA_BASE_H;
  }
  nv_inset_e = insetFr[0] * nv_layout_w;
  nv_inset_t = insetFr[1] * nv_layout_h;
  nv_inset_d = insetFr[2] * nv_layout_w;
  nv_inset_b = insetFr[3] * nv_layout_h;
}

void layout_insets_definir(float e, float t, float d, float b) {
  insetFr[0] = e; insetFr[1] = t; insetFr[2] = d; insetFr[3] = b;
  layoutAtualizar();
}

void layout_tela_definir(int width, int height) {
  if (width > 0 && height > 0) {
    telaW = width;
    telaH = height;
    layoutAtualizar();
  }
}

void layout_janela_definir(int width, int height) {
#ifdef NV_DESKTOP_UI
  if (width > 0 && height > 0) { janelaW = width; janelaH = height; layoutAtualizar(); }
#else
  (void)width; (void)height;
#endif
}

void layout_modo_definir(int mobile) {
  modoMobile = mobile != 0;
  layoutAtualizar();
}
int layout_modo_mobile(void) { return modoMobile; }
#else
void layout_janela_definir(int width, int height) { (void)width; (void)height; }
void layout_tela_definir(int width, int height) { (void)width; (void)height; }
void layout_modo_definir(int mobile) { (void)mobile; }
int layout_modo_mobile(void) { return 0; }
void layout_insets_definir(float e, float t, float d, float b) { (void)e; (void)t; (void)d; (void)b; }
#endif

