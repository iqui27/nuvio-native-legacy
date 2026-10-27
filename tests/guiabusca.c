#include "../src/guia.c"
#ifndef GUIABUSCA_GUIDE
#define GUIABUSCA_GUIDE "../src/guiav5ui.c"
#endif
#include GUIABUSCA_GUIDE
#include "spotlight.h"
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <sys/stat.h>

void guiabusca_sports_fixture(void);

static void key(SDL_Keycode k){
  SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=k;
  if(spot_aberto())spot_evento(&e);else g5ui_event(&e);
  e.type=SDL_KEYUP;if(spot_aberto())spot_evento(&e);else g5ui_event(&e);
}
static void settle(void){for(int i=0;i<200;i++)spot_atualizar(1.f/60,SDL_GetTicks());}
static void shot(SDL_Window *w,const char *dir,const char *name){
  settle();char path[1024];snprintf(path,sizeof path,"%s/%s.png",dir,name);
  for(int i=0;i<40;i++){
    txt_novo_quadro();tex_novo_quadro();gfx_novo_quadro();glClear(GL_COLOR_BUFFER_BIT);
    g5ui_draw();spot_desenhar(SDL_GetTicks(),0);SDL_GL_SwapWindow(w);
  }
  unsigned char *p=malloc(1920*1080*4);assert(p);
  SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_RGBA32);assert(s);
  glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,p);
  for(int y=0;y<1080;y++)memcpy((char*)s->pixels+y*s->pitch,p+(1079-y)*1920*4,1920*4);
  assert(!IMG_SavePNG(s,path));SDL_FreeSurface(s);free(p);
}
static int row(const char *text){for(int i=0;i<spot_n_linhas();i++)if(!strcmp(spot_linha_texto(i),text))return i;return -1;}
static void pick(const char *text){
  int target=row(text);if(target<0)fprintf(stderr,"Missing result: %s\n",text);assert(target>=0);if(spot_teclado_app_aberto())key(SDLK_ESCAPE);key(SDLK_DOWN);
  for(int i=0;i<48&&spot_linha_focada()!=target;i++)key(SDLK_DOWN);
  assert(spot_linha_focada()==target);key(SDLK_RETURN);assert(!spot_aberto());
}
static void open_search(void){g5ui_open("grade");gv_snap_force=1;gv_snapshot();gv_zone=5;focus=1104;key(SDLK_RETURN);assert(spot_aberto());settle();}
int main(int argc,char **argv){
  const char *dir=argv[1];(void)argc;mkdir(dir,0755);
  assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));IMG_Init(IMG_INIT_PNG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_Window *w=SDL_CreateWindow("TV search",0,0,1920,1080,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);assert(w&&SDL_GL_CreateContext(w));
  glViewport(0,0,1920,1080);gfx_tamanho_alvo(1920,1080);assert(gfx_iniciar());assert(txt_iniciar("deploy/app",1));tex_iniciar(64);gfx_icones_dir("deploy/app/art");
  dados_iniciar(getenv("NUVIO_DADOS"));ajustes_iniciar();context();
  esportes_abrir_sessao();for(int i=0;i<esportes_modalidades_n();i++)assert(esportes_habilitar(i,0));
  nCanais=2700;nCats=2;canais=calloc(nCanais,sizeof *canais);cats=calloc(2,sizeof *cats);assert(canais&&cats);
  cats[0]=(GCat){"Esportes",0,0};cats[1]=(GCat){"Notícias",0,1};
  for(int i=0;i<nCanais;i++){
    char name[80],id[40];snprintf(name,sizeof name,"Channel %d",i);snprintf(id,sizeof id,"tv%d",i);
    canais[i].id=strdup(id);canais[i].nome=strdup(i==2699?"SporTV":name);canais[i].base="";canais[i].tipo="channel";canais[i].logo=canais[i].desc="";canais[i].cat=i%2;canais[i].numero=101+i;canais[i].epg=-1;
  }
  nLtvF=1;ltvF[0].tipo=LTV_M3U;ltvF[0].base="";strcpy(ltvF[0].chave,"fixture");
  estado=G_PRONTO;
  char xml[2000],a[32],b[32],c[32];time_t now=time(NULL);struct tm tm;
  gmtime_r(&now,&tm);strftime(a,sizeof a,"%Y%m%d%H%M%S +0000",&tm);
  time_t next=now+1800;gmtime_r(&next,&tm);strftime(b,sizeof b,"%Y%m%d%H%M%S +0000",&tm);
  time_t end=next+1800;gmtime_r(&end,&tm);strftime(c,sizeof c,"%Y%m%d%H%M%S +0000",&tm);
  snprintf(xml,sizeof xml,"<tv><channel id=\"sport\"><display-name>SporTV</display-name></channel><programme channel=\"sport\" start=\"%s\" stop=\"%s\"><title>Flamengo live</title></programme><programme channel=\"sport\" start=\"%s\" stop=\"%s\"><title>Palmeiras next</title></programme></tv>",a,b,b,c);assert(epg_xml_processar(xml)>0);
  inventory=calloc(2,sizeof *inventory);count=2;source_loaded=1;
  inventory[0].kind=G5_MOVIE;inventory[0].age=10;snprintf(inventory[0].name,sizeof inventory[0].name,"Flamengo movie");snprintf(inventory[0].id,sizeof inventory[0].id,"movie1");
  inventory[1]=inventory[0];inventory[1].kind=G5_SERIES;snprintf(inventory[1].name,sizeof inventory[1].name,"Flamengo series");snprintf(inventory[1].id,sizeof inventory[1].id,"series1");
  guiabusca_sports_fixture();
  open_search();shot(w,dir,"empty");key(SDLK_RETURN);assert(spot_teclado_app_aberto());key(SDLK_RETURN);assert(spot_consulta()[0]);shot(w,dir,"typing");
  key(SDLK_b);key(SDLK_c);assert(strstr(spot_consulta(),"bc"));
  spot_texto_externo("flamengo");settle();assert(row("Flamengo vs Palmeiras")>=0);assert(row("Flamengo live")>=0);assert(row("Flamengo movie")>=0);assert(row("Flamengo series")>=0);shot(w,dir,"results");
  spot_texto_externo("sportv");settle();assert(row("SporTV")>=0);pick("SporTV");assert(gv_focus()&&!strcmp(gv_focus()->id,"tv2699"));CatItem it;assert(guia_pediu_preview(&it)&&!strcmp(it.imdb,"tv2699"));
  open_search();spot_texto_externo("palmeiras");settle();pick("Palmeiras next");assert(gv_tfoc==next&&gv_focus()&&!strcmp(gv_focus()->id,"tv2699"));
  open_search();spot_texto_externo("noticias");settle();pick(i18n("Notícias"));assert(!strcmp(gv_grupo,"Notícias"));
  open_search();spot_texto_externo("mengao");settle();pick("Flamengo vs Palmeiras");assert(!strcmp(screen,"esportes-hoje"));assert(stream_folha_canais_aberta());stream_folha_canais_fechar();
  open_search();spot_texto_externo("2800");settle();assert(row("SporTV")>=0);
  Uint64 t=SDL_GetPerformanceCounter();
  for(int i=0;i<100;i++){spot_texto_externo(i%2?"sportv":"flamengo");spot_atualizar(1.f/60,SDL_GetTicks());}
  double query_ms=(SDL_GetPerformanceCounter()-t)*1000./SDL_GetPerformanceFrequency()/100;
  assert(query_ms<16.7);
  t=SDL_GetPerformanceCounter();for(int i=0;i<1000;i++)spot_atualizar(1.f/60,SDL_GetTicks());
  double ms=(SDL_GetPerformanceCounter()-t)*1000./SDL_GetPerformanceFrequency();printf("PASS: typing, 2700 channels, now/next, categories, VOD, sports/teams, tune; query %.3f ms, idle %.3f ms/frame\n",query_ms,ms/1000);
  spot_fechar();return 0;
}
