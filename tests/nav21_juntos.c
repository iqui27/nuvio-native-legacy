#include "nav21.h"
#include "../src/juntosui.c"
void nav21_juntos_data(void);
static Uint32 clockFixture;
static void openFixture(void){nav21_juntos_data();camada=camadaVisual=CAMADA_SALA;entrada=tempoEntrada=0;foco=0;clockFixture=SDL_GetTicks();ultimoQuadro=clockFixture;}
static void stepFixture(float dt){clockFixture+=(Uint32)(dt*1000);juntosui_atualizar(clockFixture);stream_folha_atualizar(dt,clockFixture);}
static void keyFixture(const SDL_Event *e){juntosui_evento_sdl(e);}
static void drawFixture(void){juntosui_desenhar(clockFixture);stream_folha_desenhar(clockFixture);ilha_desenhar(clockFixture);for(int i=0;i<navigationCount;i++)nav21_target(navigationIds[i]+10,navigationRects[i]);}
static int focusFixture(void){return foco+10;}
static int ownsArrow(SDL_Keycode k){int buttons[BT_N];return k==SDLK_DOWN&&(foco<0||foco==botoesSala(buttons)-1);}
static void selectFixture(int id){foco=id-10;}
static float transitionFixture(void){return camadaVisual==CAMADA_FONTES?ilha_corpo_t():entrada;}
static int pageFixture(void){return camada;}
static float sourceTransition(void){return stream_folha_anim();}
static void internalNext(void){apertarSala(BT_FONTES);nav21_expect("Juntos/sources","Lobby requests shared Sources sheet",juntosui_pediu_fontes(),1);stream_folha_abrir();}
static void internalBack(void){SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_ESCAPE;stream_folha_evento(&e);}
void nav21_juntos(void){Nav21Screen s={"Juntos/lobby",openFixture,keyFixture,stepFixture,drawFixture,focusFixture,selectFixture,pageFixture,10,transitionFixture,ownsArrow};nav21_check(s);openFixture();for(int i=0;i<90;i++)stepFixture(1.f/60);foco=2;s.transition=sourceTransition;nav21_motion(s,internalNext,"Lobby, Sources: transition >=150ms");nav21_motion(s,internalBack,"Sources, Back: transition >=150ms");nav21_expect("Juntos/back","Lobby, focus 2, Sources, Back: previous page",camada,CAMADA_SALA);nav21_expect("Juntos/back","Lobby, focus 2, Sources, Back: previous focus",foco,2);}
