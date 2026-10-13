// Real crop, video plane and gfx_furo; only GL calls are instrumented.
#define main old_roi_main
#include "tpk-roi.c"
#undef main
#include "gfx.h"
#include "layout.h"
#include "trailer.h"
#include <math.h>

static int telaW=1920, telaH=1080, dinPendente, ambPendente, ambIntacta;
static int recorteAtivo, recorteBox[4], scissor[4], scissorOn;
static float escAtiva=1.0f, subAtual[4]={0,0,1,1}, clearAlpha;
int gfx_n_rect, gfx_n_cheio, gfx_rastro_grandes;
double gfx_fill, gfx_fill_modo[GFX_NMODOS];
unsigned long long gfx_modos_desligados;
static unsigned char alpha[1080][1920];
static void dinDescarregar(void) {}
void gfx_ambiente_descarregar(void) {}
static void probeEnable(GLenum cap) { assert(cap==GL_SCISSOR_TEST); scissorOn=1; }
static void probeDisable(GLenum cap) { assert(cap==GL_SCISSOR_TEST); scissorOn=0; }
static void probeScissor(int x,int y,int w,int h) {
  scissor[0]=x; scissor[1]=y; scissor[2]=w; scissor[3]=h;
}
static void probeColor(float r,float g,float b,float a) { (void)r;(void)g;(void)b;clearAlpha=a; }
static void probeClear(GLbitfield mask) {
  assert(mask==GL_COLOR_BUFFER_BIT && scissorOn);
  for (int y=0;y<1080;y++) for (int x=0;x<1920;x++)
    if (x>=scissor[0] && x<scissor[0]+scissor[2] && y>=scissor[1] && y<scissor[1]+scissor[3])
      alpha[1079-y][x]=(unsigned char)(clearAlpha*255);
}
#define glEnable probeEnable
#define glDisable probeDisable
#define glScissor probeScissor
#define glClearColor probeColor
#define glClear probeClear
#define GFX_TR_RECT(...) ((void)0)
#define GFX_OUTRO_INI() ((void)0)
#define GFX_OUTRO_FIM() ((void)0)
#include "tpk-card-hole-real.h"

int main(void) {
  nv_tpk_video_registrar(hAbrir,hParar,hInt,hInt,hInt,hJanela,hPos);
  cartaoExpandido(); // Round 5: the 606x340 final plane still determines the hole.
  video_tocar("http://x/apple-card.m3u8");
  video_tpk_trailer_marcar(1); video_tpk_cartao_marcar(1);
  video_tpk_tizen_definir("6.0");
  nv_tpk_video_evento(6,1920,804);
  int sx,sy,sw,sh;
  trailer_recorte(1920,804,1.0f,606,341,&sx,&sy,&sw,&sh);
  video_janela_fonte(sx,sy,sw,sh,40,300,606,341);
  confere("Apple cover plane may extend beyond the card",-63,300,815,341);
  GfxRect hole;
  video_tpk_cartao_furo(40,300,606,341,&hole.x,&hole.y,&hole.w,&hole.h);
  memset(alpha,255,sizeof alpha);
  gfx_furo(hole);
  int opaque=alpha[470][20]==255;
  printf("%s Apple cover: pixel (20,470) stays opaque\n",opaque ? "ok  " : "FALHA");
  assert(opaque);
  assert(hole.x==40 && hole.y==300 && hole.w==606 && hole.h==341);
  assert(alpha[470][300]==0);
  video_recorte_reaplicar();
  confere("Apple cover replay keeps its plane",-63,300,815,341);
  assert(falhas==0);
  puts("tpk-card-hole: PASS");
  return 0;
}
