/* Native six-category proof and D-pad regression; no network or provider secrets. */
#ifndef GUIAV5_UNDER_TEST
#define GUIAV5_UNDER_TEST "../src/guiav5ui.c"
#endif
#include "xtream.h"
#include <assert.h>
#include <pthread.h>
#include <string.h>
static pthread_mutex_t metadata_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t metadata_cond=PTHREAD_COND_INITIALIZER;
static int metadata_entered,metadata_release;
static char *slow_metadata(const char *server,const char *user,const char *password,const char *action,int seconds,int *status){
  (void)server;(void)user;(void)password;(void)action;(void)seconds;
  pthread_mutex_lock(&metadata_lock);metadata_entered=1;pthread_cond_broadcast(&metadata_cond);
  while(!metadata_release)pthread_cond_wait(&metadata_cond,&metadata_lock);
  assert(!strcmp(action,"get_vod_info&vod_id=3"));
  pthread_mutex_unlock(&metadata_lock);*status=200;
  return strdup("{\"info\":{\"plot\":\"Background result\"}}");
}
#define xtream_consultar slow_metadata
#define xtream_consultar_metadata slow_metadata
#include GUIAV5_UNDER_TEST
#undef xtream_consultar
#undef xtream_consultar_metadata
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <sys/stat.h>

static void key(SDL_Keycode code){SDL_Event e={.type=SDL_KEYDOWN};e.key.keysym.sym=code;assert(g5ui_event(&e));}
#ifndef VOD_BASELINE
static float poster_focus(int id){for(int i=0;i<64;i++)if(vod_focus_ids[i]==id)return vod_focus[i];return 0;}
#endif
static void frame(SDL_Window *window){
  SDL_PumpEvents();txt_novo_quadro();tex_novo_quadro();tex_bombear(8);gfx_novo_quadro();
  glClearColor(.04,.04,.04,1);glClear(GL_COLOR_BUFFER_BIT);g5ui_draw();SDL_GL_SwapWindow(window);SDL_Delay(8);
}
static void shot(SDL_Window *window,const char *dir,const char *name){
  for(int i=0;i<100;i++)frame(window);
  g5ui_draw();unsigned char *pixels=malloc(1920*1080*4);assert(pixels);
  SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_RGBA32);assert(surface);
  glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  for(int y=0;y<1080;y++)memcpy((char*)surface->pixels+y*surface->pitch,pixels+(1079-y)*1920*4,1920*4);
  char path[1024];snprintf(path,sizeof path,"%s/%s.png",dir,name);assert(IMG_SavePNG(surface,path)==0);
  free(pixels);SDL_FreeSurface(surface);
}
static void fixture(void){
  inventory=calloc(72,sizeof *inventory);assert(inventory);count=72;total=count;source_loaded=1;
  const char *categories[]={"FILMES: Science fiction","FILMES: Adventure","FILMES: Drama","SERIES: Science fiction","SERIES: Drama","SERIES: Mystery"};
  const char *titles[]={"Fallout","Widow's Bay","The Prestige","One Battle After Another","From","Frequency","Outcome","The Devil Wears Prada","The Invite","Prisoners","Spaceman","We Bury the Dead"};
  for(int i=0;i<count;i++){
    G5Item *it=&inventory[i];it->kind=i<36?G5_MOVIE:G5_SERIES;it->age=10;
    snprintf(it->id,sizeof it->id,"%d",i+1);snprintf(it->source,sizeof it->source,"xtream");
    snprintf(it->group,sizeof it->group,"%s",categories[i/12]);snprintf(it->name,sizeof it->name,"%s",titles[i%12]);
    snprintf(it->poster,sizeof it->poster,"deploy/app/art/poster/%02d.jpg",i%12);
    snprintf(it->backdrop,sizeof it->backdrop,"deploy/app/art/%02d.jpg",i%6);
#ifndef VOD_BASELINE
    snprintf(it->year,sizeof it->year,"2024");snprintf(it->rating,sizeof it->rating,"8.5");snprintf(it->genre,sizeof it->genre,"Science fiction");
#endif
  }
  snprintf(draft.name,sizeof draft.name,"Living room IPTV");build_groups();go("vod-filmes");focus=VOD_ITEM;
}
int main(int argc,char **argv){
  assert(argc==2);assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0);IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_Window *window=SDL_CreateWindow("VOD proof",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1920,1080,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);assert(window);
  assert(SDL_GL_CreateContext(window));SDL_GL_SetSwapInterval(0);glViewport(0,0,1920,1080);gfx_tamanho_alvo(1920,1080);
  assert(gfx_iniciar());assert(txt_iniciar("deploy/app",1));tex_iniciar(64);gfx_icones_dir("deploy/app/art");dados_iniciar(getenv("NUVIO_DADOS"));
  dados_gravar("ajustes.txt","idioma 1\nidiomaAutoLocal 0\nselected_theme 2\nescalaTextoLocal 2\nescalaTextoVersaoLocal 1\n");
  ajustes_dir(getenv("NUVIO_DADOS"));gfx_escala_txt_definir(ajustes_escala_texto());ajustes_definir_vidro(1);context();fixture();
