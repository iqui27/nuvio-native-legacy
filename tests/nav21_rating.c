#include "nav21.h"
#include "ponteiro.h"
static void target(float,float,float,float,PonteiroFn,PonteiroFn,int,int);
#define ponteiro_alvo target
#include "../src/reacao.c"
#undef ponteiro_alvo
static CatItem item;
static Uint32 now;
static void target(float x,float y,float w,float h,PonteiroFn fn,PonteiroFn activate,int a,int b){
  if(fn==ponteiroResposta)nav21_target(a,(GfxRect){x,y,w,h});
  ponteiro_alvo(x,y,w,h,fn,activate,a,b);
}
static void open(void){reacao_fechar();snprintf(item.imdb,sizeof item.imdb,"tt9000021");strcpy(item.tipo,"movie");strcpy(item.titulo,"Offline rating");reacao_detalhe_abrir(&item);now=SDL_GetTicks();}
static void step(float dt){now+=(Uint32)(dt*1000);reacao_detalhe_dica(&item,1);}
static void draw(void){nav21_targets_reset();reacao_desenhar(now,NV_TELA_H-48.f);}
static void key(const SDL_Event *e){reacao_evento(e,0);}
static int focus(void){return c.foco;}
static void selectFocus(int id){c.foco=id;}
static int page(void){return c.aberto;}
static float transition(void){return c.anim;}
void nav21_rating(void){
  Nav21Screen s={"Rating island/Detail",open,key,step,draw,focus,selectFocus,page,0,transition};nav21_check(s);
  open();SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_UP;key(&e);
  nav21_expect(s.name,"Up: star row",c.estrelas,1);int previous=c.nota;
  e.key.keysym.sym=SDLK_LEFT;key(&e);nav21_expect(s.name,"Up, Left: half-star decrement",c.nota,previous-1);
  e.key.keysym.sym=SDLK_DOWN;key(&e);nav21_expect(s.name,"Stars, Down: response row",c.estrelas,0);
  e.key.keysym.sym=SDLK_ESCAPE;key(&e);nav21_expect(s.name,"Back: dismiss without rating",c.aberto,0);
  nav21_expect(s.name,"Back: saved rating unchanged",reacao_nota(item.imdb),0);
  reacao_fechar();
}
