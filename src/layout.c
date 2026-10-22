#include "layout.h"

#ifdef NV_TOUCH_UI
float nv_layout_w = NV_TELA_BASE_W;
float nv_layout_h = NV_TELA_BASE_H;
static int telaW = (int)NV_TELA_BASE_W;
static int telaH = (int)NV_TELA_BASE_H;
static int modoMobile;

static void layoutAtualizar(void) {
  if (modoMobile) {
    float shortEdge = (float)(telaW < telaH ? telaW : telaH);
    nv_layout_w = NV_TELA_BASE_H * (float)telaW / shortEdge;
    nv_layout_h = NV_TELA_BASE_H * (float)telaH / shortEdge;
  } else {
    nv_layout_w = NV_TELA_BASE_W;
    nv_layout_h = NV_TELA_BASE_H;
  }
}

void layout_tela_definir(int width, int height) {
  if (width > 0 && height > 0) {
    telaW = width;
    telaH = height;
    layoutAtualizar();
  }
}

void layout_modo_definir(int mobile) {
  modoMobile = mobile != 0;
  layoutAtualizar();
}
int layout_modo_mobile(void) { return modoMobile; }
#else
void layout_tela_definir(int width, int height) { (void)width; (void)height; }
void layout_modo_definir(int mobile) { (void)mobile; }
int layout_modo_mobile(void) { return 0; }
#endif

