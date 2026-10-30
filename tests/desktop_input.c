#include <assert.h>
#include <stdio.h>
#define janelaW layoutWindowW
#define janelaH layoutWindowH
#include "../src/layout.c"
#undef janelaW
#undef janelaH
#define desktop_native_init test_native_init
#define desktop_native_sidebar test_native_sidebar
#define desktop_native_dark test_native_dark
#define desktop_native_swipes test_native_swipes
#include "../src/ponteiro.c"
#include "../src/rolagemtoque.h"
static int opens, saved, focuses, arrows, moves, contexts, backs, panel;
static float pixels;
static int allowSwipes=1;
void test_native_init(SDL_Window *w) { (void)w; }
void test_native_sidebar(float x,float reveal) { (void)x;(void)reveal; }
int test_native_dark(void) { return 1; }
int test_native_swipes(void) { return allowSwipes; }
void menu_desktop_expandir(void) { opens++; }
void menu_desktop_revelar(void) { opens++; }
void menu_desktop_hover(float x) { (void)x; }
void spainel_abrir(void) { saved++; panel=1; }
int spainel_aberto(void) { return panel; }
void spainel_fechar(void) { panel=0; }
float gfx_escala(void) { return 1; }
void gfx_escala_sair(float x) { (void)x; }
float gfx_opacidade_grupo=1;
void gfx_sem_recorte(void) {}
void gfx_cor(GfxRect r,float radius,float a,float b,float c,float d) { (void)r;(void)radius;(void)a;(void)b;(void)c;(void)d; }
static void focus(int a,int b) { (void)a;(void)b;focuses++; }
static void context(int a,int b) { (void)a;(void)b;contexts++; }
static void deliver(const SDL_Event *e) { if(e->type==SDL_KEYDOWN) { if(e->key.keysym.sym==SDLK_AC_BACK) backs++; else arrows++; } }
static int row, rowStart;
static int scrolling(const PonteiroRolagem *e) {
  if(e->fase==PONT_ROL_INICIO && !e->eixoY && (!row || (rowStart && e->delta>0))) return 0;
  if(e->fase==PONT_ROL_MOVER){pixels+=e->delta;moves++;rowStart=1;}
  return 1;
}
static void phased(int x,int y,float dx,float dy,int phase) {
  SDL_Event e={0};e.type=SDL_MOUSEWHEEL;e.wheel.which=NV_DESKTOP_PIXEL_WHEEL;
  e.wheel.mouseX=x;e.wheel.mouseY=y;e.wheel.preciseX=dx;e.wheel.preciseY=dy;
  // Wire values are also used by the native Cocoa bridge.
  e.wheel.x=NV_WHEEL_PHASED|phase;
  ponteiro_evento(&e,deliver);
}
static void wheel(int x,int y,float dx,float dy) {
  SDL_Event e={0};e.type=SDL_MOUSEWHEEL;e.wheel.which=NV_DESKTOP_PIXEL_WHEEL;
  e.wheel.mouseX=x;e.wheel.mouseY=y;e.wheel.preciseX=dx;e.wheel.preciseY=dy;
  ponteiro_evento(&e,deliver);
}
int main(void) {
  assert(SDL_Init(SDL_INIT_TIMER)==0);
  float offset=100;
  ToqueRolagem realRow={0};
  toquerol_vincular(&realRow,(GfxRect){0,0,1000,500},1,0,500,0,&offset);
  PonteiroRolagem start={PONT_ROL_INICIO,0,20,0,200,200};
  assert(toquerol_evento(&realRow,&start));
  offset=0;assert(!toquerol_evento(&realRow,&start));
  start.delta=-20;assert(toquerol_evento(&realRow,&start));
  start.x=1200;assert(!toquerol_evento(&realRow,&start));
  layout_janela_definir(1280,800);ponteiro_iniciar();ponteiro_teste_janela(1280,800);
  ponteiro_quadro(SDL_GetTicks());
  ponteiro_alvo(100,100,500,500,focus,NULL,0,0);ponteiro_rolagem(scrolling);fecharQuadro();
  SDL_Event e={0};e.type=SDL_MOUSEMOTION;e.motion.x=200;e.motion.y=200;ponteiro_evento(&e,deliver);
  assert(focuses==1 && !arrows && moves==0);
  wheel(200,200,0,-.25f);wheel(200,200,0,-.25f);
  assert(moves==2 && fabsf(pixels+.75f)<.001f && !inRolagem && !arrows);
  nv_desktop_wheel_until=SDL_GetTicks()+100;
  float bounce=layout_scroll_delta(0,60,0,100);
  assert(bounce<0 && bounce>=-72 && layout_scroll_assentar(bounce,0,100,.016f)==bounce);
  nv_desktop_wheel_until=0;assert(layout_scroll_assentar(bounce,0,100,.016f)>bounce);
  SDL_Delay(190);wheel(10,200,60,0);assert(opens==1 && moves==2);
  SDL_Delay(190);wheel(1275,200,-60,0);assert(saved==1);
  SDL_Delay(190);allowSwipes=0;row=1;wheel(10,200,40,0);assert(opens==1 && moves==3);
  allowSwipes=1;panel=0;row=0;
  phased(500,200,0,0,1); // Zero-delta begin still preserves the origin.
  phased(500,200,60,0,0);assert(backs==1);
  SDL_Delay(190);phased(500,200,100,0,0);assert(backs==1);
  phased(500,200,0,0,2);phased(500,200,100,0,8);phased(500,200,0,0,8|2);
  assert(backs==1);
  phased(500,200,60,0,1);assert(backs==2);
  phased(500,200,0,0,2);
  row=1;rowStart=0;int beforeMoves=moves;
  phased(500,200,30,0,1);phased(500,200,60,0,0);
  // Row reached its start during this gesture: it must retain ownership.
  phased(500,200,100,0,8);assert(backs==2 && moves==beforeMoves+3);
  phased(500,200,0,0,8|2);
  phased(500,200,60,0,1);assert(backs==3); // Fresh gesture at row start.
  phased(500,200,0,0,2);
  row=0;
  phased(500,200,-100,0,1);assert(backs==3 && saved==1); // No forward history.
  phased(500,200,0,0,2);
  phased(1275,200,0,0,1);phased(800,200,-60,0,0);assert(saved==2 && panel);
  phased(800,200,-100,0,8);assert(saved==2);
  phased(800,200,0,0,8|2);
  phased(900,200,60,0,1);assert(!panel && backs==3);
  phased(900,200,0,0,2);
  phased(10,200,60,0,1);assert(opens==2 && backs==3);
  phased(10,200,0,0,2);
  phased(500,200,20,0,1);phased(500,200,0,0,4);
  phased(500,200,100,0,8);assert(backs==3);
  phased(500,200,0,-100,1);assert(backs==3);
  phased(500,200,0,0,2);
  e.type=SDL_MOUSEBUTTONDOWN;e.button.x=200;e.button.y=200;e.button.button=SDL_BUTTON_LEFT;
  ponteiro_evento(&e,deliver);assert(focuses==2 && arrows==1);
  e.button.button=SDL_BUTTON_RIGHT;ponteiro_evento(&e,deliver);
  assert(focuses==3 && arrows==2 && !ponteiro_ok_longo());
  ponteiro_quadro(SDL_GetTicks());
  ponteiro_alvo(100,100,500,500,focus,NULL,0,0);
  ponteiro_alvo_segurar(context);fecharQuadro();
  ponteiro_evento(&e,deliver);
  assert(contexts==1 && focuses==3 && arrows==2);
  puts("PASS: hover updates focus without scrolling; fractional pixels; no Mac double momentum; left/right edges; one Back per phase sequence; row ownership; panel reverse; cancelled momentum; disabled swipe preference; click activation");
  SDL_Quit();return 0;
}
