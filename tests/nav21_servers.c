#include "nav21.h"
#include "../src/srvsetup.c"
#include <string.h>
static int fixture;
static Uint32 clockFixture;
static void openFixture(void) {
  opened=1;closing=0;historyCount=0;transitionAt=clockFixture=SDL_GetTicks();screen=fixture;provider=1;selected=0;profile=perfis_ativo();
  worker.n=2;worker.alive=0;nLibraries=3;rowChoice=0;recoveryAuth=0;codeSince=SDL_GetTicks();
  for(int i=0;i<3;i++) {snprintf(libraries[i].titulo,sizeof libraries[i].titulo,"Library %d",i);snprintf(libraries[i].chave,sizeof libraries[i].chave,"jellyfin_fixture%d",i);}
}
static int focusFixture(void) {return selected;}
static void selectFixture(int i) {selected=i;}
static int pageFixture(void) {return opened?screen:-1;}
static void stepFixture(float dt) {clockFixture+=(Uint32)(dt*1000);srvsetup_atualizar(clockFixture);}
static void keyFixture(const SDL_Event *e) {clockFixture=SDL_GetTicks();srvsetup_evento(e);}
static void drawFixture(void) {srvsetup_desenhar(clockFixture);for(int i=0;i<navigationCount;i++)nav21_target(navigationIds[i],navigationRects[i]);}
void nav21_servers(void) {
  const char *names[]={"Servers/choose","Servers/discover","Servers/address","Servers/login","Servers/users","Servers/password","Servers/code","Servers/connect-code","Servers/connect-password","Servers/connect-servers","Servers/plex-servers","Servers/libraries","Servers/ready","Servers/manage","Servers/remove","Servers/error","Servers/rows"};
  for(fixture=0;fixture<=ROWS;fixture++) {
    Nav21Screen s={names[fixture],openFixture,keyFixture,stepFixture,drawFixture,focusFixture,selectFixture,pageFixture,0};
    nav21_check(s);
  }
  for(int previous=CHOOSE;previous<=ROWS;previous++){
    openFixture();screen=previous;selected=1;change((previous+1)%(ROWS+1));
    SDL_Event back={0};back.type=SDL_KEYDOWN;back.key.keysym.sym=SDLK_ESCAPE;srvsetup_evento(&back);
    nav21_expect(names[previous],"Open subpage, Back: previous page",screen,previous);
    nav21_expect(names[previous],"Open subpage, Back: previous focus",selected,1);
  }
  openFixture();screen=MANAGE;selected=3;
  SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_RETURN;srvsetup_evento(&e);
  nav21_expect("Servers/back","Manage, Down x3, OK: choose",screen,CHOOSE);
  e.key.keysym.sym=SDLK_ESCAPE;srvsetup_evento(&e);
  nav21_expect("Servers/back","Manage, Down x3, OK, Back: previous page",opened?screen:-1,MANAGE);
  nav21_expect("Servers/back","Manage, Down x3, OK, Back: previous focus",selected,3);
}
