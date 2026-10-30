#include "nav21.h"
#include <SDL2/SDL_image.h>
#include "../src/aiostreamsui.c"
#undef ponteiro_alvo
static int fixture;
static void addonNavigation(void){
  AddonRemoto list[2]={0};
  for(int i=0;i<2;i++){snprintf(list[i].nome,sizeof list[i].nome,"AIOStreams %d",i+1);snprintf(list[i].url,sizeof list[i].url,"http://127.0.0.1:8799/aio%d/manifest.json",i);list[i].ativo=1;}
  addons_definir_lista(list,2);
  const SDL_Keycode keys[]={SDLK_LEFT,SDLK_RIGHT,SDLK_UP,SDLK_DOWN};
  for(int from=0;from<5;from++)for(int d=0;d<4;d++){
    addonsui_abrir();addonsui_ponteiro(from,0);navigationCount=0;aioui_addons_desenhar(from);
    int current=-1;for(int i=0;i<navigationCount;i++)if(navigationIds[i]==from)current=i;
    nav21_expect("Addons/manager","Focused action has rendered target",current>=0,1);
    if(current<0)continue;
    int next=focus_rect_nearest(navigationRects,navigationCount,current,(keys[d]==SDLK_RIGHT)-(keys[d]==SDLK_LEFT),(keys[d]==SDLK_DOWN)-(keys[d]==SDLK_UP));
    int expected=navigationIds[next];
    if(expected==from)continue;
    SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=keys[d];addonsui_evento(&e);
    char path[100];snprintf(path,sizeof path,"From %d, %s: visually adjacent action",from,SDL_GetKeyName(keys[d]));
    nav21_expect("Addons/manager",path,addonsui_foco(),expected);
  }
  addonsui_abrir();
  aioui_addons_desenhar(0);
  unsigned char *pixels=malloc(1920*1080*4);SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_RGBA32);
  glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  for(int y=0;y<1080;y++)memcpy((char*)surface->pixels+y*surface->pitch,pixels+(1079-y)*1920*4,1920*4);
  char path[1024];snprintf(path,sizeof path,"%s/Addons-manager.png",getenv("NAV21_OUT"));IMG_SavePNG(surface,path);SDL_FreeSurface(surface);free(pixels);
}

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
static void pagingRegression(void){
  data.resultados=12;data.evidencias=9;data.catalogos=6;
  for(int i=0;i<12;i++){snprintf(data.resultado[i].nome,sizeof data.resultado[i].nome,"Result %d",i);strcpy(data.resultado[i].tipo,"HTTP");}
  for(int i=0;i<6;i++){snprintf(data.catalogo[i].nome,sizeof data.catalogo[i].nome,"Catalog %d",i);strcpy(data.catalogo[i].tipo,"movie");}
  for(int mode=0;mode<3;mode++){
    fixture=mode==2?CATALOGOS:TITULO;openFixture();evidenciaSelecionada=mode==1;
    int first=mode==2?0:2,n=mode==0?12:mode==1?9:6;
    foco=first;lateral=0;
    for(int direction=1;direction>=-1;direction-=2)for(int i=0;i<n;i++){
      int expected=first+(direction==1?i:n-1-i);
      if(i||direction==-1){SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=direction==1?SDLK_DOWN:SDLK_UP;if(!(direction==-1&&i==0))aioui_evento(&e);}
      for(int tick=0;tick<90;tick++)stepFixture(1.f/60);nav21_targets_reset();drawFixture();
      char path[120];snprintf(path,sizeof path,"%s to row %d: adjacent row without footer detour",direction==1?"Down":"Up",expected-first+1);
      const char *name=mode==0?"AIO/results paging":mode==1?"AIO/evidence paging":"AIO/catalog paging";
      nav21_expect(name,path,foco,expected);
      int rendered=0;for(int j=0;j<navigationCount;j++)if(navigationIds[j]==expected)rendered=1;
      nav21_expect(name,"Focused row is rendered after scrolling",rendered,1);
    }
  }
  data.resultados=data.evidencias=data.catalogos=0;
}
void nav21_aio(void){
  addonNavigation();
  data.valida=1;data.configurada=1;snprintf(data.nome,sizeof data.nome,"Offline fixture");
  pagingRegression();
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
