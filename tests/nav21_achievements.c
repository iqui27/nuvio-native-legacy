#include "nav21.h"
#include "../src/conquistas.c"
static int scene;
static void openFixture(void) {
  conquistas_teste_esquecer();conquistas_abrir();
  if(scene==1){view=V_FAM;foco=F_CARD0;}
  if(scene==2){view=V_DET;foco=B_FECHAR;}
}
static void stepFixture(float dt){conquistas_atualizar(dt,SDL_GetTicks());}
static void drawFixture(void){
  conquistas_desenhar(SDL_GetTicks());
  Item items[MAX_ITENS];int n=montar(items);for(int i=0;i<n;i++)nav21_target(items[i].id,items[i].r);
  if(view==V_HUB)for(int i=0;i<nvis;i++)if(!card_visivel(i))nav21_target(C_CARD0+i,r_card(i));
}
static void selectFixture(int i){foco=i;garantir_visivel();}
static float transitionFixture(void){return scene==2?detP:entrada;}
static int pageFixture(void){return aberto?view:-1;}
static float detailTransition(void){return detP;}
static void internalNext(void){ok();}
static void internalBack(void){SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_ESCAPE;conquistas_evento(&e);}
void nav21_achievements(void){
  CqEstado state={0};for(int i=0;i<10;i++){char id[40];snprintf(id,sizeof id,"dir:fixture%d",i);cqregra_registrar(&state,id,CQ_FAM_DIRETOR,"Fixture",2,7,12,1700000000L);}cqregra_marcar_avisados(&state);cqd_gravar(&state);
  const char *names[]={"Achievements/grid","Achievements/families","Achievements/detail"};
  for(scene=0;scene<3;scene++){Nav21Screen s={names[scene],openFixture,conquistas_evento,stepFixture,drawFixture,conquistas_foco,selectFixture,pageFixture,scene==0?C_CARD0:scene==1?F_CARD0:B_FECHAR,transitionFixture};nav21_check(s);}
  scene=0;openFixture();for(int i=0;i<90;i++)stepFixture(1.f/60);foco=C_CARD0+2;Nav21Screen s={"Achievements/internal-detail",openFixture,conquistas_evento,stepFixture,drawFixture,conquistas_foco,selectFixture,pageFixture,0,detailTransition};nav21_motion(s,internalNext,"Grid, card, OK: detail transition >=150ms");nav21_motion(s,internalBack,"Detail, Back: transition >=150ms");
  nav21_expect("Achievements/back","Grid, card 2, OK, Back: previous grid",view,V_HUB);nav21_expect("Achievements/back","Grid, card 2, OK, Back: previous focus",foco,C_CARD0+2);
}
