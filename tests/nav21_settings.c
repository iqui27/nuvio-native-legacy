#include "nav21.h"
#include "../src/ajustes.c"
static int fixture;
static void openFixture(void){
  ajustes_iniciar();frAberta=0;tourAberto=0;
  valor[AJ_ANIM]=0;
  if(fixture==1){valor[AJ_AVANCADAS]=0;focarOpcao(AJ_DEBRID_RD);}
  else if(fixture==2||fixture==3){ffAbrir();if(fixture==3)ffCategoria=FONTEFILTRO_RESOLUCAO;}
  else if(fixture==4)tourAbrir();
  else {aj4Iniciar();pfTela=0;focoIndice=1;}
}
static void stepFixture(float dt){ajustes_atualizar(dt,SDL_GetTicks());}
static void drawFixture(void){ajustes_desenhar(SDL_GetTicks());for(int i=0;i<navigationCount;i++)nav21_target(navigation[i].id,navigation[i].rect);}
static int focusFixture(void){return navigationFocus();}
static void selectFixture(int id){if(id>=200000)ffPont(id-200000,ffCategoria);else if(id>=100000)tourPontFoco((id-100000)/2,(id-100000)%2);else aj4PontFoco(id/10000,id%10000);}
static int ownsArrow(SDL_Keycode k){return tourAberto && (k==SDLK_LEFT || k==SDLK_RIGHT || (k==SDLK_DOWN&&tourFoco+TOUR_COLS<TOUR_N) || (k==SDLK_UP&&tourFoco>=TOUR_COLS));}
static float transitionFixture(void){return screenReveal;}
static int pageFixture(void){return sair?-1:tourAberto?10:frAberta?frAberta*100+ffCategoria+1:aj4Grupos*1000+focoIndice*10000;}
void nav21_route_settings_open(void){
  fixture=0;openFixture();aj4Linha=7;guiaAbrir(0);
  for(int i=0;i<GUIA_NENT;i++)if(GUIA_ENT[i].tela==GT_BIBLIOTECA){
    gv.ent=i;gv.col=GC_BOTOES;int buttons[3],n=guiaBotoes(i,buttons);
    for(int b=0;b<n;b++)if(buttons[b]==GB_TELA){gv.botao=b;SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_RETURN;ajustes_evento(&e);return;}
  }
  nav21_expect("Router/guide","Library guide action exists",0,1);
}
int nav21_route_settings_focus(void){return aj4Linha*100000+guiaAberto*10000+gv.ent*10+gv.botao;}
static int motionScene;
static void internalNext(void){if(motionScene==1)focarOpcao(AJ_DEBRID_RD);else if(motionScene==2)ffAbrir();else if(motionScene==3){ffFoco=4;ffEscolher();}else tourAbrir();}
static void internalBack(void){SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_ESCAPE;ajustes_evento(&e);}
void nav21_settings(void){const char *names[]={"Settings/UX","Settings/Debrid","Settings/Filters","Settings/Filters-resolution","Settings/tour"};for(fixture=0;fixture<5;fixture++){openFixture();int primary=focusFixture();Nav21Screen s={names[fixture],openFixture,ajustes_evento,stepFixture,drawFixture,focusFixture,selectFixture,pageFixture,primary,transitionFixture,ownsArrow};nav21_check(s);}
  for(motionScene=1;motionScene<5;motionScene++){fixture=motionScene==3?2:0;openFixture();for(int i=0;i<90;i++)stepFixture(1.f/60);Nav21Screen s={names[motionScene],openFixture,ajustes_evento,stepFixture,drawFixture,focusFixture,selectFixture,pageFixture,0,transitionFixture};nav21_motion(s,internalNext,"Open internal sheet: transition >=150ms");nav21_motion(s,internalBack,"Back internal sheet: transition >=150ms");}
  fixture=2;openFixture();ffFoco=4;ffEscolher();ffVoltar();
  nav21_expect("Settings/Filters-back","Filters, Audio, OK, Back: previous category",ffCategoria,-1);nav21_expect("Settings/Filters-back","Filters, Audio, OK, Back: previous focus",ffFoco,4);
  fixture=4;openFixture();for(int i=0;i<90;i++)stepFixture(1.f/60);
  tourFoco=TOUR_COLS-1;tourBtn=1;drawFixture();
  SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_RIGHT;ajustes_evento(&e);
  nav21_expect("Settings/tour-scroll","Last card in row, Right: next row",tourFoco,TOUR_COLS);
  nav21_expect("Settings/tour-scroll","Last card in row, Right: first button",tourBtn,0);
  e.key.keysym.sym=SDLK_LEFT;ajustes_evento(&e);
  nav21_expect("Settings/tour-scroll","First card in row, Left: previous row",tourFoco,TOUR_COLS-1);
  fixture=0;openFixture();aj4Linha=7;tourAbrir();tourSair();nav21_expect("Settings/tour-back","Settings, Tour, Back: previous focus",aj4Linha,7);
}
