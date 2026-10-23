#include "addonguia.h"
#include "addons.h"
#include "ajustes.h"
#include "dados.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "streams.h"
#include "perfis.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <OpenGL/gl.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void tecla(SDL_Keycode k) {
  SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=k;addonguia_evento(&e);
}
int main(int argc,char **argv) {
  assert(argc==2);
  dados_iniciar(getenv("NUVIO_DADOS"));
  assert(dados_gravar("ajustes.txt","idioma 0\nidiomaAutoLocal 0\n"));
  ajustes_dir(getenv("NUVIO_DADOS"));
  SDL_SetHint("SDL_MAC_BACKGROUND_APP","1");assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER));
  IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_Window *w=SDL_CreateWindow("addon-guidance",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(w);
  assert(SDL_GL_CreateContext(w));
  GLuint tex,fbo;glGenTextures(1,&tex);glBindTexture(GL_TEXTURE_2D,tex);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1920,1080,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
  glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,tex,0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
  glViewport(0,0,1920,1080);gfx_tamanho_alvo(1920,1080);
  assert(gfx_iniciar());assert(txt_iniciar("deploy/app",1));tex_iniciar(128);
  gfx_tex_esquecer(0);gfx_icones_dir("deploy/app/art");ajustes_definir_vidro(1);
  for(int c=0;c<ajustes_rapido_cor_n();c++)if(!strcmp(ajustes_rapido_cor_nome(c),"Rosa"))ajustes_rapido_cor_definir(c);
  AddonRemoto a[4]={{"Torrentio","https://torrentio.strem.fun/manifest.json",1},{"Xperience","https://xperience.invalid/manifest.json",1},{"Torrentio · Real-Debrid","https://torrentio.strem.fun/direct/manifest.json",1},{"AIOStreams","https://aio.invalid/manifest.json",1}};
  assert(addons_definir_lista(a,4));
  assert(dados_gravar("perfis.txt","1\t0\t1\t0\t#7553c8\tM\t\t\n"));
  perfis_carregar_ativo();
  for(int i=0;i<4;i++)addons_manifesto_lido(i,i==1?"{\"id\":\"xperience\",\"resources\":[\"catalog\",\"meta\",\"subtitles\"]}":"{\"id\":\"torrentio\",\"resources\":[\"stream\"]}");
  addonmodo_carregar("shot",1);
  uint64_t id=addonmodo_id(addons_base(0));
  Stream sources[2]={0};sources[0].addonIdentidade=id;strcpy(sources[0].infoHash,"0123456789012345678901234567890123456789");
  sources[1].addonIdentidade=id;strcpy(sources[1].url,"https://video.invalid/file.mp4");
  assert(addonmodo_set(id,AM_DIRETO));stream_definir_lista(sources,2);assert(stream_n()==1&&stream_item(0)->url[0]);
  assert(addonmodo_set(id,AM_AUTO));
  sources[0].appResolvido=1;strcpy(sources[0].url,"https://app-debrid.invalid/resolved.mp4");
  assert(addonmodo_set(id,AM_DIRETO));stream_definir_lista(sources,2);assert(stream_n()==1);
  assert(addonmodo_set(id,AM_AUTO));
  static const struct {const char *nome;int tela;} shots[]={
    {"novo-addon-torrent",AG_NOVO_TORRENT},{"novo-addon-catalogo",AG_NOVO_CATALOGO},
    {"novo-addon-debrid",AG_NOVO_DIRETO},{"escolher-modo",AG_MODO},
    {"conectar-debrid",AG_CONECTAR},{"nao-tocou-p2p",AG_P2P},
    {"nao-tocou-debrid",AG_DEBRID},{"nao-tocou-offline",AG_OFFLINE},
    {"nao-tocou-acesso-401",AG_401},{"nao-tocou-acesso-403",AG_403},
    {"nao-tocou-hdr",AG_HDR},{"nao-tocou-sem-fontes",AG_VAZIO},
    {"p2p-iniciando",AG_INICIANDO},{"meus-addons",AG_LISTA}
  };
  SDL_Surface *original=IMG_Load("/Volumes/ExternalSSD/tmp/addon-guia-mock/assets/detalhe.jpg");assert(original);
  SDL_Surface *backdrop=SDL_ConvertSurfaceFormat(original,SDL_PIXELFORMAT_RGBA32,0);assert(backdrop);SDL_FreeSurface(original);
  GLuint bg;glGenTextures(1,&bg);glBindTexture(GL_TEXTURE_2D,bg);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,backdrop->w,backdrop->h,0,GL_RGBA,GL_UNSIGNED_BYTE,backdrop->pixels);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  SDL_FreeSurface(backdrop);
  for(int i=0;i<14;i++) {
    addonmodo_resposta(id,200,1,0,2);
    addonmodo_resposta(addonmodo_id(addons_base(2)),200,1,2,0);
    addonmodo_resposta(addonmodo_id(addons_base(3)),-1,0,0,0);
    assert(addonmodo_set(id,shots[i].tela==AG_DEBRID?AM_DEBRID:shots[i].tela==AG_MODO||shots[i].tela==AG_INICIANDO?AM_P2P:AM_AUTO));
    if(shots[i].tela==AG_NOVO_DIRETO)addonmodo_resposta(id,200,1,2,0);
    addonguia_shot(shots[i].tela);
    if(shots[i].tela==AG_NOVO_CATALOGO)addonguia_abrir(AG_NOVO_CATALOGO,1,NULL);
    for(int f=0;f<40;f++) {
      SDL_PumpEvents();txt_novo_quadro();tex_novo_quadro();tex_bombear(12);gfx_novo_quadro();
      glClearColor(0.05f,0.05f,0.05f,1);glClear(GL_COLOR_BUFFER_BIT);
      gfx_textura((GfxRect){0,0,1920,1080},bg);
      gfx_veu_css((GfxRect){0,0,1920,1080},0,1,1,0.5f);
      addonguia_desenhar();SDL_Delay(6);
    }
    assert(bg);
    unsigned char *pixels=malloc(1920*1080*4);assert(pixels);glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_RGBA32);assert(s);
    for(int y=0;y<1080;y++)memcpy((char*)s->pixels+y*s->pitch,pixels+(1079-y)*1920*4,1920*4);
    char path[1024];snprintf(path,sizeof path,"%s/%s-app.png",argv[1],shots[i].nome);
    int erro=IMG_SavePNG(s,path);assert(!erro&&"savepng");
    SDL_FreeSurface(s);free(pixels);printf("PNG %s\n",path);
  }
  addonguia_shot(AG_MODO);tecla(SDLK_RETURN);
  assert(!addonguia_aberto()&&addonmodo_get(id)==AM_P2P&&ajustes_p2p_ligado());
  assert(addonguia_pedido()==AGP_REPETIR);
  addonguia_shot(AG_MODO);addonguia_desenhar();tecla(SDLK_RIGHT);tecla(SDLK_RETURN);
  assert(addonguia_shot_tela()==AG_CONECTAR);tecla(SDLK_ESCAPE);
  assert(addonguia_shot_tela()==AG_MODO);
  addonguia_desenhar();tecla(SDLK_RIGHT);tecla(SDLK_RIGHT);tecla(SDLK_RETURN);
  assert(!addonguia_aberto()&&addonmodo_get(id)==AM_DIRETO);
  assert(addonguia_pedido()==AGP_NADA);
  addonguia_configurar(2);assert(addonguia_shot_tela()==AG_NOVO_DIRETO);addonguia_fechar();
  addonguia_shot(AG_HDR);tecla(SDLK_RETURN);assert(addonguia_pedido()==AGP_PROXIMA);
  addonguia_shot(AG_CONECTAR);tecla(SDLK_ESCAPE);assert(addonguia_aberto());
  addonguia_fechar();stream_definir_lista(NULL,0);
  // Sem fonte escolhida, o 401 do laco vale como antes.
  addonmodo_nova_busca();addonmodo_resposta(id,401,0,0,0);addonguia_falha();assert(addonguia_shot_tela()==AG_401);
  // Com fonte de OUTRO addon (indice 3), o 401 do addon 0 nao puxa a tela.
  addonguia_fechar();uint64_t id3=addonmodo_id(addons_base(3));
  addonmodo_resposta(id3,200,1,2,0);
  Stream outras[1]={0};outras[0].addonIdentidade=id3;strcpy(outras[0].url,"https://video.invalid/outro.mp4");
  stream_definir_lista(outras,1);stream_definir_atual(0);
  addonguia_falha();assert(addonguia_shot_tela()!=AG_401);
  // E o 401 do addon da propria fonte continua valendo.
  addonguia_fechar();Stream mesma[1]={0};mesma[0].addonIdentidade=id;strcpy(mesma[0].url,"https://video.invalid/mesma.mp4");
  stream_definir_lista(mesma,1);stream_definir_atual(0);addonguia_falha();assert(addonguia_shot_tela()==AG_401);
  addonguia_fechar();stream_definir_lista(NULL,0);
  addonguia_fechar();addonmodo_resposta(id,403,0,0,0);addonguia_falha();assert(addonguia_shot_tela()==AG_403);
  addonguia_fechar();addonmodo_nova_busca();addonmodo_resposta(id,200,1,0,2);
  assert(addonmodo_set(id,AM_AUTO));ajustes_definir_p2p_ligado(0);addonguia_falha();assert(addonguia_shot_tela()==AG_P2P);
  addonguia_fechar();addonmodo_nova_busca();
  for(int a=0;a<4;a++)if(a!=1)addonmodo_resposta(addonmodo_id(addons_base(a)),200,1,0,0);
  addonguia_falha();assert(addonguia_shot_tela()==AG_VAZIO);tecla(SDLK_RETURN);assert(addonguia_pedido()==AGP_REPETIR);
  puts("addonguia shot: native mode/filter/actions OK");
}
