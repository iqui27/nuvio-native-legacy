#include "nav21.h"
#include "ponteiro.h"
static void target(float,float,float,float,PonteiroFn,PonteiroFn,int,int);
static Uint32 now;
#define SDL_GetTicks() now
#define ponteiro_alvo target
#define ponteiro_ativo() 1
#include "../src/player.c"
#undef ponteiro_alvo
#undef ponteiro_ativo
static int registered;
static void target(float x,float y,float w,float h,PonteiroFn fn,PonteiroFn activate,int a,int b){
  if(fn==ponteiroBotao){nav21_target(a,(GfxRect){x,y,w,h});registered++;}
  ponteiro_alvo(x,y,w,h,fn,activate,a,b);
}
static void open(void){
  reacao_fechar();pausao_fechar();now+=1000;retido=0;aberto=1;saindo=pediuSair=0;comVideo=0;idx=-1;canalSessao=0;epT=epE=-1;skipFoco=0;
  player_shot_estado(now,120,7200,1,PLR_PLAY,0,0);anim=0;
}
static void step(float dt){now+=(Uint32)(dt*1000);player_atualizar(dt,now);}
static void draw(void){nav21_targets_reset();registered=0;player_desenhar(now);plrilha_desenhar(now);}
static int focus(void){return barraFoco?1000:botao;}
static void selectFocus(int id){barraFoco=id==1000;if(!barraFoco)botao=id;}
static int page(void){return aberto;}
static float transition(void){return anim;}
void nav21_player(void){
  Nav21Screen s={"Player/OSD",open,player_evento,step,draw,focus,selectFocus,page,PLR_PLAY,transition};nav21_check(s);
  open();for(int i=0;i<90;i++)step(1.f/60);draw();
  nav21_expect(s.name,"Open: transport controls rendered",registered>0,1);
  SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_UP;player_evento(&e);
  nav21_expect(s.name,"Play, Up: seek bar",barraFoco,1);
  e.key.keysym.sym=SDLK_DOWN;player_evento(&e);
  nav21_expect(s.name,"Seek bar, Down: previous transport focus",focus(),PLR_PLAY);
  e.key.keysym.sym=SDLK_ESCAPE;player_evento(&e);
  nav21_expect(s.name,"Back: controls hidden",visivel,0);
  nav21_expect(s.name,"Back: player retained",aberto&&!saindo,1);
  for(int i=0;i<90;i++)step(1.f/60);
  e.key.keysym.sym=SDLK_DOWN;player_evento(&e);
  nav21_expect(s.name,"Back, Down: controls restored",visivel,1);
  nav21_expect(s.name,"Back, Down: Play focus restored",focus(),PLR_PLAY);
  aberto=0;anim=0;
}
