#include "nav21.h"
#include "../src/juntosui.c"
void nav21_juntos_data(void);
static Uint32 clockFixture;
static void openFixture(void){nav21_juntos_data();camada=camadaVisual=CAMADA_SALA;entrada=tempoEntrada=0;foco=0;clockFixture=SDL_GetTicks();ultimoQuadro=clockFixture;}
static void stepFixture(float dt){clockFixture+=(Uint32)(dt*1000);juntosui_atualizar(clockFixture);}
static void keyFixture(const SDL_Event *e){juntosui_evento_sdl(e);}
static void drawFixture(void){juntosui_desenhar(clockFixture);ilha_desenhar(clockFixture);for(int i=0;i<navigationCount;i++)nav21_target(navigationIds[i]+10,navigationRects[i]);}
static int focusFixture(void){return foco+10;}
static void selectFixture(int id){foco=id-10;}
static float transitionFixture(void){return camadaVisual==CAMADA_FONTES?ilha_corpo_t():entrada;}
static int pageFixture(void){return camada;}
static void internalNext(void){apertarSala(BT_FONTES);}
static void internalBack(void){SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_ESCAPE;juntosui_evento_sdl(&e);}
void nav21_juntos(void){Nav21Screen s={"Juntos/lobby",openFixture,keyFixture,stepFixture,drawFixture,focusFixture,selectFixture,pageFixture,10,transitionFixture};nav21_check(s);openFixture();for(int i=0;i<90;i++)stepFixture(1.f/60);foco=2;nav21_motion(s,internalNext,"Lobby, Sources: transition >=150ms");nav21_motion(s,internalBack,"Sources, Back: transition >=150ms");nav21_expect("Juntos/back","Lobby, focus 2, Sources, Back: previous page",camada,CAMADA_SALA);nav21_expect("Juntos/back","Lobby, focus 2, Sources, Back: previous focus",foco,2);}
