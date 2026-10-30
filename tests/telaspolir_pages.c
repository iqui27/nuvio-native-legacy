#include "nav21.h"
#include "ponteiro.h"
#include "login.h"
#include "busca.h"
#include "agendaui.h"
#include "agenda.h"
#include "catalogo.h"
#include "diagnostico.h"
#include "explorar.h"
#include "salvospainel.h"
#include "amigoperfil.h"
#include "socialvis.h"
#include "faixas.h"
#include "vertudo.h"
#include "colecoes.h"
#include "descanso.h"
#include "plrilha.h"
#include "tex_cache.h"
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void key(void (*event)(const SDL_Event *),SDL_Keycode code){
  SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=code;event(&e);e.type=SDL_KEYUP;event(&e);
}
static void capture(const char *name,void (*step)(float,Uint32),void (*draw)(Uint32)){
  for(int i=0;i<100;i++){
    if(step)step(.016f,SDL_GetTicks());txt_novo_quadro();tex_novo_quadro();tex_bombear(12);gfx_novo_quadro();
    glClearColor(.05f,.05f,.05f,1);glClear(GL_COLOR_BUFFER_BIT);draw(SDL_GetTicks());
    if(!strncmp(name,"Player-",7))plrilha_desenhar(SDL_GetTicks());SDL_Delay(16);
  }
  unsigned char *pixels=malloc(1920*1080*4);assert(pixels);
  glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_RGBA32);assert(surface);
  for(int y=0;y<1080;y++)memcpy((char*)surface->pixels+y*surface->pitch,pixels+(1079-y)*1920*4,1920*4);
  char path[1024];snprintf(path,sizeof path,"%s/%s.png",getenv("NAV21_OUT"),name);
  assert(!IMG_SavePNG(surface,path));SDL_FreeSurface(surface);free(pixels);
}
extern void telaspolir_login(void),telaspolir_login_nav(void);
void telaspolir_pages(void){
  telaspolir_login_nav();
  login_recursos("deploy/app/art");login_iniciar();capture("Login-QR",login_atualizar,login_desenhar);
  telaspolir_login();capture("Login-email",login_atualizar,login_desenhar);
  explorar_iniciar();capture("Explore",explorar_atualizar,explorar_desenhar);explorar_encerrar();
  diagnostico_iniciar();diagnostico_intro_dispensar(0);capture("Diagnostics",diagnostico_atualizar,diagnostico_desenhar);
  diagnostico_abrir_velocidade();capture("Diagnostics-speed",diagnostico_atualizar,diagnostico_desenhar);
  diagnostico_encerrar();
  static CatItem items[3];
  for(int i=0;i<3;i++){
    snprintf(items[i].imdb,sizeof items[i].imdb,"tt%07d",100+i);strcpy(items[i].tipo,"series");items[i].naLista=1;
    snprintf(items[i].titulo,sizeof items[i].titulo,"%s",i==0?"Foundation":i==1?"Severance":"Andor");
    snprintf(items[i].poster,sizeof items[i].poster,"deploy/app/art/poster/%02d.jpg",i);
  }
  cat_definir_tudo(items,3,NULL,0);
  static ColFolder collection;static ColSource source;
  strcpy(collection.id,"offline");strcpy(collection.title,"Science fiction");collection.sources=&source;collection.nSources=1;
  strcpy(source.title,"Films");strcpy(source.base,"http://127.0.0.1:8799");strcpy(source.type,"movie");strcpy(source.catId,"offline");
  vertudo_colecao(&collection);capture("Collections-empty",vertudo_atualizar,vertudo_desenhar);vertudo_fechar_seco();
  descanso_quadro(SDL_GetTicks(),1,1,0,0);capture("Screensaver",NULL,descanso_desenhar);
  agenda_iniciar();agenda_definir_hoje("2026-10-29");
  for(int i=0;i<3;i++)agenda_registrar(items[i].imdb,items[i].titulo,items[i].poster,"Returning Series",2,i+1,"New episode",i==0?"2026-10-29":"2026-10-31","2026-10-22");
  agendaui_iniciar();capture("Calendar-list",agendaui_atualizar,agendaui_desenhar);
  key(agendaui_evento,SDLK_UP);key(agendaui_evento,SDLK_RIGHT);key(agendaui_evento,SDLK_RETURN);
  capture("Calendar-month",agendaui_atualizar,agendaui_desenhar);
  for(int tab=2;tab<=4;tab++){
    spainel_abrir();spainel_ir_aba(tab);capture(tab==2?"Friends":tab==3?"Notifications":"Social-agenda",spainel_atualizar,spainel_desenhar);spainel_fechar();
    for(int i=0;i<100;i++)spainel_atualizar(.016f,SDL_GetTicks());
  }
  amigoperfil_abrir("nuvio:offline-friend");capture("Friends-profile-empty",amigoperfil_atualizar,amigoperfil_desenhar);
  faixas_abrir();capture("Player-audio",faixas_atualizar,faixas_desenhar);
  faixas_abrir_em(1);capture("Player-subtitles",faixas_atualizar,faixas_desenhar);
  faixas_aba_estilo(1);capture("Player-subtitle-style",faixas_atualizar,faixas_desenhar);
}
