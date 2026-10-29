/* Exercise the real UI worker with category-scoped Xtream responses. */
#include "xtream.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdatomic.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
static atomic_int requests, full_lists, category_requests, fail_items, empty_leading;
static char *fixture_request(const char *server,const char *user,const char *password,const char *action,int seconds,int *status){
  (void)server;(void)user;(void)password;(void)seconds;
  atomic_fetch_add(&requests,1);*status=200;
  if(strstr(action,"categories")){
    atomic_fetch_add(&category_requests,1);
    return strdup("[{\"category_id\":7,\"category_name\":\"Drama\"},{\"category_id\":8,\"category_name\":\"Adventure\"},{\"category_id\":9,\"category_name\":\"Mystery\"}]");
  }
  if(!strstr(action,"category_id=")){atomic_fetch_add(&full_lists,1);*status=413;return NULL;}
  if(atomic_load(&fail_items)){*status=503;return NULL;}
  /* Slow provider: UI must keep drawing while this worker waits. */
  SDL_Delay(80);
  char *body=malloc(900000);assert(body);size_t n=0;body[n++]='[';
  int category=atoi(strstr(action,"category_id=")+12);
  if(atomic_load(&empty_leading)&&category<9){free(body);return strdup("[]");}
  for(int i=0;i<4000;i++)n+=snprintf(body+n,900000-n,"%s{\"stream_id\":%d,\"series_id\":%d,\"name\":\"%s title %04d\",\"category_id\":%d,\"container_extension\":\"mp4\"}",i?",":"",category*10000+i,category*10000+i,category==9?"Hidden":"Fixture",i,category);
  strcpy(body+n,"]");return body;
}
#define xtream_consultar fixture_request
#ifndef GUIAV5_UNDER_TEST
#define GUIAV5_UNDER_TEST "../src/guiav5ui.c"
#endif
#include GUIAV5_UNDER_TEST
#undef xtream_consultar
#include <SDL2/SDL_image.h>
static void frame(SDL_Window *window){
  SDL_PumpEvents();txt_novo_quadro();tex_novo_quadro();gfx_novo_quadro();
  glClearColor(.04,.04,.04,1);glClear(GL_COLOR_BUFFER_BIT);g5ui_draw();SDL_GL_SwapWindow(window);
}
static void shot(SDL_Window *window,const char *name){
  for(int i=0;i<90;i++){frame(window);SDL_Delay(8);}
  g5ui_draw();unsigned char *pixels=malloc(1920*1080*4);assert(pixels);
  SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_RGBA32);assert(surface);
  glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  for(int y=0;y<1080;y++)memcpy((char*)surface->pixels+y*surface->pitch,pixels+(1079-y)*1920*4,1920*4);
  char path[1024];snprintf(path,sizeof path,"/Volumes/ExternalSSD/tmp/rev/vodcarga/%s.png",name);assert(!IMG_SavePNG(surface,path));free(pixels);SDL_FreeSurface(surface);
}
static void pump(void){g5ui_update();vod_filter();SDL_Delay(1);}
static atomic_int lifetime_finished;
static void *lifetime_worker(void *unused){
  (void)unused;
  /* Simulate a transport that takes time to acknowledge cancellation. */
  while(!atomic_load(&worker_cancel))SDL_Delay(1);
  SDL_Delay(250);
  atomic_store(&lifetime_finished,1);atomic_store(&ready,1);return NULL;
}
static void lifetime_test(const char *test){
  inventory=calloc(1,sizeof *inventory);assert(inventory);count=1;
  inventory[0].kind=G5_MOVIE;strcpy(inventory[0].name,"Mixed lifetime title");
  strcpy(inventory[0].source,"m3u");strcpy(inventory[0].group,"Mixed");
  if(!strcmp(test,"clear")){
    worker_cancel=0;atomic_store(&busy,1);
    assert(!pthread_create(&worker,NULL,lifetime_worker,NULL));pthread_detach(worker);
    Uint32 began=SDL_GetTicks();clear_catalog();
    assert(SDL_GetTicks()-began<50&&"account switch must never wait for a worker");
    assert(!inventory&&!count);
    assert(!start(8)&&"cancelled staging stays owned until completion");
    while(atomic_load(&busy)&&SDL_GetTicks()-began<2000)pump();
    assert(atomic_load(&lifetime_finished)&&!atomic_load(&busy));
    assert(!staging&&!vod_pending.refs&&!vod_data.count);
  }else{
    assert(start(8));
    Uint32 began=SDL_GetTicks();
    while(!atomic_load(&ready)&&SDL_GetTicks()-began<3000)SDL_Delay(1);
    assert(atomic_load(&ready));
    assert(!vod_pending.count&&"background indexing must not read shared inventory");
    g5ui_update();
    assert(vod_data.count==1&&vod_item(0)==&inventory[0]);
    clear_catalog();
  }
  printf("VOD inventory lifetime (%s): PASS\n",test);
}
int main(int argc,char **argv){
  assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));IMG_Init(IMG_INIT_PNG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_Window *window=SDL_CreateWindow("VOD loading proof",0,0,1920,1080,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);assert(window&&SDL_GL_CreateContext(window));
  SDL_GL_SetSwapInterval(0);glViewport(0,0,1920,1080);gfx_tamanho_alvo(1920,1080);
  assert(gfx_iniciar()&&txt_iniciar("deploy/app",1));tex_iniciar(64);gfx_icones_dir("deploy/app/art");dados_iniciar(getenv("NUVIO_DADOS"));ajustes_dir(getenv("NUVIO_DADOS"));context();
  assert(xtream_conta_definir("http://fixture.invalid","fixture","fixture"));
  draft.movies=1;draft.series=0;draft.live=1;committed=1;
  if(argc>1&&!strcmp(argv[1],"empty")){
    atomic_store(&empty_leading,1);g5ui_open("vod-filmes");
    Uint32 began=SDL_GetTicks();
    while(vod_data.count<4000&&SDL_GetTicks()-began<2000)pump();
    assert(vod_data.count==4000&&"empty leading categories must not starve later rows");
    while(atomic_load(&busy))pump();
    assert(vod_rows==1&&vod_done_count[0]==3);
    puts("VOD empty leading categories: PASS");return 0;
  }
  if(argc>1&&!strcmp(argv[1],"search")){
    for(int k=0;k<2;k++){
      clear_catalog();query[0]=0;draft.movies=!k;draft.series=k;
      g5ui_open(k?"vod-series":"vod-filmes");
      Uint32 began=SDL_GetTicks();
      while((vod_done_count[k]<2||atomic_load(&busy))&&SDL_GetTicks()-began<3000)pump();
      assert(vod_done_count[k]==2&&"only the first two categories of this kind should be loaded");
      snprintf(query,sizeof query,"Hidden title 0007");vod_filter();
      assert(!vod_size&&"target must be absent before loading the unopened category");
      began=SDL_GetTicks();Uint32 longest=0;
      while((!vod_size||atomic_load(&busy))&&SDL_GetTicks()-began<3000){
        Uint32 tick=SDL_GetTicks();pump();frame(window);
        Uint32 elapsed=SDL_GetTicks()-tick;if(elapsed>longest)longest=elapsed;
      }
      assert(vod_size==1&&"search must discover a title unique to the unopened third category");
      assert(!strcmp(vod_item(vod_order[0])->name,"Hidden title 0007"));
      assert(longest<80&&"search loading must not block UI on the provider");
      assert(vod_done_count[k]==3&&!atomic_load(&full_lists));
      int before=atomic_load(&requests);for(int i=0;i<5;i++)pump();
      assert(atomic_load(&requests)==before&&"completed search categories must not reload");
      printf("VOD unopened-category search (%s): PASS, longest frame %u ms\n",k?"series":"movies",longest);
    }
    return 0;
  }
  if(argc>1){lifetime_test(argv[1]);return 0;}
  g5ui_open("vod-filmes");shot(window,"loading-skeleton");
  Uint32 began=SDL_GetTicks();
  while(!atomic_load(&ready)&&SDL_GetTicks()-began<3000)SDL_Delay(1);
  assert(!atomic_load(&full_lists)&&"must request categories before scoped items; full list fails on Android-sized panels");
  Uint32 longest_frame=0;
  while(vod_data.count<4000&&SDL_GetTicks()-began<3000){
    Uint32 tick=SDL_GetTicks();pump();frame(window);
    Uint32 elapsed=SDL_GetTicks()-tick;if(elapsed>longest_frame)longest_frame=elapsed;
  }
  printf("longest UI frame during 80 ms provider wait: %u ms\n",longest_frame);
  assert(longest_frame<80);
  assert(vod_data.count==4000&&SDL_GetTicks()-began<500);printf("first category: %u ms, %d items\n",SDL_GetTicks()-began,vod_data.count);
  shot(window,"filled-grid");
  /* Cached startup must not wait for a failing provider. */
  while(atomic_load(&busy))pump();
  clear_catalog();atomic_store(&fail_items,1);began=SDL_GetTicks();g5ui_open("vod-filmes");
  while(vod_data.count<4000&&SDL_GetTicks()-began<1000)pump();
  assert(vod_size>=4000&&SDL_GetTicks()-began<250);
  printf("cached startup: %u ms, %d items\n",SDL_GetTicks()-began,vod_size);
  while(atomic_load(&busy))pump();
  assert(vod_failed&&vod_data.count==8000);
  atomic_store(&fail_items,0);activate(810);
  Uint32 retry=SDL_GetTicks();
  while((vod_failed||atomic_load(&busy)||!vod_done_count[0])&&SDL_GetTicks()-retry<2000)pump();
  assert(!vod_failed&&vod_done_count[0]>0&&vod_data.count==8000);
  while(atomic_load(&busy))pump();
  /* The third category stays lazy until remote focus reaches its neighbour. */
  int before=atomic_load(&requests);for(int i=0;i<5;i++)pump();assert(atomic_load(&requests)==before);
  focus=VOD_ITEM+vod_layout[1].start;pump();
  while(atomic_load(&busy))pump();assert(vod_data.count==12000);
  draft.series=1;source_loaded=0;go("vod-series");vod_begin();
  Uint32 series_begin=SDL_GetTicks();
  while(vod_done_count[1]<1&&SDL_GetTicks()-series_begin<2000)pump();
  assert(vod_done_count[1]>0);while(atomic_load(&busy))pump();
  /* Another account must never see the previous account's cached titles. */
  assert(xtream_conta_definir("http://fixture.invalid","other-account","other-secret"));
  clear_catalog();atomic_store(&fail_items,1);g5ui_open("vod-filmes");
  while(atomic_load(&busy))pump();
  assert(!vod_data.count&&vod_failed);
  assert(atomic_load(&full_lists)>0&&"failed scoped requests must try full-list fallback");
  puts("VOD category fixture: PASS");return 0;
}
