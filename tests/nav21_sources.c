#include "nav21.h"
#include "../src/streams.c"
static int clockMode;
static Uint32 clockFixture;
void nav21_island_draw(Uint32 now);
static void openFixture(void){
  clockFixture=SDL_GetTicks()+2000;anim=0;stream_folha_nome("Offline title");
  if(clockMode){ilha_abrir_sessao();ilha_coberta(0);ilha_relogio_visivel(1);ilha_posicionar(1);for(int i=0;i<90;i++)nav21_island_draw(clockFixture+=16);}
  stream_folha_abrir();if(!clockMode)folhaRelogio=0;
}
static void stepFixture(float dt){clockFixture+=(Uint32)(dt*1000);stream_folha_atualizar(dt,clockFixture);}
static void drawFixture(void){nav21_targets_reset();stream_folha_desenhar(clockFixture);if(clockMode)nav21_island_draw(clockFixture);for(int i=0;i<navigationCount;i++)nav21_target((navigation[i].group+2)*10000+navigation[i].id+100,navigation[i].rect);}
static int focusFixture(void){return (grupo+2)*10000+(grupo==0?filtro:foco)+100;}
static void selectFixture(int id){grupo=id/10000-2;if(grupo==0)filtro=id%10000-100;else foco=id%10000-100;}
static float transitionFixture(void){return clockMode?ilha_corpo_t():anim;}
static int pageFixture(void){return aberta;}
static void providerRegression(void){
  Stream v[32]={0};for(int i=0;i<32;i++){snprintf(v[i].url,sizeof v[i].url,"file:///provider-%d.mp4",i);snprintf(v[i].rotulo,sizeof v[i].rotulo,"Source %d",i);snprintf(v[i].provedor,sizeof v[i].provedor,"%s",i==31?"Short":"Long");v[i].altura=1080;}
  stream_definir_lista(v,32);openFixture();stepFixture(1);drawFixture();
  for(int direction=0;direction<2;direction++){
    grupo=0;filtro=direction?2:1;rolagem=900;velRol=80;filtroServidor=0;drawFixture();
    SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=direction?SDLK_LEFT:SDLK_RIGHT;stream_folha_evento(&e);
    nav21_expect("Sources/provider","Long list, tabs, Left/Right: provider",filtro,direction?1:2);
    nav21_expect("Sources/provider","Long list, tabs, Left/Right: server filter",filtroServidor,-1);
    nav21_expect("Sources/provider","Long list, tabs, Left/Right: scroll",(int)rolagem,0);
    nav21_expect("Sources/provider","Long list, tabs, Left/Right: velocity",(int)velRol,0);
  }
}
static float detailTransition(void){return detAnim;}
static void internalNext(void){detalheAbrir(&lista[0]);}
static void internalBack(void){SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_ESCAPE;stream_folha_evento(&e);}
void nav21_sources(void){providerRegression();Stream v[4]={0};for(int i=0;i<4;i++){snprintf(v[i].url,sizeof v[i].url,"file:///fixture-%d.mp4",i);snprintf(v[i].rotulo,sizeof v[i].rotulo,"Fixture %d",i);snprintf(v[i].provedor,sizeof v[i].provedor,"Offline");v[i].altura=1080;}stream_definir_lista(v,4);openFixture();int primary=focusFixture();Nav21Screen s={"Sources sheet",openFixture,stream_folha_evento,stepFixture,drawFixture,focusFixture,selectFixture,pageFixture,primary,transitionFixture};nav21_check(s);openFixture();for(int i=0;i<90;i++)stepFixture(1.f/60);s.transition=detailTransition;nav21_motion(s,internalNext,"Open source detail: transition >=150ms");nav21_motion(s,internalBack,"Back source detail: transition >=150ms");nav21_expect(s.name,"Source detail, Back: sheet remains open",aberta,1);nav21_expect(s.name,"Source detail, Back: previous focus",focusFixture(),primary);
  clockMode=1;s.name="Sources/clock island";s.transition=transitionFixture;nav21_check(s);
  openFixture();nav21_expect(s.name,"Open from visible clock: folhaRelogio",folhaRelogio,1);
  for(int i=0;i<90;i++){stepFixture(1.f/60);drawFixture();}
  int previous=focusFixture();s.transition=detailTransition;
  nav21_motion(s,internalNext,"Open source detail from clock: transition >=150ms");
  nav21_motion(s,internalBack,"Back source detail to clock: transition >=150ms");
  nav21_expect(s.name,"Source detail, Back: sheet remains open",aberta,1);
  nav21_expect(s.name,"Source detail, Back: previous focus",focusFixture(),previous);
  internalBack();for(int i=0;i<90;i++){stepFixture(1.f/60);drawFixture();}
  clockMode=0;
}
