#include "nav21.h"
#include "../src/comecar.c"
static int scene;
static void openFixture(void){comecar_teste_esquecer();comecar_abrir();snprintf(dados.nome,sizeof dados.nome,"Fixture");passo=scene;foco=foco_padrao();}
static void stepFixture(float dt){comecar_atualizar(dt,SDL_GetTicks());}
static void drawFixture(void){comecar_desenhar(SDL_GetTicks());Item items[MAX_ITENS];int n=montar(items);for(int i=0;i<n;i++)nav21_target(items[i].id,items[i].r);}
static void selectFixture(int i){foco=i;}
static int pageFixture(void){return aberto?passo:-1;}
void nav21_start(void){const char *names[]={"Start/profile","Start/import","Start/tracking","Start/finish"};for(scene=0;scene<4;scene++){Nav21Screen s={names[scene],openFixture,comecar_evento,stepFixture,drawFixture,comecar_foco,selectFixture,pageFixture,B_PRINC};nav21_check(s);}
  for(int i=0;i<3;i++){scene=i;openFixture();foco=i==0?I_COR0:I_TV;int saved=foco;ir_passo(i+1);SDL_Event back={0};back.type=SDL_KEYDOWN;back.key.keysym.sym=SDLK_ESCAPE;comecar_evento(&back);nav21_expect("Start/back","Step, choose control, Next, Back: previous step",passo,i);nav21_expect("Start/back","Step, choose control, Next, Back: previous focus",foco,saved);}
}
