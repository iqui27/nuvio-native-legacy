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
static int pageFixture(void){return sair?-1:tourAberto?10:frAberta?frAberta*100+ffCategoria+1:aj4Grupos*1000+focoIndice*10000;}
void nav21_settings(void){const char *names[]={"Settings/UX","Settings/Debrid","Settings/Filters","Settings/Filters-resolution","Settings/tour"};for(fixture=0;fixture<5;fixture++){openFixture();int primary=focusFixture();Nav21Screen s={names[fixture],openFixture,ajustes_evento,stepFixture,drawFixture,focusFixture,selectFixture,pageFixture,primary};nav21_check(s);}
  fixture=2;openFixture();ffFoco=4;ffEscolher();ffVoltar();
  nav21_expect("Settings/Filters-back","Filters, Audio, OK, Back: previous category",ffCategoria,-1);nav21_expect("Settings/Filters-back","Filters, Audio, OK, Back: previous focus",ffFoco,4);
  fixture=0;openFixture();aj4Linha=7;tourAbrir();tourSair();nav21_expect("Settings/tour-back","Settings, Tour, Back: previous focus",aj4Linha,7);
}
