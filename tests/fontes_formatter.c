// #479: real source sheet, hidden GL window, local fixtures only.
#include "streams.h"
#include "nvtmp.h"
#include "limpa.h"
static int cleanCalls;
static size_t countedClean(const char *s, char *d, size_t n, int flags) {
  cleanCalls++;
  return nv_limpar_texto(s, d, n, flags);
}
#define nv_limpar_texto countedClean
#include "../src/streams.c"
#undef nv_limpar_texto
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <unistd.h>
#define CARDS 60
#define MOVES 118
static Stream fixtures[CARDS];
static void frame(void) {
  txt_novo_quadro();
  stream_folha_atualizar(1.0f/60, SDL_GetTicks());
  stream_folha_desenhar(SDL_GetTicks());
}
static void setting(const char *dir, int addon) {
  char path[512]; snprintf(path,sizeof path,"%s/ajustes.txt",dir);
  FILE *f=fopen(path,"w"); assert(f);
  fprintf(f,"fonteTextoLocal %d\nvidroLocal 1\n",addon);
  fclose(f); ajustes_dir(dir);
  assert(ajustes_fonte_texto_addon()==addon);
}
static void measure(const char *dir, int addon, int verify) {
  setting(dir,addon);
  stream_definir_lista(fixtures,CARDS); stream_folha_abrir();
  anim=1;
  // Build every card's expanded text WITHOUT focus. Moving focus must reuse it.
  grupo=0;
  for(int row=0;row<CARDS;row++) {
    linhaAnt=row; abreAnt=1; montar(automaticaDaFolha()); rolagem=linhaY[row];
    for(int j=0;j<3;j++) { txt_novo_quadro(); stream_folha_desenhar(SDL_GetTicks()); }
  }
  grupo=1; foco=0; focoVisto=0; linhaAnt=-1; abreAnt=0; abreFoco=1;
  rolagem=0;
  // Warm shared selected labels without warming all selected card titles.
  for(int j=0;j<4;j++) frame();
  int raster=txt_rasterizadas, cleans=cleanCalls;
  Uint64 start=SDL_GetPerformanceCounter();
  for(int j=0;j<MOVES;j++) {
    SDL_Event e={0}; e.type=SDL_KEYDOWN;
    e.key.keysym.sym=j<CARDS-1?SDLK_DOWN:SDLK_UP;
    stream_folha_evento(&e); frame();
  }
  double ms=(SDL_GetPerformanceCounter()-start)*1000.0/SDL_GetPerformanceFrequency()/MOVES;
  printf("formatter=%d cards=%d moves=%d ms/move=%.3f rasterized=%d cleaned=%d\n",
         addon,CARDS,MOVES,ms,txt_rasterizadas-raster,cleanCalls-cleans);
  fflush(stdout);
  if(verify) {
    assert(txt_rasterizadas==raster && "focus must reuse prewarmed text textures");
    if(addon) assert(cleanCalls==cleans && "focus must reuse parsed addon cards");
  }
}
// Compare the real sheet with the previous opaque RGB rendering, including
// antialiased edges and animation alpha over both grey and light artwork.
static int pixelLine(const char *label, TxtEstilo style, const char *s,
                     int r, int g, int b, float x, float y, float alpha,
                     int background, const char *icon) {
  unsigned char actual[640*48*4], expected[sizeof actual];
  int height=style==TXT_CALLOUT?40:28;
  size_t bytes=640*height*4;
  glReadPixels((int)x,1080-(int)y-height,640,height,GL_RGBA,GL_UNSIGNED_BYTE,actual);
  glClearColor(background/255.0f,background/255.0f,background/255.0f,1);
  glClear(GL_COLOR_BUFFER_BIT);
  if(icon) {
    gfx_icone((GfxRect){x,y+3,22,22},icon,r/255.0f,g/255.0f,b/255.0f,alpha);
    x+=30;
  }
  TxtLinha l=txt_linha(style,s,r,g,b,255);
  txt_desenhar_alpha(l,x,y,alpha);
  glReadPixels((int)x-(icon?30:0),1080-(int)y-height,640,height,GL_RGBA,GL_UNSIGNED_BYTE,expected);
  int mismatches=0, ink=0, iconInk=0;
  for(size_t p=0;p<bytes;p+=4) {
    if(expected[p]!=background) { ink++; if((p/4)%640<22) iconInk++; }
    for(int c=0;c<4;c++) if(abs(actual[p+c]-expected[p+c])>1) { mismatches++; break; }
  }
  printf("pixels %s bg=%d alpha=%.1f mismatches=%d ink=%d\n",label,background,alpha,mismatches,ink);
  return mismatches==0 && ink>0 && (!icon || iconInk>0);
}
static void pixels(const char *dir) {
  assert(tex_iniciar(64)); gfx_icones_dir("deploy/app/art");
  const char *paths[]={"deploy/app/art/icones/aj_hard-drive.png","deploy/app/art/icones/aj_volume-2.png"};
  for(int i=0;i<2;i++) {
    GLuint icon=0;
    for(int tries=0;tries<200 && !icon;tries++) {
      tex_bombear(8); icon=tex_obter_larg(paths[i],22);
      if(!icon) SDL_Delay(5);
    }
    assert(icon && "pixel assertion must draw real icons");
  }
  int ok=1;
  for(int addon=0;addon<=1;addon++) for(int failed=0;failed<=1;failed++)
    for(int bg=77;bg<=220;bg+=143) for(int fade=0;fade<=1;fade++) {
      setting(dir,addon);
      Stream s=fixtures[1];
      snprintf(s.rotulo,sizeof s.rotulo,"Pixel title");
      snprintf(s.descricao,sizeof s.descricao,"💾 Numbers\n🔊 Secondary");
      stream_definir_lista(&s,1); stream_folha_abrir();
      if(failed) stream_vod_falhou(0);
      grupo=0; linhaAnt=-1; abreAnt=0; rolagem=0;
      montar(automaticaDaFolha());
      float a=fade?.6f:1, x=FOLHA_PAD_E+FOLHA_TXT;
      float y=FOLHA_TOPO+linhaY[0]+20+(failed?FOLHA_MARCA_H:0);
      char title[96], ep[64]="";
      if(addon) snprintf(title,sizeof title,"%s",textoAddonDa(0)->titulo);
      else tituloConteudo(&lista[0],title,sizeof title,ep,sizeof ep);
      // Redraw for each crop because pixelLine clears the framebuffer.
      int count=addon?3:1;
      for(int line=0;line<count;line++) {
        txt_novo_quadro();
        glClearColor(bg/255.0f,bg/255.0f,bg/255.0f,1); glClear(GL_COLOR_BUFFER_BIT);
        corpoFolha(0,1920,a,SDL_GetTicks(),0);
        if(line==0) ok &= pixelLine(failed?"failed title":"title",TXT_CALLOUT,title,
                                   failed?105:205,failed?105:205,failed?103:203,x,y,a,bg,NULL);
        else {
          const LinhaAddon *l=&textoAddonDa(0)->linhas[line-1];
          ok &= pixelLine(line==1?"numbers + icon":"secondary + icon",TXT_PG_FIM,l->tx[0],
                          line==1?180:130,line==1?180:130,line==1?180:130,
                          x,y+40+(line-1)*FOLHA_ADDON_LD,a,bg,l->ic[0]);
        }
      }
      stream_vod_tocou(0);
    }
  fflush(stdout);
  assert(ok && "unfocused text and icons must preserve the previous RGB and alpha");
  tex_encerrar();
}
int main(int argc,char **argv) {
  int verify=argc==1 || strcmp(argv[1],"--measure");
  char dir[NVT_MAX]; nvt_set(dir, sizeof dir, "/nuvio-formatter479-XXXXXX"); assert(mkdtemp(dir));
  SDL_SetHint("SDL_MAC_BACKGROUND_APP","1");
  assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0); IMG_Init(IMG_INIT_PNG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_Window *win=SDL_CreateWindow("formatter479",0,0,1920,1080,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN); assert(win);
  SDL_GLContext gl=SDL_GL_CreateContext(win); assert(gl); SDL_GL_SetSwapInterval(0);
  gfx_tamanho_alvo(1920,1080); assert(gfx_iniciar()); assert(txt_iniciar("deploy/app",1));
  glViewport(0,0,1920,1080);
  for(int i=0;i<CARDS;i++) {
    Stream *s=&fixtures[i]; s->altura=1080; s->mp4=1; s->fileIdx=-1;
    snprintf(s->url,sizeof s->url,"https://fixture.invalid/%d.mp4",i);
    snprintf(s->provedor,sizeof s->provedor,"AIOStreams");
    snprintf(s->rotulo,sizeof s->rotulo,"⚡ 𝗕𝗹𝘂𝗥𝗮𝘆 Source %02d",i);
    snprintf(s->descricao,sizeof s->descricao,
      "💾 11.1 GB | 30.1 Mbps |\n🔊 DDP5.1 Atmos\n🌐 English | Portuguese\n⚙️ Release group\nMovie.2026.1080p.Source.%02d.mkv",i);
  }
  measure(dir,0,verify); measure(dir,1,verify);
  // Append keeps existing parsed cards, including when the backing array moves.
  int cleans=cleanCalls;
  stream_lista_acrescentar(&fixtures[0],1,0);
  const TextoAddon *old=textoAddonDa(0); assert(old->nl==5);
  assert(cleanCalls==cleans);
  const TextoAddon *added=textoAddonDa(CARDS); assert(added->nl==5);
  assert(cleanCalls>cleans);
  cleans=cleanCalls; textoAddonDa(CARDS); assert(cleanCalls==cleans);
  // A replacement of the same length must discard the previous card content.
  snprintf(fixtures[0].rotulo,sizeof fixtures[0].rotulo,"Replacement");
  snprintf(fixtures[0].descricao,sizeof fixtures[0].descricao,"New description");
  stream_definir_lista(fixtures,CARDS);
  const TextoAddon *replaced=textoAddonDa(0);
  assert(!strcmp(replaced->titulo,"Replacement") && replaced->nl==1);
  // Settings switch while the list survives; parsing is independent of style.
  setting(dir,0); montar(automaticaDaFolha());
  setting(dir,1); montar(automaticaDaFolha());
  cleans=cleanCalls; montar(automaticaDaFolha()); assert(cleanCalls==cleans);
  puts("formatter cache: append, replacement and setting changes OK");
  pixels(dir);
  txt_encerrar(); stream_definir_lista(NULL,0);
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(win); IMG_Quit(); SDL_Quit();
  char path[512]; snprintf(path,sizeof path,"%s/ajustes.txt",dir); unlink(path); rmdir(dir);
  return 0;
}
