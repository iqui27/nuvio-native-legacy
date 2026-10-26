#include "nav21.h"
#include "../src/addonguia.c"
static int fixture;
static void openFixture(void){addonguia_shot(fixture);}
static void stepFixture(float dt){addonguia_shot_passo(dt);}
static void drawFixture(void){addonguia_desenhar();for(int i=0;i<nAlvos;i++)nav21_target(alvos[i].id,alvos[i].r);}
static int focusFixture(void){return foco;}
static void selectFixture(int i){foco=i;}
static int pageFixture(void){return saindo?-1:tela;}
void nav21_addonguide(void){
  const char *names[]={"Addon guide/torrent","Addon guide/catalog","Addon guide/direct","Addon guide/mode","Addon guide/connect","Addon guide/starting"};
  for(fixture=AG_NOVO_TORRENT;fixture<AG_LISTA;fixture++){openFixture();drawFixture();int primary=foco;Nav21Screen s={names[fixture-AG_NOVO_TORRENT],openFixture,addonguia_evento,stepFixture,drawFixture,focusFixture,selectFixture,pageFixture,primary};nav21_check(s);}
  fixture=AG_MODO;openFixture();foco=1;conectar();SDL_Event back={0};back.type=SDL_KEYDOWN;back.key.keysym.sym=SDLK_ESCAPE;addonguia_evento(&back);nav21_expect("Addon guide/back","Mode, focus 1, Connect, Back: previous page",tela,AG_MODO);nav21_expect("Addon guide/back","Mode, focus 1, Connect, Back: previous focus",foco,1);
}
