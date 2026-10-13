/* Native production trailer.c, with a deterministic video backend boundary.
 * Prepared is NOT Playing; buffering/paused after a real start keeps the last
 * frame visible, and a new source can never inherit that sticky state. */
#include "trailer.h"
#include "video.h"
#include "plrui.h"
#include "text.h"
#include "layout.h"
#include "ajustes.h"
#include "trailerfonte.h"
#include <assert.h>
#include <stdio.h>
static int pronto,tocando,ativo,falhou,terminou,conflito;
static double pos;
static int recorte, cropN, fonteN;
static float zoom=1;
static Uint32 ticks=100;
Uint32 SDL_GetTicks(void){return ticks;}
int ajustes_trailer_fonte(void){return 0;}
float ajustes_trailer_zoom(void){return zoom;}
int video_iniciar_auto(void){return 1;}
int video_conflito_recurso(void){return conflito;}
int video_tocar(const char *u){(void)u;ativo=1;pronto=tocando=falhou=terminou=0;pos=0;return 1;}
void video_parar(void){ativo=pronto=tocando=0;}
static int volN,volUlt=-1;
void video_volume(int p){volN++;volUlt=p;}
void video_pausar(int p){tocando=!p;}
int video_ativo(void){return ativo;}
int video_pronto(void){return pronto;}
int video_tocando(void){return tocando;}
int video_falhou(void){return falhou;}
int video_terminou(void){return terminou;}
int video_largura(void){return 1920;}
int video_altura(void){return 1080;}
double video_pos(void){return pos;}
void video_bombear(void){}
int video_recorte_fonte(void){return recorte;}
int video_recorte_fonte_trailer(void){return recorte;}   // NV_TPK: trailer.c asks for the trailer-only setting
void video_tpk_trailer_marcar(int sim){(void)sim;}
void video_tpk_cartao_marcar(int sim){(void)sim;}
static int cover;
int video_tpk_cartao_cover(void){return cover;}
void video_recorte_reaplicar(void){}
void video_janela(int x,int y,int w,int h){(void)x;(void)y;(void)w;(void)h;}
void video_janela_fonte(int sx,int sy,int sw,int sh,int x,int y,int w,int h){
  fonteN++;
  if (sx || sy || sw < 1920 || sh < 1080) cropN++;
  (void)x;(void)y;(void)w;(void)h;
}
/* Platform headers above remain the Mac SDK. Only the actual production
 * trailer implementation below selects its native TPK path in this fixture. */
#undef __APPLE__
#include "../src/trailer.c"
int main(void) {
  GfxRect r={0,0,1920,1080};
  trailer_abrir("https://fixture.invalid/a.mp4",r,0,0);assert(trailer_aberto());
  assert(!trailer_tocando());pronto=1;
  assert(!trailer_tocando());assert(!trailer_mostra_video());
  puts("ok native Prepared alone never counts as playback");
  tocando=1;trailer_atualizar(ticks);assert(trailer_tocando());
  assert(!trailer_mostra_video());pos=.5;ticks+=800;
  trailer_atualizar(ticks);assert(trailer_mostra_video());
  puts("ok native Playing then advancing position reveals the image");
  tocando=0;trailer_atualizar(++ticks);
  assert(trailer_tocando()&&trailer_mostra_video());
  puts("ok pause/buffering after first Playing keeps last image");
  trailer_fechar();assert(!trailer_tocando());
  trailer_abrir("https://fixture.invalid/b.mp4",r,0,0);pronto=1;
  assert(!trailer_tocando()&&!trailer_mostra_video());
  puts("ok new source never inherits Playing/image from previous source");
  tocando=1;trailer_atualizar(++ticks);falhou=1;
  assert(!trailer_tocando());trailer_atualizar(++ticks);
  assert(!trailer_aberto()&&trailer_falhou());
  puts("ok native terminal failure closes the trailer");
  /* Sent at an even SDL tick, t|1 is one millisecond ahead. Unsigned age
   * subtraction used to wrap and skip the 300 ms ROI settling period. */
  ticks=2000;recorte=1;r.h=500;
  trailer_abrir("https://fixture.invalid/c.mp4",r,0,0);pronto=tocando=1;pos=.5;
  trailer_atualizar(ticks);ticks+=800;trailer_atualizar(ticks);
  assert(!trailer_mostra_video());ticks+=299;assert(!trailer_mostra_video());
  ticks+=2;assert(trailer_mostra_video());
  puts("ok native ROI wait never wraps through a future/even tick");
  trailer_fechar();
  /* #281: the volume goes at opening (Player still preparing on the host) and
   * once more at the source's first Playing; never every frame. */
  volN=0;volUlt=-1;tocando=0;pronto=0;
  trailer_abrir("https://fixture.invalid/d.mp4",r,1,0);
  assert(volN==1&&volUlt==100);
  trailer_atualizar(++ticks);assert(volN==1);
  pronto=tocando=1;trailer_atualizar(++ticks);assert(volN==2&&volUlt==100);
  trailer_atualizar(++ticks);trailer_atualizar(++ticks);assert(volN==2);
  trailer_continuar(r,0);assert(volN==3&&volUlt==0);
  trailer_fechar();
  tocando=0;trailer_abrir("https://fixture.invalid/e.mp4",r,1,0);
  tocando=1;trailer_atualizar(++ticks);assert(volN==5&&volUlt==100);
  puts("ok native trailer volume re-sent once at first Playing, per source");
  trailer_fechar();
  /* P1: the detail adopts a playing card before marking its new owner.
   * With experimental zoom off, that must never crop the fullscreen target. */
  recorte=0;zoom=1.34f;cropN=0;ticks=10000;
  trailer_no_cartao(1);
  trailer_abrir("https://fixture.invalid/card.mp4",(GfxRect){40,300,360,540},0,0);
  pronto=tocando=1;pos=.5;trailer_atualizar(ticks);
  ticks+=800;trailer_atualizar(ticks);
  int cardCrops=cropN, cardRequests=fonteN;
  assert(trailer_continuar((GfxRect){0,0,1920,1080},0));
  trailer_marcar_dono(TRAILER_DONO_DETALHE,"tt471");
  ticks+=1500;trailer_atualizar(ticks);
  assert(cropN==cardCrops && fonteN==cardRequests);
  puts("ok P1 card adoption never crops fullscreen with experimental zoom off");
  trailer_fechar();
  /* P3: a card that will only be letterboxed reveals the plane at once; a card
   * covered by a source crop (Tizen 6.x) waits for that crop to settle. */
  recorte=0;cover=0;zoom=1;ticks=20000;
  trailer_no_cartao(1);
  trailer_abrir("https://fixture.invalid/lb.mp4",(GfxRect){40,300,360,540},0,0);
  pronto=tocando=1;pos=.5;trailer_atualizar(ticks);
  ticks+=800;trailer_atualizar(ticks);
  assert(trailer_mostra_video());
  trailer_fechar();
  cover=1;ticks=30000;
  trailer_no_cartao(1);
  trailer_abrir("https://fixture.invalid/cv.mp4",(GfxRect){40,300,360,540},0,0);
  pronto=tocando=1;pos=.5;trailer_atualizar(ticks);
  ticks+=800;trailer_atualizar(ticks);
  assert(!trailer_mostra_video());
  ticks+=301;assert(trailer_mostra_video());
  trailer_no_cartao(0);cover=0;
  puts("ok P3 card reveal: letterbox at once, cover waits for the crop");
  trailer_fechar();
  return 0;
}
