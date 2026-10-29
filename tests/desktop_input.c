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
static int opens, saved, focuses, arrows, moves;
static float pixels;
static int allowSwipes=1;
void test_native_init(SDL_Window *w) { (void)w; }
void test_native_sidebar(float x) { (void)x; }
int test_native_dark(void) { return 1; }
int test_native_swipes(void) { return allowSwipes; }
void menu_desktop_expandir(void) { opens++; }
void spainel_abrir(void) { saved++; }
float gfx_escala(void) { return 1; }
void gfx_escala_sair(float x) { (void)x; }
float gfx_opacidade_grupo=1;
void gfx_sem_recorte(void) {}
void gfx_cor(GfxRect r,float radius,float a,float b,float c,float d) { (void)r;(void)radius;(void)a;(void)b;(void)c;(void)d; }
static void focus(int a,int b) { (void)a;(void)b;focuses++; }
static void deliver(const SDL_Event *e) { if(e->type==SDL_KEYDOWN) arrows++; }
static int scrolling(const PonteiroRolagem *e) { if(e->fase==PONT_ROL_MOVER){pixels+=e->delta;moves++;} return 1; }
static void wheel(int x,int y,float dx,float dy) {
  SDL_Event e={0};e.type=SDL_MOUSEWHEEL;e.wheel.which=NV_DESKTOP_PIXEL_WHEEL;
  e.wheel.mouseX=x;e.wheel.mouseY=y;e.wheel.preciseX=dx;e.wheel.preciseY=dy;
  ponteiro_evento(&e,deliver);
}
int main(void) {
  assert(SDL_Init(SDL_INIT_TIMER)==0);
  layout_janela_definir(1280,800);ponteiro_iniciar();ponteiro_teste_janela(1280,800);
  ponteiro_quadro(SDL_GetTicks());
  ponteiro_alvo(100,100,500,500,focus,NULL,0,0);ponteiro_rolagem(scrolling);fecharQuadro();
  SDL_Event e={0};e.type=SDL_MOUSEMOTION;e.motion.x=200;e.motion.y=200;ponteiro_evento(&e,deliver);
  assert(focuses==0);
  wheel(200,200,0,-.25f);wheel(200,200,0,-.25f);
  assert(moves==2 && fabsf(pixels+.75f)<.001f && !inRolagem && !arrows);
  nv_desktop_wheel_until=SDL_GetTicks()+100;
  float bounce=layout_scroll_delta(0,60,0,100);
  assert(bounce<0 && bounce>=-72 && layout_scroll_assentar(bounce,0,100,.016f)==bounce);
  nv_desktop_wheel_until=0;assert(layout_scroll_assentar(bounce,0,100,.016f)>bounce);
  SDL_Delay(190);wheel(10,200,60,0);assert(opens==1 && moves==2);
  SDL_Delay(190);wheel(1275,200,-60,0);assert(saved==1);
  SDL_Delay(190);allowSwipes=0;wheel(10,200,40,0);assert(opens==1 && moves==3);
  e.type=SDL_MOUSEBUTTONDOWN;e.button.x=200;e.button.y=200;e.button.button=SDL_BUTTON_LEFT;
  ponteiro_evento(&e,deliver);assert(focuses==1 && arrows==1);
  puts("PASS: hover preserves focus; fractional pixels; no Mac double momentum; left/right edges; disabled swipe preference; click activation");
  SDL_Quit();return 0;
}
