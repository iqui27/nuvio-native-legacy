#define SPORTS_V5_MAIN legacy_sports_main
#include "esportes_v5_shot.c"
#undef SPORTS_V5_MAIN

static void polish_capture(SDL_Window *w,const char *out,const char *phase,const char *name){
  char file[100];snprintf(file,sizeof file,"%s-%s",phase,name);capture(w,out,file);
}
static void polish_perf(SDL_Window *w,const char *phase,const char *name){
  double update=0,draw=0,worst=0;int rasters=0,uploads=0,rects=0;
  Uint64 hz=SDL_GetPerformanceFrequency();
  for(int i=0;i<150;i++){
    txt_novo_quadro();tex_novo_quadro();gfx_novo_quadro();tex_bombear(8);txt_rasterizadas=tex_upl_n=0;
    Uint64 a=SDL_GetPerformanceCounter();gv_tick();Uint64 b=SDL_GetPerformanceCounter();
    g5ui_draw();glFinish();double ms=(SDL_GetPerformanceCounter()-b)*1000.0/hz;
    if(i>=30){update+=(b-a)*1000.0/hz;draw+=ms;worst=fmax(worst,ms);rasters+=txt_rasterizadas;uploads+=tex_upl_n;rects+=gfx_n_rect;}
    SDL_GL_SwapWindow(w);
  }
  printf("[espolir] %s %s events=%d frames=120 update=%.3fms draw=%.3fms worst=%.3fms rects=%.1f text_uploads=%d art_uploads=%d\n",phase,name,gs_n,update/120,draw/120,worst,rects/120.0,rasters,uploads);
  if(!strcmp(phase,"after")){assert(draw/120<16);assert(!rasters);}
}
int main(int argc,char **argv){
  if(getenv("ESPOLIR_LEGACY"))return legacy_sports_main(argc,argv);
  const char *out=argc>1?argv[1]:"/Volumes/ExternalSSD/tmp/rev/espolir",*phase=argc>2?argv[2]:"after";
  assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_Window *w=SDL_CreateWindow("Sports polish proof",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1920,1080,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);assert(w&&SDL_GL_CreateContext(w));SDL_GL_SetSwapInterval(0);
  glViewport(0,0,1920,1080);gfx_tamanho_alvo(1920,1080);assert(gfx_iniciar());assert(txt_iniciar("deploy/app",1));tex_iniciar(64);gfx_icones_dir("deploy/app/art");
  dados_iniciar(getenv("NUVIO_DADOS"));dados_gravar("ajustes.txt","idioma 0\nidiomaAutoLocal 0\nselected_theme 2\nescalaTextoLocal 2\nescalaTextoVersaoLocal 1\n");ajustes_dir(getenv("NUVIO_DADOS"));ajustes_definir_vidro(0);context();semear();fixture();
  for(int i=0;i<9;i++)strcpy(catalogo[i].liga.logo,"tests/fixtures/guiacat-team-0.png");
  for(int i=0;i<8;i++){
    rowCache[i].linha=i;rowCache[i].n=50;rowCache[i].estado=1;
    rowCache[i].eventos=realloc(rowCache[i].eventos,50*sizeof *eventos);assert(rowCache[i].eventos);
    for(int j=0;j<50;j++){
      EspEvento *e=&rowCache[i].eventos[j];*e=eventos[j%8];
      snprintf(e->id,sizeof e->id,"stress-%d-%d",i,j);
      snprintf(e->ligaId,sizeof e->ligaId,"league%d",i);
      if(j%2){strcpy(e->participantes[0].nome,"Manchester United");strcpy(e->participantes[1].nome,"Wolverhampton Wanderers");}
    }
  }
  esp_fixture_revision++;
  const char *pages[]={"esportes-ligas","esportes-hoje","esportes-proximos","esportes-resultados"};
  const char *names[]={"home","live-focused","upcoming","results"};
  for(int i=0;i<4;i++){
    g5ui_open(pages[i]);quadro();focus=i?2200:2071;
    polish_capture(w,out,phase,names[i]);polish_perf(w,phase,names[i]);
  }
  gs_event_open(&eventos[0]);quadro();polish_capture(w,out,phase,"event-live");polish_perf(w,phase,"event-live");
  gs_back();gs_event_open(&eventos[2]);quadro();polish_capture(w,out,phase,"event-upcoming");
  gs_back();g5ui_open("esportes-liga");gs_league=0;gs_sport=0;
  int count=nEventos;nEventos=0;esp_fixture_revision++;quadro();focus=2020;
  polish_capture(w,out,phase,"empty");assert(gs_n==0);
  esp_fixture_busy=1;polish_capture(w,out,phase,"loading");assert(gs_n==0);esp_fixture_busy=0;
  esp_fixture_error=1;polish_capture(w,out,phase,"error");
#ifdef ESPOLIR_IMPLEMENTED
  int retry=0;for(int i=0;i<num_focus;i++)if(next_focus[i]==2034)retry=1;assert(retry);
#endif
  esp_fixture_error=0;
  nEventos=count;esp_fixture_revision++;
#ifdef ESPOLIR_IMPLEMENTED
  /* Production sort: live first, followed teams before other peers, newest results first. */
  EspEvento a=eventos[2],b=a;a.estado=1;assert(gs_order(&a,&b)<0);
  a=b;a.participantes[0].id[0]=0;a.participantes[1].id[0]=0;assert(gs_order(&b,&a)<0);
  a=b;a.estado=b.estado=2;a.inicio++;assert(gs_order(&a,&b)<0);
  g5ui_open("esportes-ligas");quadro();focus=3000;gv_event(SDLK_DOWN,0);assert(focus>=3400&&focus<3800);
  quadro();gv_event(SDLK_RIGHT,0);quadro();assert(focus==3401);
  gs_activate();assert(!strcmp(screen,"esportes-evento"));gs_back();quadro();assert(focus==3401);
  const int shelfFocus[]={3000,3400,3800,4200,4600,5000};
  for(int i=0;i<6;i++){focus=shelfFocus[i];quadro();char name[40];snprintf(name,sizeof name,"home-shelf-%d",i);polish_capture(w,out,phase,name);}
  ajustes_definir_vidro(1);focus=3000;polish_capture(w,out,phase,"home-glass");ajustes_definir_vidro(0);
  g5ui_open("esportes-liga");quadro();focus=2200;
  gv_event(SDLK_DOWN,0);assert(focus==2203);quadro();gv_event(SDLK_DOWN,0);assert(gs_page==1&&focus==2203);
  gv_event(SDLK_UP,0);assert(focus==2200);
  gv_event(SDLK_LEFT,0);assert(gv_side_open);gv_event(SDLK_RIGHT,0);assert(!gv_side_open);
  gv_event(SDLK_DOWN,0);quadro();int selected=gs_page*3+focus-2200;
  esp_fixture_revision++;gs_collect();assert(gs_page*3+focus-2200==selected);
  gs_page=0;focus=2200;quadro();
  gv_event(SDLK_RIGHT,0);int position=focus;for(int i=0;i<20;i++)gv_event(SDLK_RIGHT,1);assert(focus==position);
  gs_event_open(&eventos[0]);quadro();assert(gs_watch_n);
  for(int i=1;i<8;i++){gs_watch[i]=gs_watch[0];snprintf(gs_watch[i].name,sizeof gs_watch[i].name,"Sports %d",i+1);}gs_watch_n=8;
  focus=2080;polish_capture(w,out,phase,"event-channels");gv_event(SDLK_RIGHT,0);assert(focus==2081);
  gv_event(SDLK_DOWN,0);assert(focus==2085);gv_event(SDLK_DOWN,0);assert(focus==2089);
  gs_back();
  puts("PASS Sports polish: ordering, shelves, directional scroll, event/Back focus, refresh selection, paced repeat, channel tiles, empty/loading/error fixtures");
#endif
  if(!strcmp(phase,"after")){setenv("ESPEVENTO_PROOF","after",1);event_proof(w,out);}
  return 0;
}
