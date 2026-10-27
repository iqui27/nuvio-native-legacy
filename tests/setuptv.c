// Real LAN pairing, native worker handoff, persistence and production rendering.
#include "../src/guiav5ui.c"
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <unistd.h>

static SDL_Window *window;
static const char *output;
static void capture(const char *name){
  for(int f=0;f<45;f++){
    SDL_PumpEvents();txt_novo_quadro();tex_novo_quadro();tex_bombear(8);gfx_novo_quadro();
    glClear(GL_COLOR_BUFFER_BIT);g5ui_draw();glFinish();SDL_Delay(8);
  }
  unsigned char *pixels=malloc(1920*1080*4);assert(pixels);
  SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_RGBA32);assert(s);
  glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  for(int y=0;y<1080;y++)memcpy((char*)s->pixels+y*s->pitch,pixels+(1079-y)*1920*4,1920*4);
  char path[1024];snprintf(path,sizeof path,"%s/%s.png",output,name);assert(!IMG_SavePNG(s,path));puts(path);
  SDL_FreeSurface(s);free(pixels);
}
static void request(const char *json){
  char command[6000];const char *token=strrchr(celular_url(),'/');assert(token);
  snprintf(command,sizeof command,"curl --fail -s --max-time 5 --data-urlencode 't=%s' 'http://127.0.0.1:%d%s' >/dev/null",json,celular_porta(),token);
  assert(!system(command));g5ui_update();
}
static void settle_worker(void){
  for(int i=0;i<1000&&(atomic_load(&busy)||atomic_load(&ready));i++){SDL_Delay(5);g5ui_update();}
  assert(!atomic_load(&busy));
}
int main(int argc,char **argv){
  assert(argc==2);output=argv[1];
  assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));assert(IMG_Init(IMG_INIT_PNG));
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  window=SDL_CreateWindow("TV setup proof",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(window&&SDL_GL_CreateContext(window));
  GLuint texture,target;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1920,1080,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
  glGenFramebuffers(1,&target);glBindFramebuffer(GL_FRAMEBUFFER,target);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
  glViewport(0,0,1920,1080);gfx_tamanho_alvo(1920,1080);assert(gfx_iniciar());assert(txt_iniciar("deploy/app",1));tex_iniciar(64);char art[1024];assert(realpath("deploy/app/art",art));gfx_icones_dir(art);
  dados_iniciar(getenv("NUVIO_DADOS"));dados_gravar("ajustes.txt","idioma 0\nidiomaAutoLocal 0\nselected_theme 2\n");ajustes_dir(dados_dir());ajustes_definir_vidro(0);context();
  phone();assert(phone_stage==0&&phone_step()==0&&!celular_conectado());
  capture("waiting");
  char command[512];snprintf(command,sizeof command,"curl --fail -s --max-time 5 'http://127.0.0.1:%d%s' >/dev/null",celular_porta(),strrchr(celular_url(),'/'));assert(!system(command));
  assert(celular_conectado()&&phone_step()==1);capture("connected");
  // Actual credential submission starts a worker; M3U fixture needs no provider.
  request("{\"step\":1,\"name\":\"Living room\",\"type\":2,\"file\":\"#EXTM3U\\n#EXTINF:-1 tvg-id=\\\"news\\\" group-title=\\\"News\\\",News HD\\nhttps://example.test/news.m3u8\\n#EXTINF:-1 tvg-id=\\\"sports\\\" group-title=\\\"Sports\\\",Sports HD\\nhttps://example.test/sports.m3u8\\n#EXTINF:-1 group-title=\\\"Adult\\\",Adult\\nhttps://example.test/adult.m3u8\\n\"}");
  assert(phone_stage==1&&atomic_load(&busy)&&celular_url()[0]);
  settle_worker();assert(phone_stage==2&&count==3&&group_count==3&&phone_step()==3);
  assert(atomic_load(&phone_counts[G5_LIVE])==3&&atomic_load(&phone_parts)==1);
  capture("review");
  // Freeze a published Xtream worker snapshot, never read its mutable staging.
  phone_stage=4;draft.type=0;atomic_store(&phone_parts,2);
  atomic_store(&phone_counts[G5_LIVE],0);atomic_store(&phone_counts[G5_MOVIE],186);atomic_store(&phone_counts[G5_SERIES],42);
  assert(phone_step()==2);capture("importing");
  phone_stage=2;draft.type=2;
  request("{\"step\":2,\"live\":1,\"movies\":0,\"series\":0,\"epg\":1,\"xmltv\":\"https://example.test/guide.xml\",\"off\":[1]}");
  assert(phone_stage==3&&phone_step()==4&&!strcmp(screen,"setup-pronto")&&committed&&!save_failed);
  assert(group_on[0]&&!group_on[1]&&!group_on[2]);
  assert(phone_epg_days()==0); // Never invent coverage from a URL alone.
  char file[64];filename(file,sizeof file,"source");char *saved=dados_ler(file);assert(saved&&strstr(saved,"Living room"));free(saved);
  capture("done");
  // Same geometry with glass enabled; reduced motion retains state meaning.
  ajustes_definir_vidro(1);capture("done-glass");ajustes_definir_vidro(0);
  phone_stage=2;go("setup-celular-tv");
  request("{\"step\":2,\"live\":1,\"movies\":0,\"series\":0,\"xmltv\":\"javascript:bad\"}");assert(phone_stage==2);
  request("{\"step\":2,\"live\":0,\"movies\":0,\"series\":0}");assert(phone_stage==2);
  phone_stage=3;go("setup-pronto");request("{\"step\":3}");assert(strcmp(screen,"setup-pronto"));
  phone();assert(phone_stage==0&&!celular_conectado()&&phone_step()==0);celular_fechar();
  assert(!celular_url()[0]);capture("expired");
  g5ui_close();puts("PASS: TV setup waiting, connection, worker progress, review, save, invalid choices, finish and reset");
  return 0;
}
