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
static int pageFixture(void){return aberto?pagina:-1;}
void nav21_aio(void){
  data.valida=1;data.configurada=1;snprintf(data.nome,sizeof data.nome,"Offline fixture");
  const char *names[]={"AIO/start","AIO/instance","AIO/import","AIO/services","AIO/ready","AIO/overview","AIO/catalogs","AIO/sources","AIO/links","AIO/subtitles","AIO/filters","AIO/health","AIO/title"};
  for(fixture=0;fixture<=TITULO;fixture++){Nav21Screen s={names[fixture],openFixture,aioui_evento,stepFixture,drawFixture,focusFixture,selectFixture,pageFixture,fixture==TITULO?2:0};nav21_check(s);}
  openFixture();pagina=SERVICOS;foco=2;lateral=0;mudar(FILTROS);
  SDL_Event back={0};back.type=SDL_KEYDOWN;back.key.keysym.sym=SDLK_ESCAPE;aioui_evento(&back);
  nav21_expect("AIO/back","Services, focus 2, Filters, Back: previous page",pagina,SERVICOS);
  nav21_expect("AIO/back","Services, focus 2, Filters, Back: previous focus",foco,2);
}
