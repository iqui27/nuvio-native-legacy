#include "nav21.h"
#include "../src/home.c"
static void openFixture(void){
  AddonRemoto addon={0};snprintf(addon.nome,sizeof addon.nome,"Offline");snprintf(addon.url,sizeof addon.url,"https://fixture.invalid");addon.ativo=1;addons_definir_lista(&addon,1);
  detail_fechar_seco();trailercinema_zerar(&heroCinema);
  memset(filEntraEm,0,sizeof filEntraEm);memset(filNAntes,0,sizeof filNAntes);
  focoHero=1;scrollY=0;sair=0;pedidoMenu=0;
  heroAtual=0;heroDesejado=heroPendente=-1;heroAutoDesligado=0;
  nFileiras=2;for(int r=0;r<2;r++){memset(&fileiras[r],0,sizeof fileiras[r]);snprintf(fileiras[r].titulo,sizeof fileiras[r].titulo,"Fixture row %d",r);fileiras[r].tipo=FILEIRA_NORMAL;fileiras[r].n=4;fileiras[r].ini=r*4;}
  int cols[]={4,4};focus_iniciar(&foco,2,cols);
}
static void stepFixture(float dt){home_atualizar(dt,SDL_GetTicks());}
static void drawFixture(void){home_desenhar(SDL_GetTicks());for(int i=0;i<navigationCount;i++)nav21_target((navigation[i].row+1)*1000+navigation[i].col,navigation[i].rect);}
static int focusFixture(void){return focoHero?0:(foco.fileira+1)*1000+foco.coluna;}
static void selectFixture(int id){if(id==0)ponteiroHero(0,0);else ponteiroCard(id/1000-1,id%1000);}
static float transitionFixture(void){return revela_entra(filEntraEm[0],0,SDL_GetTicks());}
static int pageFixture(void){return !sair;}
// Row navigation follows logical order and saved columns, including clipped rows.
static int ownsArrow(SDL_Keycode k){(void)k;return 1;}
static void heroRegression(void){
  openFixture();for(int i=0;i<90;i++)stepFixture(1.f/60);drawFixture();
  SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_RIGHT;
  int next=heroIdxEm(heroPosDe(heroIntencao())+1);
  nav21_expect("Home/hero","Fixture has a next carousel title",next>=0,1);
  home_evento(&e);
  nav21_expect("Home/hero","Hero, Right: hero retains focus",focusFixture(),0);
  nav21_expect("Home/hero","Hero, Right: next carousel title requested",heroIntencao(),next);
  e.key.keysym.sym=SDLK_LEFT;home_evento(&e);
  nav21_expect("Home/hero","Hero, Right, Left: previous carousel title",heroIntencao(),0);
  nav21_expect("Home/hero","Hero, Right, Left: hero retains focus",focusFixture(),0);
  home_evento(&e);nav21_expect("Home/hero","First hero, Left: sidebar requested",pedidoMenu,1);
  ponteiroCard(0,3);drawFixture();e.key.keysym.sym=SDLK_RIGHT;home_evento(&e);
  nav21_expect("Home/row","Last poster, Right: row retained",focusFixture(),1003);
  e.key.keysym.sym=SDLK_LEFT;home_evento(&e);
  nav21_expect("Home/row","Last poster, Left: previous poster",focusFixture(),1002);
}
void nav21_home(void){
  CatItem v[8]={0};for(int i=0;i<8;i++){snprintf(v[i].titulo,sizeof v[i].titulo,"Fixture %d",i);snprintf(v[i].imdb,sizeof v[i].imdb,"tt%07d",i+1);snprintf(v[i].tipo,sizeof v[i].tipo,"movie");snprintf(v[i].sinopse,sizeof v[i].sinopse,"Offline fixture synopsis");}CatFileira rows[2]={0};for(int r=0;r<2;r++){snprintf(rows[r].chave,sizeof rows[r].chave,"fixture-%d",r);snprintf(rows[r].titulo,sizeof rows[r].titulo,"Fixture row %d",r);snprintf(rows[r].tipo,sizeof rows[r].tipo,"movie");rows[r].ini=r*4;rows[r].n=4;}cat_definir_tudo(v,8,rows,2);
  heroRegression();
  Nav21Screen s={"Home 2.1",openFixture,home_evento,stepFixture,drawFixture,focusFixture,selectFixture,pageFixture,0,transitionFixture,ownsArrow};nav21_check(s);
}
