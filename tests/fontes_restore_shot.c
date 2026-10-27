// Native 1920x1080 comparison using the approved Fallout reference data.
#include "catalogo.h"
#include "ajustes.h"
#include "addons.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "streams.h"
#include "ilha.h"
#include "badges.h"
#include "menu.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLuint fbo, color;
static Uint32 now=100000;
static const char *out;
static float scale;
static void frame(int count) {
  for(int f=0;f<count;f++) {
    now+=16;
    SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(16);
    menu_atualizar(1.f/60,now); stream_folha_atualizar(1.f/60,now);
    glBindFramebuffer(GL_FRAMEBUFFER,fbo); glViewport(0,0,1920,1080);
    glClear(GL_COLOR_BUFFER_BIT); gfx_novo_quadro();
    GLuint art=tex_obter_hero("deploy/app/art/00.jpg");
    if(art) gfx_textura((GfxRect){0,0,1920,1080},art);
    menu_desenhar(now);
    ilha_ancorar((1920-64)/scale,36/scale,1);
    ilha_relogio_visivel(!stream_folha_aberta());
    stream_folha_desenhar(now); ilha_desenhar(now);
  }
}
static void save(const char *name) {
  static unsigned char pixels[1920*1080*3];
  SDL_Surface *s=SDL_CreateRGBSurface(0,1920,1080,24,0xff,0xff00,0xff0000,0); assert(s);
  glFinish(); glPixelStorei(GL_PACK_ALIGNMENT,1);
  glReadPixels(0,0,1920,1080,GL_RGB,GL_UNSIGNED_BYTE,pixels);
  for(int y=0;y<1080;y++) memcpy((char *)s->pixels+y*s->pitch,pixels+(1079-y)*1920*3,1920*3);
  char path[1024]; snprintf(path,sizeof path,"%s-%s.png",out,name);
  assert(!IMG_SavePNG(s,path)); SDL_FreeSurface(s); printf("PNG %s\n",path);
}
static void key(SDL_Keycode k) {
  SDL_Event e={0}; e.type=SDL_KEYDOWN; e.key.keysym.sym=k; stream_folha_evento(&e);
}
int main(int argc,char **argv) {
  assert(argc==4 || argc==5); out=argv[1]; scale=atof(argv[2])/100.f;
  int server=argc==5;
  SDL_SetHint("SDL_MAC_BACKGROUND_APP","1"); assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));
  IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_Window *win=SDL_CreateWindow("Sources restoration",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
  assert(win && SDL_GL_CreateContext(win)); SDL_GL_SetSwapInterval(0);
  glGenTextures(1,&color); glBindTexture(GL_TEXTURE_2D,color);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1920,1080,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
  glGenFramebuffers(1,&fbo); glBindFramebuffer(GL_FRAMEBUFFER,fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
  gfx_tamanho_alvo(1920,1080); assert(gfx_iniciar()); assert(txt_iniciar("deploy/app",1)); assert(tex_iniciar(64));
  gfx_icones_dir("deploy/app/art"); badges_carregar("deploy/app/art");
  char settings[1024]; snprintf(settings,sizeof settings,"%s/ajustes.txt",getenv("NUVIO_DADOS"));
  FILE *config=fopen(settings,"w"); assert(config);
  fprintf(config,"idioma 0\nselected_theme 2\nhomeLayoutLocal 0\nmodernSidebar 0\ncollapseSidebar 0\nmenuTopoLocal 0\n");
  fclose(config);
  ajustes_dir(getenv("NUVIO_DADOS")); ajustes_iniciar();
  ajustes_shot_valor("homeLayoutLocal",0); ajustes_shot_valor("fonteTextoLocal",0);
  ajustes_shot_valor("fonteOrdemLocal",0); ajustes_shot_valor("relogioTelaLocal",0);
  gfx_escala_txt_definir(scale);
  setenv("NUVIO_SHOT_VIDRO_FOSCO","1",1); ajustes_teste_vidro_env();
  ajustes_definir_vidro(!strcmp(argv[3],"glass"));
  assert(ajustes_vidro()==!strcmp(argv[3],"glass"));
  assert(!ajustes_menu_topo() && !ajustes_rail_recolhida());
  menu_iniciar();
  AddonRemoto a={.nome="AIOStreams",.url="https://example.invalid/stremio/test/test/manifest.json",.ativo=1};
  addons_definir_lista(&a,1);
  static Stream st[5];
  const char *prov[]={"AIOStreams","AIOStreams","Torrentio","AIOStreams","Torrentio"};
  const long mb[]={18637,9626,7987,2150,1840};
  for(int i=0;i<5;i++) {
    snprintf(st[i].rotulo,sizeof st[i].rotulo,"Fallout");
    snprintf(st[i].provedor,sizeof st[i].provedor,"%s",prov[i]);
    snprintf(st[i].url,sizeof st[i].url,"https://example.invalid/%d.mkv",i);
    st[i].tamanhoMB=mb[i]; st[i].altura=i<3?2160:1080; st[i].fileIdx=-1;
  }
  st[0].badges=badges_bit("r-4k")|badges_bit("v-hdr10")|badges_bit("a-atmos")|badges_bit("q-remux");
  st[1].badges=badges_bit("r-4k")|badges_bit("v-dv")|badges_bit("a-ddp")|badges_bit("q-webdl");
  st[1].mp4=1; st[1].dolbyVision=1;
  st[2].badges=badges_bit("r-4k")|badges_bit("v-hdr10plus")|badges_bit("a-ddp");
  st[3].badges=badges_bit("r-1080")|badges_bit("a-ddp"); st[4].badges=badges_bit("r-1080");
  if(server) {
    snprintf(st[0].provedor,sizeof st[0].provedor,"Plex · Casa");
    snprintf(st[0].bingeGroup,sizeof st[0].bingeGroup,"plex|fixture");
  }
  stream_definir_alvo("tt12637874:1:3"); stream_definir_lista(st,5); stream_definir_atual(0);
  stream_folha_nome("Fallout"); stream_folha_contexto("Fallout · T1E3");
  frame(70); save("clock");
  float px,py,pw,ph,x,y,w,h; assert(ilha_rect(&px,&py,&pw,&ph));
  stream_folha_abrir(); frame(1); save("open-016");
  assert(ilha_rect(&x,&y,&w,&h));
  assert(w>pw && h>ph && w<400 && fabsf(x+w-px-pw)<1);
  frame(5); save("open-096"); frame(8); save("open-224"); frame(70); save("list");
  assert(ilha_rect(&x,&y,&w,&h)); assert(fabsf(w-820)<1 && fabsf(x+w-px-pw)<1);
  for(int i=0;i<12;i++) key(SDLK_UP);
  frame(40); save("actions");
  key(SDLK_DOWN); key(SDLK_RIGHT); frame(40); save("tabs");
  key(SDLK_ESCAPE); assert(!stream_folha_aberta());
  frame(1); save("close-016"); frame(5); save("close-096"); frame(8); save("close-224");
  frame(100); save("closed"); assert(ilha_rect(&x,&y,&w,&h));
  // The real clock can cross a minute while capturing, changing its text width.
  assert(ilha_corpo_t()==0 && w<200 && fabsf(h-ph)<1);
  puts("PASS: AIO installed; compact sheet, clock origin, reverse motion, D-pad actions/tabs");
  return 0;
}
