// Focused phone rendering proof. Offline catalog/source fixtures; production renderers.
#include "home.h"
#include "detail.h"
#include "busca.h"
#include "ajustes.h"
#include "streams.h"
#include "vaitocar.h"
#include "badges.h"
#include "dados.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "extras.h"
#include "ponteiro.h"
#include "menu.h"
#include "layout.h"
#include "escala.h"
#include "idioma.h"
#include "addons.h"
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SDL_Window *window;
static GLuint target, texture;
static int width=1398, height=2034;
static Uint32 clockNow=100000;
static const char *screen;
static void frame(void) {
  clockNow+=16;
  SDL_PumpEvents();
  txt_novo_quadro(); tex_novo_quadro(); tex_bombear(16); gfx_novo_quadro();
  ponteiro_quadro(clockNow);
  glBindFramebuffer(GL_FRAMEBUFFER,target);
  glViewport(0,0,width,height);
  glClearColor(.025,.025,.03,1); glClear(GL_COLOR_BUFFER_BIT);
  if (!strcmp(screen,"home")) { home_atualizar(1.f/60,clockNow); home_desenhar(clockNow); }
  if (!strcmp(screen,"settings")) { ajustes_atualizar(1.f/60,clockNow); ajustes_desenhar(clockNow); }
  if (!strcmp(screen,"search")) { busca_atualizar(1.f/60,clockNow); busca_desenhar(clockNow); }
  if (!strcmp(screen,"sources")) { stream_folha_atualizar(1.f/60,clockNow); stream_folha_desenhar(clockNow); }
  gfx_ambiente_descarregar();
  ponteiro_desenhar();
  SDL_Delay(4);
}
static void sourceHeader(void) {
  const PonteiroAlvo *targets;
  int n=ponteiro_teste_lista(&targets), count=0;
  PonteiroAlvo buttons[2];
  for(int i=0;i<n;i++) if(targets[i].focar && targets[i].x>650 && targets[i].y>270 && targets[i].y<390 && targets[i].w<100) {
    if(count<2) buttons[count]=targets[i];
    count++;
  }
  if(count!=2) for(int i=0;i<n;i++) fprintf(stderr,"target %d %.1f %.1f %.1f %.1f focus=%d\n",i,targets[i].x,targets[i].y,targets[i].w,targets[i].h,targets[i].focar!=NULL);
  assert(count==2);
  SDL_Event e={0}; e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_RETURN;
  buttons[0].focar(buttons[0].a,buttons[0].b); stream_folha_evento(&e);
  assert(stream_folha_recarregar()); assert(stream_folha_aberta());
  buttons[1].focar(buttons[1].a,buttons[1].b); stream_folha_evento(&e);
  assert(!stream_folha_aberta());
}
static void searchSeam(void) {
  // Background above controls must stay continuous at the old 80% canvas edge.
  unsigned char left[4],right[4];
  int x=(int)(width*.8f), y=height-100;
  glReadPixels(x-2,y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,left);
  glReadPixels(x+2,y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,right);
  for(int i=0;i<3;i++) assert(abs((int)left[i]-right[i])<8);
}
static void save(const char *root,const char *name) {
  unsigned char *pixels=malloc((size_t)width*height*4);
  SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,width,height,32,SDL_PIXELFORMAT_RGBA32);
  assert(pixels&&s); glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  for(int y=0;y<height;y++) memcpy((char *)s->pixels+y*s->pitch,pixels+(size_t)(height-1-y)*width*4,(size_t)width*4);
  char path[900]; snprintf(path,sizeof path,"%s/%s.png",root,name); assert(!IMG_SavePNG(s,path));
  free(pixels); SDL_FreeSurface(s);
}
int main(int argc,char **argv) {
  assert(argc==2);
  assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0); IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  window=SDL_CreateWindow("M3 offline proof",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
  assert(window&&SDL_GL_CreateContext(window));
  layout_tela_definir(width,height); layout_modo_definir(1);
  layout_insets_definir(0,0,72.f/466,0);
  gfx_tamanho_alvo(width,height); assert(gfx_iniciar()); assert(txt_iniciar("deploy/app",1));
  tex_iniciar(128); gfx_icones_dir("deploy/app/art"); badges_carregar("deploy/app/art");
  dados_iniciar(getenv("NUVIO_DADOS")); ajustes_dir(dados_dir());
  char path[900]; snprintf(path,sizeof path,"%s/ajustes.txt",dados_dir());
  FILE *f=fopen(path,"w"); assert(f); fprintf(f,"idioma 1\ninterfaceModoLocal 1\nfontePrepararLocal 1\ntrailerHero 1\n"); fclose(f);
  ajustes_iniciar();
  layout_tela_definir(width,height); layout_modo_definir(1);
  gfx_tamanho_alvo(width,height);
  assert(nv_layout_h>nv_layout_w);
  ponteiro_teste_janela(width,height); ponteiro_teste_toque(1);
  glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  gfx_tex_esquecer(0);
  glGenFramebuffers(1,&target); glBindFramebuffer(GL_FRAMEBUFFER,target);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
  assert(!strcmp(i18n("Fontes"),"Sources"));
  assert(!strcmp(i18n("Melhor para este iPhone"),"Best for this iPhone"));
  snprintf(path,sizeof path,"%s/addons.txt",dados_dir());
  f=fopen(path,"w"); assert(f); fprintf(f,"Offline\thttps://example.invalid/manifest.json\n"); fclose(f);
  addons_carregar(dados_dir());
  assert(addons_n()==1);
  assert(home_iniciar("deploy/app/art"));
  layout_tela_definir(width,height); layout_modo_definir(1); gfx_tamanho_alvo(width,height);
  static CatItem items[12]; CatFileira row={0};
  for(int i=0;i<12;i++) {
    snprintf(items[i].titulo,sizeof items[i].titulo,"Fallout");
    snprintf(items[i].imdb,sizeof items[i].imdb,"tt%07d",219150+i);
    snprintf(items[i].tipo,sizeof items[i].tipo,"movie");
    snprintf(items[i].meta,sizeof items[i].meta,"2024 · 120 min");
    snprintf(items[i].genero,sizeof items[i].genero,"Movie · Sci-Fi");
    snprintf(items[i].sinopse,sizeof items[i].sinopse,"An offline catalog fixture for phone rendering.");
    snprintf(items[i].backdrop,sizeof items[i].backdrop,"deploy/app/art/19.jpg");
    snprintf(items[i].poster,sizeof items[i].poster,"deploy/app/art/poster/19.jpg");
    snprintf(items[i].logo,sizeof items[i].logo,"deploy/app/art/19.jpg");
    items[i].nota=84;
    ExResumo resumo={0}; resumo.pronto=1;
    snprintf(resumo.status,sizeof resumo.status,"Released");
    extras_resumo_definir(items[i].imdb,&resumo);
  }
  snprintf(row.chave,sizeof row.chave,"pop_movie"); snprintf(row.titulo,sizeof row.titulo,"Popular movies");
  snprintf(row.tipo,sizeof row.tipo,"movie"); row.ini=0; row.n=12;
  cat_definir_tudo(items,12,&row,1);
  Stream sources[3]={0};
  for(int i=0;i<3;i++) {
    snprintf(sources[i].provedor,sizeof sources[i].provedor,"%s",i?"Torrentio":"AIOStreams");
    snprintf(sources[i].rotulo,sizeof sources[i].rotulo,"Fallout 2160p DV Atmos");
    snprintf(sources[i].arquivo,sizeof sources[i].arquivo,"Fallout.2024.2160p.DV.Atmos.%d.mp4",i);
    snprintf(sources[i].url,sizeof sources[i].url,"https://example.invalid/%d.mp4",i);
    sources[i].altura=i==2?1080:2160; sources[i].mp4=1; sources[i].dolbyVision=1; sources[i].dolbyAtmos=1;
    sources[i].badges=badges_detectar(sources[i].arquivo); sources[i].tamanhoMB=8500-i*1000; sources[i].fileIdx=-1;
  }
  stream_definir_alvo(items[0].imdb); stream_definir_lista(sources,3);
  VaiTocar vt; assert(vt_montar(&vt,items[0].imdb,0,0)&&!strcmp(vt.res,"4K"));
  assert(!vt_montar(&vt,"tt-other",0,0));
  screen="home"; for(int i=0;i<120;i++) frame();
  assert(home_fileiras_n()>0); assert(addons_n()>0);
  save(argv[1],"home");
  stream_folha_nome("Fallout"); stream_folha_contexto("Fallout · 2024"); stream_folha_abrir();
  screen="sources"; for(int i=0;i<80;i++) frame(); save(argv[1],"sources"); sourceHeader();
  screen="search"; busca_iniciar(); for(int i=0;i<60;i++) frame(); searchSeam(); save(argv[1],"search");
  screen="settings"; for(int i=0;i<60;i++) frame(); save(argv[1],"settings");
  assert(ajustes_shot_valor("idioma",7));
  for(int i=0;i<4;i++) frame(); save(argv[1],"settings-long-title");
  assert(ajustes_shot_valor("idioma",2));
  char status[32];
  assert(extras_status_titulo(items[0].imdb,status,sizeof status)&&!strcmp(status,"Released"));
  assert(!extras_status_titulo("tt-other",status,sizeof status));
  // Repeated tab selection must still emit the event that closes Detail.
  menu_ir(MENU_INICIO); assert(menu_escolheu());
  menu_ir(MENU_INICIO); assert(menu_escolheu());
  puts("PASS: M3 phone renderers, source identity/status, header actions, Search seam, repeated tab selection");
  return 0;
}
