#include "nav21.h"
#include "../src/ajustes.c"
// Three-island Settings (2.1). Island arrows are semantic (Up/Down in the
// island, Right enters, Left leaves), so the gate checks reachability and
// motion; the filter sheets and the tour keep their own rules.
static int fixture;
static void openFixture(void){
  ajustes_iniciar();frAberta=0;tourAberto=0;
  valor[AJ_ANIM]=0;
  triIniciar();focarSecao(0);triSecFoco=0;
  if(fixture==1)focarOpcao(AJ_DEBRID_RD);
  else if(fixture==2||fixture==3){ffAbrir();if(fixture==3)ffCategoria=FONTEFILTRO_RESOLUCAO;}
  else if(fixture==4)tourAbrir();
}
static void stepFixture(float dt){ajustes_atualizar(dt,SDL_GetTicks());}
static void drawFixture(void){ajustes_desenhar(SDL_GetTicks());for(int i=0;i<navigationCount;i++)nav21_target(navigation[i].id,navigation[i].rect);}
static int focusFixture(void){return navigationFocus();}
static void selectFixture(int id){
  if(id>=200000)ffPont(id-200000,ffCategoria);
  else if(id>=100000)tourPontFoco((id-100000)/2,(id-100000)%2);
  else if(id>=10000)ajPonteiroItem(id-10000,0);
  else if(id>=1000)triFocarSecao(id-1000);
  else triPontChip(id,0);
}
static int ownsArrow(SDL_Keycode k){
  if(tourAberto)return k==SDLK_LEFT || k==SDLK_RIGHT || (k==SDLK_DOWN&&tourFoco+TOUR_COLS<TOUR_N) || (k==SDLK_UP&&tourFoco>=TOUR_COLS);
  return triSetaSemantica(k);
}
static float transitionFixture(void){return screenReveal*ajustes_teste_movimento();}
static int pageFixture(void){return sair?-1:tourAberto?10:frAberta?frAberta*100+ffCategoria+1:triRota()*1000+(triIlha==TRI_LISTA?secAtual:0);}
void nav21_route_settings_open(void){
  fixture=0;openFixture();guiaAbrir(0);
  for(int i=0;i<GUIA_NENT;i++)if(GUIA_ENT[i].tela==GT_BIBLIOTECA){
    gv.ent=i;gv.col=GC_BOTOES;int buttons[3],n=guiaBotoes(i,buttons);
    for(int b=0;b<n;b++)if(buttons[b]==GB_TELA){gv.botao=b;SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_RETURN;ajustes_evento(&e);return;}
  }
  nav21_expect("Router/guide","Library guide action exists",0,1);
}
int nav21_route_settings_focus(void){return guiaAberto*10000+gv.ent*10+gv.botao;}
static int motionScene;
static void internalNext(void){if(motionScene==1)focarOpcao(AJ_DEBRID_RD);else if(motionScene==2)ffAbrir();else if(motionScene==3){ffFoco=4;ffEscolher();}else tourAbrir();}
static void internalBack(void){SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_ESCAPE;ajustes_evento(&e);}
static void islandFrame(void) {
  nav21_targets_reset();txt_novo_quadro();stepFixture(1.f/60);drawFixture();
}
// A section change animates the list for about 220 ms; reduced motion settles at once.
static void islandMotion(void) {
  fixture=0;openFixture();
  for(int i=0;i<90;i++)islandFrame();
  SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_DOWN;ajustes_evento(&e);
  for(int i=0;i<6;i++)islandFrame();
  nav21_expect("Settings/island-motion","Down in sections: the next section's list is still entering",ajustes_teste_transicao(),1);
  nav21_expect("Settings/island-motion","Down in sections: the list shows the next section",secAtual,1);
  for(int i=0;i<30;i++)islandFrame();
  nav21_expect("Settings/island-motion","Settled after 600 ms",ajustes_teste_transicao(),0);
  e.key.keysym.sym=SDLK_RIGHT;ajustes_evento(&e);
  nav21_expect("Settings/island-motion","Right enters the list of the focused section",triIlha==TRI_LISTA && !focoIndice && secAtual==1,1);
  e.key.keysym.sym=SDLK_LEFT;ajustes_evento(&e);
  nav21_expect("Settings/island-motion","Left returns to the same section",triIlha==TRI_SECOES && triSecFoco==1,1);
  valor[AJ_ANIM]=1;e.key.keysym.sym=SDLK_DOWN;ajustes_evento(&e);
  islandFrame();
  nav21_expect("Settings/island-motion","Reduced motion draws one settled list",ajustes_teste_transicao(),0);
}
void nav21_settings(void){islandMotion();const char *names[]={"Settings/UX","Settings/Debrid","Settings/Filters","Settings/Filters-resolution","Settings/tour"};for(fixture=0;fixture<5;fixture++){openFixture();int primary=focusFixture();Nav21Screen s={names[fixture],openFixture,ajustes_evento,stepFixture,drawFixture,focusFixture,selectFixture,pageFixture,primary,transitionFixture,ownsArrow};nav21_check(s);}
  for(motionScene=1;motionScene<5;motionScene++){fixture=motionScene==3?2:0;openFixture();for(int i=0;i<90;i++)stepFixture(1.f/60);Nav21Screen s={names[motionScene],openFixture,ajustes_evento,stepFixture,drawFixture,focusFixture,selectFixture,pageFixture,0,transitionFixture};nav21_motion(s,internalNext,"Open internal sheet: transition >=150ms");nav21_motion(s,internalBack,"Back internal sheet: transition >=150ms");}
  fixture=2;openFixture();ffFoco=4;ffEscolher();ffVoltar();
  nav21_expect("Settings/Filters-back","Filters, Audio, OK, Back: previous category",ffCategoria,-1);nav21_expect("Settings/Filters-back","Filters, Audio, OK, Back: previous focus",ffFoco,4);
  fixture=4;openFixture();for(int i=0;i<90;i++)stepFixture(1.f/60);
  tourFoco=TOUR_COLS-1;tourBtn=1;drawFixture();
  SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_RIGHT;ajustes_evento(&e);
  nav21_expect("Settings/tour-scroll","Last card in row, Right: next row",tourFoco,TOUR_COLS);
}
