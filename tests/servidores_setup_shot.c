// Offline fixtures, production setup renderer and keyboard navigation.
#include "../src/srvsetup.c"
#include "rail_shot.h"
#include <SDL2/SDL_image.h>
#include <assert.h>
static GLuint target;
static void capture(const char *dir, const char *name) {
  for (int i=0;i<15;i++) {
    SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(8); gfx_novo_quadro();
    glBindFramebuffer(GL_FRAMEBUFFER,target);glViewport(0,0,1920,1080);glClear(GL_COLOR_BUFFER_BIT);
    srvsetup_desenhar(codeSince+28000); rail_shot_desenhar(MENU_BIBLIOTECA);glFinish();
  }
  unsigned char *pixels=malloc(1920*1080*4); assert(pixels);
  glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_RGBA32);assert(surface);
  for(int y=0;y<1080;y++)memcpy((char*)surface->pixels+y*surface->pitch,pixels+(1079-y)*1920*4,1920*4);
  char path[900];snprintf(path,sizeof path,"%s/%s-app.png",dir,name);assert(!IMG_SavePNG(surface,path));puts(path);
  free(pixels);SDL_FreeSurface(surface);
}
static void key(SDL_Keycode k){SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=k;srvsetup_evento(&e);}
int main(int argc,char **argv){
  assert(argc==2);SDL_SetHint("SDL_MAC_BACKGROUND_APP","1");assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);
  dados_iniciar(getenv("NUVIO_DADOS"));dados_gravar("ajustes.txt","idioma 0\njellyfinLocal 0\nbordaFocoCartaz 1\n");ajustes_dir(dados_dir());
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_Window *w=SDL_CreateWindow("Personal servers proof",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(w);assert(SDL_GL_CreateContext(w));
  GLuint texture;glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1920,1080,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
  glGenFramebuffers(1,&target);glBindFramebuffer(GL_FRAMEBUFFER,target);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
  gfx_tamanho_alvo(1920,1080);assert(gfx_iniciar());assert(txt_iniciar("deploy/app",1));tex_iniciar(64);gfx_icones_dir("deploy/app/art");rail_shot_aplicar();
  opened=1;profile=perfis_ativo();provider=0;codeSince=1000;change(CHOOSE);capture(argv[1],"setup-escolher");
  key(SDLK_RIGHT);assert(selected==1);key(SDLK_LEFT);assert(selected==0);key(SDLK_ESCAPE);assert(!opened);
  struct {int state,p;const char *name;} scenes[]={
    {CODE,0,"setup-plex-pin"},{CODE,1,"setup-jellyfin-quickconnect"},{CONNECT_CODE,2,"setup-emby-connect"},
    {DISCOVER,0,"setup-plex-descoberta"},{DISCOVER,1,"setup-jellyfin-descoberta"},{DISCOVER,2,"setup-emby-descoberta"},
    {ADDRESS,0,"setup-plex-manual"},{ADDRESS,1,"setup-manual"},{ADDRESS,2,"setup-emby-manual"},
    {USERS,1,"setup-jellyfin-usuario"},{USERS,2,"setup-emby-usuario"},
    {PASSWORD,1,"setup-jellyfin-senha"},{PASSWORD,2,"setup-emby-senha"},{CONNECT_PASSWORD,2,"setup-emby-credentials"},
    {LIBRARIES,0,"setup-plex-bibliotecas"},{LIBRARIES,1,"setup-bibliotecas"},{LIBRARIES,2,"setup-emby-bibliotecas"},
    {READY,0,"setup-plex-pronto"},{READY,1,"setup-pronto"},{READY,2,"setup-emby-pronto"},
    {MANAGE,0,"servidores-gerenciar"},{REMOVE,0,"servidor-remover"},{ERROR_SCREEN,2,"servidor-offline"},
    {ERROR_SCREEN,1,"setup-credencial-expirada"},{ROWS,0,"fileira-adicionar-servidor"}};
  for(unsigned i=0;i<sizeof scenes/sizeof *scenes;i++){
    opened=1;provider=scenes[i].p;change(scenes[i].state);worker.n=2;
    strcpy(code,provider==0?"H7KM":"482916");makeQR(provider==0?"https://plex.tv/link":provider==1?"http://jellyfin.example.test/web/#/quickconnect":"https://emby.media/pin");
    for(int j=0;j<2;j++){snprintf(worker.servers[j].nome,96,"Servidor %d",j+1);snprintf(worker.users[j].nome,96,"Perfil %d",j+1);}
    nLibraries=2;memset(libraries,0,sizeof libraries);strcpy(libraries[0].titulo,"Filmes");strcpy(libraries[1].titulo,"Séries");checked[0]=checked[1]=1;
    strcpy(username,"Marina");strcpy(password,"fixture");strcpy(addressText,"http://media.example.test:8096");
    capture(argv[1],scenes[i].name);
  }
  screen=REMOVE;selected=0;key(SDLK_RETURN);assert(screen==MANAGE);
  screen=LIBRARIES;nLibraries=2;selected=0;key(SDLK_RETURN);assert(!checked[0]);
  puts("Setup D-pad: bounded focus, Back, safe removal default, library toggle: PASS");
  return 0;
}
