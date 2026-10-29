// Reuse the real video_tpk.c fixture's host/network stubs.
#define main old_roi_main
#include "tpk-roi.c"
#undef main

void nv_tpk_video_registrar_aspecto(void (*)(int,int,int,int,int,int,int,int));
static int lx,ly,lw,lh;
static void hAspecto(int x,int y,int w,int h,int a,int b,int c,int d) {
  hJanela(x,y,w,h); lx=a;ly=b;lw=c;lh=d;
}
static void check(int x,int y,int w,int h,int a,int b,int c,int d) {
  confere("B bridge",x,y,w,h);
  assert(lx==a && ly==b && lw==c && lh==d);
}
int main(void) {
  nv_tpk_video_registrar(hAbrir,hParar,hInt,hInt,hInt,hJanela,hPos);
  nv_tpk_video_registrar_aspecto(hAspecto);
  video_tocar("http://test/movie");
  assert(video_recorte_fonte()); // All eight player modes, without opt-in.
  assert(!video_recorte_fonte_trailer());
  // Original / Fill: source is whole; target is contain / stretched.
  const int sizes[][2]={{1920,1080},{1912,800},{1440,1080}};
  const int contain[][4]={{0,0,1920,1080},{0,138,1920,804},{240,0,1440,1080}};
  for(int i=0;i<3;i++) {
    nv_tpk_video_evento(6,sizes[i][0],sizes[i][1]);
    video_janela_fonte(0,0,sizes[i][0],sizes[i][1],contain[i][0],contain[i][1],contain[i][2],contain[i][3]);
    confere("Original",contain[i][0],contain[i][1],contain[i][2],contain[i][3]);
    video_janela_fonte(0,0,sizes[i][0],sizes[i][1],0,0,1920,1080);
    confere("Fill",0,0,1920,1080);
  }
  // Zoom: production source crop is integer/even; preserve its exact scale.
  nv_tpk_video_evento(6,1912,800);
  video_pausar(1); nv_tpk_video_evento(3,0,0);
  int beforePausedCrop = nj;
  video_janela_fonte(244,0,1422,800,0,0,1920,1080);
  assert(nj == beforePausedCrop + 1 && !video_tocando());
#ifdef NV_TPK40
  check(-328,0,2582,1080,-328,0,2582,1080);
#else
  check(-328,0,2582,1080,0,0,1920,1080);
#endif
  video_recorte_reaplicar();
#ifdef NV_TPK40
  check(-328,0,2582,1080,-328,0,2582,1080);
#else
  check(-328,0,2582,1080,0,0,1920,1080);
#endif
  // P1: a new destination invalidates the cached card/player crop.
  video_janela(0,0,1920,1080);
  nj=0; video_recorte_reaplicar(); assert(nj==0);
  nv_tpk_video_evento(6,1440,1080);
  video_janela_fonte(0,134,1440,810,0,0,1920,1080);
#ifdef NV_TPK40
  check(0,-178,1920,1440,0,-178,1920,1440);
#else
  check(0,-178,1920,1440,0,0,1920,1080);
#endif
  // A cached crop can never be replayed into another player generation.
  nj=0; video_tocar("http://test/next"); video_recorte_reaplicar(); assert(nj==0);
  nv_tpk_video_evento(6,1920,1080);
  video_janela_fonte(480,270,960,540,0,0,1920,1080);
  nj=0; video_parar(); video_recorte_reaplicar(); assert(nj==0);
  assert(video_largura()==0 && video_altura()==0);
  // A pending live open cannot use the previous stream's source crop.
  video_janela_fonte(0,0,1280,720,0,0,1920,1080);
  confere("stopped stream stays full-frame",0,0,1920,1080);
  // The trailer's legacy gate remains separate from the player.
  video_tocar("http://test/trailer"); video_tpk_trailer_marcar(1);
  nv_tpk_video_evento(6,1920,1080);
  video_janela_fonte(480,270,960,540,0,0,1920,1080);
#ifndef NV_TPK40
  confere("trailer opt-in unchanged",0,0,1920,1080);
#endif
  // P1: only the card trailer is fitted; others keep the raw destination.
  nv_tpk_video_evento(6,1440,1080);
  video_janela_fonte(0,0,1440,1080,0,0,1920,1080);
  confere("P1 non-card trailer keeps fullscreen",0,0,1920,1080);
  video_tpk_cartao_marcar(1);
  // P2: a vertical trailer cannot cover 960x540 within a 1080-high panel.
  nv_tpk_video_evento(6,1080,1920);
  cartaoDaHome("P2 aspect bridge: plane equals hole",0,656,1080,606,400,200,960,540);
  confere("P2 portrait fit with aspect bridge",728,200,304,540);
  nj=0; video_recorte_reaplicar(); assert(nj==0);
  nv_tpk_video_evento(6,1920,1080);
  cartaoDaHome("aspect bridge: plane equals hole",600,0,720,1080,40,300,360,540);
  cartaoDaHome("aspect bridge: whole-frame plane equals hole",0,0,1920,1080,40,300,360,540);
  confere("unproven family uses proportional fit",40,469,360,202);
  cartaoExpandido();
  assert(falhas==0);
  puts("tpk-aspecto-roi: PASS"); return 0;
}
