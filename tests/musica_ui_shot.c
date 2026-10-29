// Native GL capture using the same album renderer and frozen public catalog.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "../src/musicaui.c"
#include "dados.h"
#include "js.h"
static SDL_Window *window;
static char *load(const char *path) {
  FILE *f=fopen(path,"rb"); assert(f); fseek(f,0,SEEK_END); long n=ftell(f); rewind(f);
  char *b=calloc(n+1,1); assert(b); assert(fread(b,1,n,f)==(size_t)n); fclose(f); return b;
}
static void album(const char *root, int series, MusAlbum *a) {
  char path[1024]; snprintf(path,sizeof path,"%s/src/assets/%s.json",root,series?"series":"film");
  char *json=load(path); const char *p=js_array(json,NULL,"songs");
  memset(a,0,sizeof *a);
  js_texto(json,NULL,"collectionName",a->titulo,sizeof a->titulo);
  js_texto(json,NULL,"artistName",a->artista,sizeof a->artista);
  js_texto(json,NULL,"collectionViewUrl",a->apple,sizeof a->apple);
  a->ano=series?2013:2008;
  snprintf(a->capa,sizeof a->capa,"%s/src/assets/%s.jpg",root,series?"series":"film");
  for(;p&&a->n<MUS_FAIXAS_MAX;p=js_prox(js_fim(p))) {
    const char *end=js_fim(p); if(!end)break; int k=a->n++;
    a->faixa[k].n=k+1;
    js_texto(p,end,"trackName",a->faixa[k].titulo,sizeof a->faixa[k].titulo);
    js_texto(p,end,"artistName",a->faixa[k].artista,sizeof a->faixa[k].artista);
    js_texto(p,end,"previewUrl",a->faixa[k].previa,sizeof a->faixa[k].previa);
    a->faixa[k].dur=(int)js_num(p,end,"trackTimeMillis",0)/1000;
  }
  assert(a->n== (series?19:14)); free(json);
}
static void shot(const char *path) {
  unsigned char *p=malloc(1920*1080*4); assert(p);
  glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,p);
  SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_RGBA32);assert(s);
  for(int y=0;y<1080;y++)memcpy((char*)s->pixels+y*s->pitch,p+(1079-y)*1920*4,1920*4);
  assert(!IMG_SavePNG(s,path));SDL_FreeSurface(s);free(p);printf("PNG %s\n",path);
}
static void key(SDL_Keycode k) { SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=k;musicaui_evento(&e); }
int main(int argc,char **argv) {
  assert(argc==3);assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));IMG_Init(IMG_INIT_JPG|IMG_INIT_PNG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  window=SDL_CreateWindow("Music native proof",0,0,1920,1080,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);assert(window);
  SDL_GLContext gl=SDL_GL_CreateContext(window);assert(gl);SDL_GL_SetSwapInterval(0);
  gfx_tamanho_alvo(1920,1080);assert(gfx_iniciar());assert(txt_iniciar("deploy/app",1));assert(tex_iniciar(32));
  char art[1024];assert(realpath("deploy/app/art",art));gfx_icones_dir(art);dados_iniciar(art);
  assert(dados_gravar("aparencia-20.txt","1\n"));
  assert(dados_gravar("ajustes.txt","idioma 0\nselected_theme 10\n")); ajustes_iniciar(); ajustes_dir(dados_dir());
  assert(ajustes_idioma()==0);
  // Capture uses Portuguese and the existing Arctic Blue accent setting.
  static MusAlbum a;
  const char *names[]={"filme","foco-faixa","serie","guardado"};
  for(int mode=0;mode<4;mode++) {
    album(argv[1],mode==2,&a);musicaui_abrir(&a,mode==2?"tt0944947":"tt0468569",mode==2?"Game of Thrones":"The Dark Knight",a.ano,mode==2?3:0);
    if(mode==1) {ui.p.ativa=ui.p.pronta=1;ui.p.faixa=2;ui.foco=7;ui.pendente=1;}
    if(mode==3) {ui.foco=3;key(SDLK_RETURN);assert(ui.guardado);}
    for(int frame=0;frame<100;frame++) {
      SDL_PumpEvents();tex_bombear(5);txt_novo_quadro();tex_novo_quadro();gfx_novo_quadro();
      Uint32 now=SDL_GetTicks();
      if(mode==1) { ui.p.ativa=0; musicaui_atualizar(now); ui.p.ativa=1;ui.p.faixa=2;ui.pendente=1; }
      else musicaui_atualizar(now);
      ui.entrada=now-1000;ui.mudou=now-1000;
      if(mode==1) {
        VideoSimulacao sim={0}; sim.pos=12; video_simular(&sim); ui.pendente=0;
      }
      musicaui_desenhar(now);if(frame<99)SDL_GL_SwapWindow(window);
      SDL_Delay(5);
    }
    char path[1024];snprintf(path,sizeof path,"%s/app-%s.png",argv[2],names[mode]);shot(path);
    if(mode==1) {key(SDLK_ESCAPE);assert(ui.aberta&&!ui.p.ativa);}
    if(mode==0) {
      key(SDLK_DOWN);for(int i=0;i<7;i++)key(SDLK_DOWN);assert(pagina()==1&&ui.foco==12);
      key(SDLK_LEFT);assert(ui.foco==1);key(SDLK_UP);assert(ui.foco==0);
    }
    musicaui_fechar();assert(!ui.aberta&&!ui.p.ativa);
  }
  tex_encerrar();SDL_GL_DeleteContext(gl);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
