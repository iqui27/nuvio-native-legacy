/* #338 review follow-up (job 205): the meter must come back when a NATIVE
 * fullscreen trailer is paused. trailer_tocando() stays latched at 1 while
 * paused (the hole holds the last frame), so main must ask trailer_pausado()
 * too — and the Home highlight (not fullscreen, no pause) keeps hiding the
 * meter. This fixture runs the production trailer.c + desempenho.c with a
 * deterministic video backend and drives the real OK key through
 * trailer_evento. */
#include "trailer.h"
#include "video.h"
#include "plrui.h"
#include "text.h"
#include "layout.h"
#include "ajustes.h"
#include "trailerfonte.h"
#include "desempenho.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int pronto,tocando,ativo,falhou,terminou,conflito;
static double pos;
static float zoom=1;
static Uint32 ticks=100;
Uint32 SDL_GetTicks(void){return ticks;}
int ajustes_trailer_fonte(void){return 0;}
float ajustes_trailer_zoom(void){return zoom;}
int video_iniciar_auto(void){return 1;}
int video_conflito_recurso(void){return conflito;}
int video_tocar(const char *u){(void)u;ativo=1;pronto=tocando=falhou=terminou=0;pos=0;return 1;}
void video_parar(void){ativo=pronto=tocando=0;}
void video_volume(int p){(void)p;}
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
int video_recorte_fonte(void){return 0;}
int video_recorte_fonte_trailer(void){return 0;}
void video_tpk_trailer_marcar(int sim){(void)sim;}
void video_tpk_cartao_marcar(int sim){(void)sim;}
int video_tpk_cartao_cover(void){return 0;}
void video_recorte_reaplicar(void){}
void video_janela(int x,int y,int w,int h){(void)x;(void)y;(void)w;(void)h;}
void video_janela_fonte(int sx,int sy,int sw,int sh,int x,int y,int w,int h){
  (void)sx;(void)sy;(void)sw;(void)sh;(void)x;(void)y;(void)w;(void)h;
}
/* Platform headers above remain the Mac SDK. Only the actual production code
 * below selects the native TPK path in this fixture. */
#undef __APPLE__
#include "../src/desempenho.c"
int ajustes_medidor_desempenho(void){return DS_GRANDE;}
#include "../src/trailer.c"
/* What main.c does every frame: the meter flag is exactly this composition. */
static void quadro(void){ desempenho_trailer(trailer_medidor_esconde()); }
int main(void) {
  SDL_Event ok;
  GfxRect tela={0,0,1920,1080};
  desempenho_amostra(60.0f, 4.0f, 0, 0, 0.0f, 0, 0, 0, 120.0f);
  /* Fullscreen trailer starts, plays: meter hides. */
  trailer_abrir("https://fixture.invalid/fs.mp4",tela,0,1);
  assert(trailer_aberto() && trailer_cheia() && !trailer_medidor_esconde());
  quadro();assert(desempenho_forma()==DS_GRANDE);
  pronto=tocando=1;pos=.5;trailer_atualizar(ticks);
  assert(trailer_tocando() && trailer_medidor_esconde());
  quadro();assert(desempenho_forma()==DS_DESLIGADO);
  puts("ok native fullscreen playing hides the meter");
  /* OK pauses: trailer_tocando stays latched, but the meter comes back. */
  memset(&ok,0,sizeof ok);ok.type=SDL_KEYDOWN;ok.key.keysym.sym=SDLK_RETURN;
  assert(trailer_evento(&ok)==1);
  assert(trailer_pausado() && !video_tocando());
  assert(trailer_tocando());            /* the hole still holds the frame */
  assert(!trailer_medidor_esconde());    /* <- the P2: meter must return */
  quadro();assert(desempenho_forma()==DS_GRANDE);
  puts("ok native fullscreen pause brings the meter back");
  /* OK resumes: meter hides again. */
  assert(trailer_evento(&ok)==1);
  assert(!trailer_pausado() && video_tocando());
  assert(trailer_medidor_esconde());
  quadro();assert(desempenho_forma()==DS_DESLIGADO);
  puts("ok native resume hides the meter again");
  trailer_fechar();
  assert(!trailer_aberto() && !trailer_medidor_esconde());
  quadro();assert(desempenho_forma()==DS_GRANDE);
  puts("ok closing the trailer brings the meter back");
  /* Home highlight is never fullscreen, so OK is not a pause there and the
   * meter stays hidden while it plays (it swaps sources without stopping). */
  trailer_abrir("https://fixture.invalid/home.mp4",(GfxRect){40,300,360,540},0,0);
  assert(trailer_aberto() && !trailer_cheia());
  pronto=tocando=1;pos=.5;trailer_atualizar(++ticks);
  assert(trailer_tocando() && !trailer_pausado());
  assert(trailer_evento(&ok)==0);        /* not fullscreen: key not consumed */
  assert(trailer_medidor_esconde());
  quadro();assert(desempenho_forma()==DS_DESLIGADO);
  puts("ok home highlight keeps the meter hidden while playing");
  trailer_fechar();
  return 0;
}