#ifdef VOD_BASELINE
  shot(window,argv[1],"before");
#else
  shot(window,argv[1],"after");
  assert(vod_rows==6&&vod_size==72&&vod_layout[0].count==12&&vod_layout[3].kind==G5_SERIES);
  assert(!strcmp(vod_category("FILMES: Adventure"),"Adventure"));
  assert(!strcmp(vod_category("SERIES:"),i18n("Outros")));
  assert(!strcmp(vod_category("  FILMES: Adventure"),"Adventure"));
  focus=VOD_ITEM+2;frame(window);
  /* Metadata-only refresh must retain poster controls and remote focus. */
  operation=5;atomic_store(&busy,1);
  for(int i=0;i<3;i++){
    g5ui_update();frame(window);assert(focus==VOD_ITEM+2);
    int visible=0;for(int j=0;j<num_focus;j++)if(next_focus[j]==focus)visible=1;
    assert(visible);
  }
  key(SDLK_RIGHT);assert(focus==VOD_ITEM+3);g5ui_update();frame(window);
  assert(focus==VOD_ITEM+3);key(SDLK_LEFT);assert(focus==VOD_ITEM+2);
  atomic_store(&busy,0);g5ui_update();frame(window);assert(focus==VOD_ITEM+2);
  /* Hold a real metadata worker inside the provider call while activating UI. */
  snprintf(action,sizeof action,"get_vod_info&vod_id=3");assert(start(5));
  pthread_mutex_lock(&metadata_lock);
  while(!metadata_entered)pthread_cond_wait(&metadata_cond,&metadata_lock);
  pthread_mutex_unlock(&metadata_lock);
  key(SDLK_RETURN);assert(!strcmp(screen,"vod-titulo")&&selected==2);
  focus=0;key(SDLK_RETURN);assert(playing);playing=0;
  go("vod-filmes");focus=807;key(SDLK_RETURN);assert(!strcmp(screen,"vod-titulo"));
  go("vod-filmes");focus=809;key(SDLK_RETURN);assert(playing);playing=0;
  go("vod-filmes");focus=803;key(SDLK_RETURN);assert(!strcmp(screen,"fontes-gerenciar"));
  /* Foreground work must get its own worker while metadata remains stalled. */
  action[0]=0;
  assert(start(3));assert(operation==3&&atomic_load(&busy));
  while(!atomic_load(&ready))SDL_Delay(1);
  g5ui_update();assert(!atomic_load(&busy));
  pthread_mutex_lock(&metadata_lock);metadata_release=1;pthread_cond_broadcast(&metadata_cond);pthread_mutex_unlock(&metadata_lock);
  while(!atomic_load(&metadata.ready))SDL_Delay(1);
  g5ui_update();assert(!atomic_load(&metadata.busy)&&!atomic_load(&busy));
  go("vod-filmes");focus=VOD_ITEM+2;frame(window);
  int initial=focus;key(SDLK_RIGHT);assert(focus==initial+1);frame(window);
  assert(poster_focus(focus)>0);key(SDLK_DOWN);assert(focus==VOD_ITEM+15);frame(window);assert(vod_row_at(vod_position())==1);
  key(SDLK_UP);assert(focus==VOD_ITEM+3);key(SDLK_UP);assert(focus==807&&nav<0);
  shot(window,argv[1],"hero-actions");
  key(SDLK_RIGHT);assert(focus==808);key(SDLK_LEFT);assert(focus==807);
  key(SDLK_LEFT);assert(focus==809);key(SDLK_RIGHT);assert(focus==807);
  key(SDLK_UP);assert(nav==3);key(SDLK_DOWN);assert(nav<0&&focus==VOD_ITEM+3);
  key(SDLK_ESCAPE);assert(nav==3&&!strcmp(screen,"vod-filmes"));key(SDLK_DOWN);assert(focus==VOD_ITEM+3);
  key(SDLK_UP);key(SDLK_RETURN);assert(!strcmp(screen,"vod-titulo")&&selected==3);
  key(SDLK_ESCAPE);assert(!strcmp(screen,"vod-filmes")&&focus==807);
  key(SDLK_RIGHT);key(SDLK_RETURN);assert(!strcmp(screen,"vod-series"));frame(window);assert(vod_rows==3&&vod_size==36);
  go("vod-filmes");focus=VOD_ITEM;frame(window);key(SDLK_UP);key(SDLK_LEFT);assert(focus==809);key(SDLK_RETURN);assert(playing&&selected==0);playing=0;
  go("vod-filmes");focus=VOD_ITEM;frame(window);
  for(int i=0;i<10;i++)key(SDLK_RIGHT);assert(focus==VOD_ITEM+10);
  shot(window,argv[1],"horizontal");assert(vod_layout[0].scroll>0);
  for(int i=0;i<4;i++)key(SDLK_DOWN);assert(vod_row_at(vod_position())==4);
  shot(window,argv[1],"series-rows");assert(vod_scroll>1000);assert(poster_focus(focus)>.95f);
  go("vod-filmes");focus=VOD_ITEM;inventory[0].backdrop[0]=0;
  for(int i=0;i<12;i++){inventory[i].year[0]=inventory[i].rating[0]=inventory[i].genre[0]=0;}
  shot(window,argv[1],"poster-fallback");
  ajustes_definir_vidro(0);shot(window,argv[1],"solid");ajustes_definir_vidro(1);
  /* Compact index includes records beyond the former 4096 ceiling, without
   * expanding them all into full G5Item records. Same provider category IDs
   * in movies and series must remain separate rows. */
  clear_catalog();inventory=calloc(1,sizeof *inventory);source_loaded=1;
  size_t bytes=1024*1024;char *json=calloc(bytes,1);assert(json);size_t used=0;
  used+=snprintf(json+used,bytes-used,"[");
  for(int i=0;i<4300;i++)used+=snprintf(json+used,bytes-used,"%s{\"stream_id\":%d,\"name\":\"Movie %d\",\"category_id\":\"1\"}",i?",":"",i+1,i);
  snprintf(json+used,bytes-used,"]");vod_data.json[0]=json;
  vod_data.categories[0]=strdup("[{\"category_id\":\"1\",\"category_name\":\"FILMES: Full catalog\"}]");
  vod_data.json[1]=strdup("[{\"series_id\":1,\"name\":\"Series\",\"category_id\":\"1\"}]");
  vod_data.categories[1]=strdup("[{\"category_id\":\"1\",\"category_name\":\"SERIES: Full catalog\"}]");
  query[0]=catalog_group[0]=0;go("vod-filmes");vod_rebuild();vod_filter();
  assert(vod_size==4301&&vod_rows==2&&count==0);
  assert(!strcmp(vod_item(vod_order[4299])->name,"Movie 4299"));
  vod_select(4299,0);assert(selected==0&&!strcmp(inventory[selected].name,"Movie 4299")&&count==1);
  go("vod-filmes");snprintf(query,sizeof query,"Movie 4299");vod_filter();assert(vod_size==1);
  query[0]=0;rules.hide_locked=1;rules.ask_unrated=1;vod_filter();assert(vod_size==0);
  /* Import confirmation must discard retained catalogs and invalidate cached rows. */
  rules.hide_locked=rules.ask_unrated=0;vod_filter();assert(vod_size==4301);
  drop_kind(G5_MOVIE);vod_filter();
  assert(vod_size==1&&vod_rows==1&&vod_layout[0].kind==G5_SERIES);
  assert(!vod_data.json[0]&&!vod_data.categories[0]);
  vod_select(0,0);assert(inventory[selected].kind==G5_SERIES);
  drop_kind(G5_SERIES);vod_filter();
  assert(vod_size==0&&vod_rows==0&&count==0);
  assert(!vod_data.json[1]&&!vod_data.categories[1]);
  puts("PASS: disabled import kinds stay removed; six category rows, movie/series order, poster focus, activation during stalled metadata, concurrent foreground worker, hero actions, Back, scrolling, fallback and uncapped lazy records");
#endif
  return 0;
}
