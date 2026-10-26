#include "nav21.h"
#include "../src/aiostreamsui.c"
#undef ponteiro_alvo
static int fixture;
static AioSnapshot data;
static void openFixture(void){aioui_shot(fixture,&data);}
static void stepFixture(float dt){movimento(dt);if(saindo&&abertura<=0)encerrar();}
static void drawFixture(void){aioui_desenhar(SDL_GetTicks());for(int i=0;i<navigationCount;i++)nav21_target(navigationIds[i],navigationRects[i]);}
static int focusFixture(void){return foco+lateral*1000;}
static void selectFixture(int id){pointer(id%1000,id/1000);}
static float transitionFixture(void){return abertura*corpo;}
static int pageFixture(void){return aberto?pagina:-1;}
static int motionPage;
static void internalNext(void){mudar(motionPage);}
static void internalBack(void){SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_ESCAPE;aioui_evento(&e);}
void nav21_aio(void){
  data.valida=1;data.configurada=1;snprintf(data.nome,sizeof data.nome,"Offline fixture");
  const char *names[]={"AIO/start","AIO/instance","AIO/import","AIO/services","AIO/ready","AIO/overview","AIO/catalogs","AIO/sources","AIO/links","AIO/subtitles","AIO/filters","AIO/health","AIO/title"};
  for(fixture=0;fixture<=TITULO;fixture++){Nav21Screen s={names[fixture],openFixture,aioui_evento,stepFixture,drawFixture,focusFixture,selectFixture,pageFixture,fixture==TITULO?2:0,transitionFixture};nav21_check(s);}
  for(motionPage=SEM+1;motionPage<=TITULO;motionPage++){
    fixture=SEM;openFixture();for(int i=0;i<90;i++)stepFixture(1.f/60);
    Nav21Screen s={names[motionPage],openFixture,aioui_evento,stepFixture,drawFixture,focusFixture,selectFixture,pageFixture,0,transitionFixture};
    nav21_motion(s,internalNext,"Navigate: internal page transition >=150ms");
    nav21_motion(s,internalBack,"Back: internal page transition >=150ms");
    nav21_expect(names[motionPage],"Navigate, Back: previous route",pagina,SEM);
    nav21_expect(names[motionPage],"Navigate, Back: previous focus",foco,0);
  }
  openFixture();pagina=SERVICOS;foco=2;lateral=0;mudar(FILTROS);
  SDL_Event back={0};back.type=SDL_KEYDOWN;back.key.keysym.sym=SDLK_ESCAPE;aioui_evento(&back);
  nav21_expect("AIO/back","Services, focus 2, Filters, Back: previous page",pagina,SERVICOS);
  nav21_expect("AIO/back","Services, focus 2, Filters, Back: previous focus",foco,2);
}
