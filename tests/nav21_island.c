#include "nav21.h"
#include "ponteiro.h"
static void target(float,float,float,float,PonteiroFn,PonteiroFn,int,int);
#define ponteiro_alvo target
#include "../src/ilha.c"
#undef ponteiro_alvo
static GfxRect rects[3];
static int registered;
static Uint32 now;
static void target(float x,float y,float w,float h,PonteiroFn fn,PonteiroFn activate,int a,int b){
  if(fn==pontFoco&&a>=0&&a<3){rects[a]=(GfxRect){x,y,w,h};registered|=1<<a;nav21_target(a,rects[a]);}
  ponteiro_alvo(x,y,w,h,fn,activate,a,b);
}
void nav21_island_draw(Uint32 tick){ilha_desenhar(tick);}
static void step(float dt){now+=(Uint32)(dt*1000);nav21_targets_reset();ilha_desenhar(now);}
static void draw(void){nav21_targets_reset();registered=0;ilha_desenhar(now);}
static void key(const SDL_Event *e){ilha_evento(e);}
static int focus(void){return modalFoco;}
static void selectFocus(int id){pontFoco(id,0);}
static int page(void){return modalAberto;}
static float transition(void){return modalT;}
static void open(void){
  ilha_abrir_sessao();ilha_coberta(0);ilha_relogio_visivel(1);ilha_posicionar(1);now=SDL_GetTicks()+1000;
  IlhaModal m={0};snprintf(m.titulo,sizeof m.titulo,"Offline clock modal");m.nBotoes=3;
  for(int i=0;i<3;i++)snprintf(m.botao[i],sizeof m.botao[i],"Choice %d",i);
  IlhaAvisoEx a={0};a.chave="nav21-clock";a.texto="Offline notice";a.modal=&m;a.ms=60000;
  ilha_avisar_ex(&a);for(int i=0;i<90;i++)step(1.f/60);
  SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_PAGEUP;ilha_evento(&e);
}
void nav21_island(void){
  Nav21Screen s={"Clock island/modal",open,key,step,draw,focus,selectFocus,page,0,transition};nav21_check(s);
  open();for(int i=0;i<90;i++){step(1.f/60);draw();}
  nav21_expect(s.name,"Open: all three buttons rendered",registered,7);
  SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_ESCAPE;key(&e);
  nav21_expect(s.name,"Back: modal dismissed",modalAberto,0);
  nav21_expect(s.name,"Back: clock retained",relogioQuer,1);
  nav21_expect(s.name,"Back: no unrelated route request",pedido,0);
  ilha_modal_fechar(1);ilha_abrir_sessao();
}
