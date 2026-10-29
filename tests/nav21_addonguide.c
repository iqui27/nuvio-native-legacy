#include "nav21.h"
#include "../src/addonguia.c"
static int fixture;
static void openFixture(void){addonguia_shot(fixture);}
static void stepFixture(float dt){addonguia_shot_passo(dt);}
static void drawFixture(void){addonguia_desenhar();for(int i=0;i<nAlvos;i++)nav21_target(alvos[i].id,alvos[i].r);}
static int focusFixture(void){return foco;}
static void selectFixture(int i){foco=i;}
static float transitionFixture(void){return abertura*paginaAlpha;}
static int pageFixture(void){return saindo?-1:tela;}
static void internalNext(void){conectar();}
static void internalBack(void){SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_ESCAPE;addonguia_evento(&e);}
void nav21_addonguide(void){
  const char *names[]={"Addon guide/torrent","Addon guide/catalog","Addon guide/direct","Addon guide/mode","Addon guide/connect","Addon guide/starting"};
  for(fixture=AG_NOVO_TORRENT;fixture<AG_LISTA;fixture++){openFixture();drawFixture();int primary=foco;Nav21Screen s={names[fixture-AG_NOVO_TORRENT],openFixture,addonguia_evento,stepFixture,drawFixture,focusFixture,selectFixture,pageFixture,primary,transitionFixture};nav21_check(s);}
  fixture=AG_MODO;openFixture();foco=1;for(int i=0;i<90;i++)stepFixture(1.f/60);Nav21Screen s={"Addon guide/internal",openFixture,addonguia_evento,stepFixture,drawFixture,focusFixture,selectFixture,pageFixture,1,transitionFixture};nav21_motion(s,internalNext,"Mode, Connect: transition >=150ms");nav21_motion(s,internalBack,"Connect, Back: transition >=150ms");nav21_expect("Addon guide/back","Mode, focus 1, Connect, Back: previous page",tela,AG_MODO);nav21_expect("Addon guide/back","Mode, focus 1, Connect, Back: previous focus",foco,1);
}
