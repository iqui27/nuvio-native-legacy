#include "nav21.h"
static int authenticated;
static int fixtureConnected(void){return authenticated;}
#define plex_conectado fixtureConnected
#define jellyfin_conectado fixtureConnected
#define emby_conectado fixtureConnected
#include "../src/srvsetup.c"
#include <string.h>
static int fixture;
static Uint32 clockFixture;
static void openFixture(void) {
  opened=1;closing=0;historyCount=0;transitionAt=clockFixture=SDL_GetTicks();screen=fixture;provider=1;selected=0;profile=perfis_ativo();
  worker.n=2;worker.alive=0;nLibraries=3;rowChoice=0;recoveryAuth=0;codeSince=SDL_GetTicks();
  for(int i=0;i<3;i++) {snprintf(libraries[i].titulo,sizeof libraries[i].titulo,"Library %d",i);snprintf(libraries[i].chave,sizeof libraries[i].chave,"jellyfin_fixture%d",i);}
}
static void internalNext(void){change((screen+1)%(ROWS+1));}
static void internalBack(void){SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_ESCAPE;srvsetup_evento(&e);clockFixture=SDL_GetTicks();}
static int focusFixture(void) {return selected;}
static void selectFixture(int i) {selected=i;}
static float transitionFixture(void){return closing?1-fminf(1,(clockFixture-transitionAt)/240.f):fminf(1,(clockFixture-transitionAt)/240.f);}
static int pageFixture(void) {return opened?screen:-1;}
static void stepFixture(float dt) {clockFixture+=(Uint32)(dt*1000);srvsetup_atualizar(clockFixture);}
static void keyFixture(const SDL_Event *e) {clockFixture=SDL_GetTicks();srvsetup_evento(e);}
static void drawFixture(void) {srvsetup_desenhar(clockFixture);for(int i=0;i<navigationCount;i++)nav21_target(navigationIds[i],navigationRects[i]);}
void nav21_servers(void) {
  const char *names[]={"Servers/choose","Servers/discover","Servers/address","Servers/login","Servers/users","Servers/password","Servers/code","Servers/connect-code","Servers/connect-password","Servers/connect-servers","Servers/plex-servers","Servers/libraries","Servers/ready","Servers/manage","Servers/remove","Servers/error","Servers/rows"};
  for(fixture=0;fixture<=ROWS;fixture++) {
    Nav21Screen s={names[fixture],openFixture,keyFixture,stepFixture,drawFixture,focusFixture,selectFixture,pageFixture,0,transitionFixture};
    nav21_check(s);
  }
  for(int previous=CHOOSE;previous<=ROWS;previous++){
    openFixture();screen=previous;selected=1;
    Nav21Screen s={names[previous],openFixture,keyFixture,stepFixture,drawFixture,focusFixture,selectFixture,pageFixture,0,transitionFixture};
    nav21_motion(s,internalNext,"Next: internal page transition >=150ms");
    nav21_motion(s,internalBack,"Back: internal page transition >=150ms");
    nav21_expect(names[previous],"Open subpage, Back: previous page",screen,previous);
    nav21_expect(names[previous],"Open subpage, Back: previous focus",selected,1);
  }
  const int authPages[]={CODE,PASSWORD,LOGIN};
  for(int p=0;p<3;p++)for(int i=0;i<3;i++){
    openFixture();provider=p;screen=authPages[i];selected=1;change(p==0?PLEX_SERVERS:LIBRARIES);authenticated=1;
    SDL_Event back={0};back.type=SDL_KEYDOWN;back.key.keysym.sym=SDLK_ESCAPE;srvsetup_evento(&back);
    for(int tick=0;tick<30;tick++)stepFixture(.025f);
    nav21_expect(names[authPages[i]],"Authenticated, Next, Back, 750ms: previous auth page",screen,authPages[i]);
    nav21_expect(names[authPages[i]],"Authenticated, Next, Back, 750ms: previous focus",selected,1);
    authenticated=0;
  }
  openFixture();screen=MANAGE;selected=3;
  SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_RETURN;srvsetup_evento(&e);
  nav21_expect("Servers/back","Manage, Down x3, OK: choose",screen,CHOOSE);
  e.key.keysym.sym=SDLK_ESCAPE;srvsetup_evento(&e);
  nav21_expect("Servers/back","Manage, Down x3, OK, Back: previous page",opened?screen:-1,MANAGE);
  nav21_expect("Servers/back","Manage, Down x3, OK, Back: previous focus",selected,3);
}
