// Native, offline screenshots of approved AIOStreams routes. No credentials.
#include "aiostreamsui.h"
#include "addonsui.h"
#include "addons.h"
#include "ajustes.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "streams.h"
#include "catalogo.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static GLuint fbo, color;
static unsigned char pixels[1920*1080*3];
static void save(const char *dir,const char *name) {
  glFinish(); glPixelStorei(GL_PACK_ALIGNMENT,1);
  glReadPixels(0,0,1920,1080,GL_RGB,GL_UNSIGNED_BYTE,pixels);
  SDL_Surface *s=SDL_CreateRGBSurface(0,1920,1080,24,0xff,0xff00,0xff0000,0); assert(s);
  for(int y=0;y<1080;y++) memcpy((char *)s->pixels+y*s->pitch,pixels+(1079-y)*1920*3,1920*3);
  char path[1024]; snprintf(path,sizeof path,"%s/aio-%s.png",dir,name); assert(!IMG_SavePNG(s,path)); SDL_FreeSurface(s);
  printf("PNG %s\n",path);
}
int main(int argc,char **argv) {
  assert(argc==2);
  SDL_SetHint("SDL_MAC_BACKGROUND_APP","1"); assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)); IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_Window *w=SDL_CreateWindow("AIOStreams",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN); assert(w); assert(SDL_GL_CreateContext(w));
  glGenTextures(1,&color); glBindTexture(GL_TEXTURE_2D,color); glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1920,1080,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
  glGenFramebuffers(1,&fbo); glBindFramebuffer(GL_FRAMEBUFFER,fbo); glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
  glViewport(0,0,1920,1080); gfx_tamanho_alvo(1920,1080); assert(gfx_iniciar()); assert(txt_iniciar("deploy/app",1)); assert(tex_iniciar(32)); gfx_icones_dir("deploy/app/art"); aioui_arte("deploy/app/art");
  ajustes_dir(getenv("NUVIO_DADOS")); ajustes_iniciar();
  static AioSnapshot s;
  aio_manifesto(&s,"{\"id\":\"aiostreams.test\",\"name\":\"AIOStreams\",\"version\":\"2.36.0\",\"resources\":[\"stream\",\"catalog\",\"subtitles\",\"meta\"],\"catalogs\":[{\"id\":\"trending\",\"name\":\"Filmes em alta\",\"type\":\"movie\"},{\"id\":\"popular\",\"name\":\"Séries populares\",\"type\":\"series\"},{\"id\":\"netflix\",\"name\":\"Filmes Netflix\",\"type\":\"movie\"},{\"id\":\"disney\",\"name\":\"Séries Disney+\",\"type\":\"series\"}]}");
  aio_configuracao(&s,"{\"data\":{\"userData\":{\"services\":[{\"id\":\"TorBox\",\"enabled\":true}],\"presets\":[{\"type\":\"Torrentio\",\"enabled\":true},{\"type\":\"Comet\",\"enabled\":true},{\"type\":\"MediaFusion\",\"enabled\":true},{\"type\":\"WebStreamr\",\"enabled\":true}],\"excludedResolutions\":[\"720p\"],\"preferredLanguages\":[\"Portuguese\",\"English\"],\"excludeUncached\":false,\"sortCriteria\":{\"global\":[{\"key\":\"cached\"},{\"key\":\"resolution\"}]},\"formatter\":{\"id\":\"aiostreams\"}}}}");
  aio_status(&s,"{\"success\":true,\"data\":{\"version\":\"2.36.0\",\"channel\":\"stable\",\"settings\":{\"searchApiDisabled\":false}}}");
  aio_search(&s,"{\"success\":true,\"data\":{\"statistics\":[{\"title\":\"Torrentio\",\"description\":\"4 resultados\"},{\"title\":\"Comet\",\"description\":\"2 resultados\"}],\"errors\":[{\"title\":\"WebStreamr\",\"description\":\"tempo esgotado\"}],\"filtered\":3}}");
  aio_resultados(&s,"{\"streams\":[{\"name\":\"4K · Dolby Vision · MP4\",\"url\":\"https://example.invalid/video\",\"streamData\":{\"cached\":true,\"addon\":\"Torrentio\",\"parsedFile\":{\"resolution\":\"2160p\"}}},{\"name\":\"4K · HDR10 · REMUX\",\"url\":\"https://example.invalid/video\",\"streamData\":{\"cached\":true,\"addon\":\"Comet\"}},{\"name\":\"1080p · Dolby Vision · MP4\",\"url\":\"https://example.invalid/video\",\"streamData\":{\"cached\":true,\"addon\":\"MediaFusion\"}},{\"name\":\"1080p · Blu-ray · SDR\",\"url\":\"https://example.invalid/video\",\"streamData\":{\"cached\":true,\"addon\":\"Torrentio\"}},{\"name\":\"720p · WEB-DL · SDR\",\"url\":\"https://example.invalid/video\",\"streamData\":{\"cached\":true,\"addon\":\"Torrentio\"}},{\"name\":\"1080p · HTTP\",\"url\":\"https://example.invalid/video\",\"streamData\":{\"addon\":\"WebStreamr\"}},{\"name\":\"Resolução desconhecida · MKV\",\"externalUrl\":\"https://example.invalid/external\",\"streamData\":{\"cached\":true,\"addon\":\"Comet\"}},{\"name\":\"4K · HDR10 · MKV\",\"infoHash\":\"abcdef\",\"streamData\":{\"addon\":\"Torrentio\"}}]}");
  aio_legendas(&s,"{\"subtitles\":[{\"lang\":\"por\",\"name\":\"Português\",\"forced\":false,\"sdh\":false},{\"lang\":\"eng\",\"name\":\"English\",\"forced\":true,\"sdh\":true}]}");
  for(int i=0;i<6;i++) { s.http[i]=200; s.ms[i]=184+i*60; }
  s.addon[s.addons++]=(AioItem){.nome="OpenSubtitles v3 Plus",.ativo=1,.legenda=1};
  s.addon[s.addons++]=(AioItem){.nome="SubDL",.ativo=1,.legenda=1};
  const char *names[]={"sem-config","setup-instancia","setup-importar","setup-servicos","setup-pronto","painel","catalogos","fontes","links","legendas","filtros","saude","titulo-debug"};
  for(int p=0;p<13;p++) {
    aioui_shot(p,&s);
    for(int frame=0;frame<160;frame++) {
      SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(8); SDL_Delay(8);
      glBindFramebuffer(GL_FRAMEBUFFER,fbo); glViewport(0,0,1920,1080); glClear(GL_COLOR_BUFFER_BIT); gfx_novo_quadro(); aioui_desenhar(100000);
    }
    save(argv[1],names[p]);
  }
  AddonRemoto add={.nome="AIOStreams",.url="https://example.invalid/stremio/sample/sample/manifest.json",.ativo=1}; addons_definir_lista(&add,1);
  // Drawing only: opening the list starts manifest network probes.

  for(int frame=0;frame<30;frame++) {
    txt_novo_quadro(); tex_novo_quadro(); tex_bombear(8); glBindFramebuffer(GL_FRAMEBUFFER,fbo); gfx_novo_quadro(); addonsui_desenhar(100000);
  }
  save(argv[1],"integracao-addons");
  CatItem movie={0}; strcpy(movie.titulo,"Batman: O Cavaleiro das Trevas");
  strcpy(movie.backdrop,"/Volumes/ExternalSSD/tmp/aiostreams-mock/assets/backdrop.png");
  cat_definir(&movie,1); stream_folha_item(0);
  Stream streams[2]={0}; strcpy(streams[0].provedor,"AIOStreams"); strcpy(streams[0].rotulo,"4K · Dolby Vision · MP4"); strcpy(streams[0].arquivo,"Torrentio · TorBox"); strcpy(streams[0].url,"https://example.invalid/video"); streams[0].altura=2160; streams[0].mp4=1; streams[0].tamanhoMB=10035;
  streams[1]=streams[0]; streams[1].altura=1080; streams[1].tamanhoMB=3277; strcpy(streams[1].rotulo,"1080p · Blu-ray · SDR"); strcpy(streams[1].arquivo,"Comet · TorBox");
  stream_definir_alvo("tt0468569"); stream_definir_lista(streams,2); stream_folha_nome("Batman: O Cavaleiro das Trevas"); stream_folha_abrir();
  for(int frame=0;frame<40;frame++) {
    aioui_shot(5,&s); aioui_desenhar(100000);
    SDL_Delay(8); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(8); stream_folha_atualizar(1.f/60,100000+frame*16); glBindFramebuffer(GL_FRAMEBUFFER,fbo); gfx_novo_quadro(); stream_folha_desenhar(100000+frame*16);
  }
  save(argv[1],"integracao-fontes");
  return 0;
}
