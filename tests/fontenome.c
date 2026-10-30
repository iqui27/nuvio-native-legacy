// #735: real Sources rendering, offline release names, weak-TV effects.
#include "text.h"
static void probe(TxtLinha l, float x, float y, float a);
#define txt_desenhar_alpha probe
#include "../src/streams.c"
#undef txt_desenhar_alpha
#include <assert.h>
#include <SDL2/SDL_image.h>

static const char *output;
static Uint32 clockMs=100000;
static GLuint target, color;
static SDL_Surface *strip;
static int watching, seen;
static float nameX, nameY, nameW;
static TxtLinha drawn;
static char release[512];
static void probe(TxtLinha l, float x, float y, float a) {
  if (watching && fabsf(y-nameY)<.1f && a>.5f) {
    drawn=l; nameX=x; seen++;
    assert(glIsEnabled(GL_SCISSOR_TEST));
  }
  txt_desenhar_alpha(l,x,y,a);
}
static void frame(void) {
  txt_novo_quadro(); tex_novo_quadro(); tex_bombear(8);
  stream_folha_atualizar(.016f,clockMs);
  glBindFramebuffer(GL_FRAMEBUFFER,target); glViewport(0,0,1920,1080);
  glClearColor(.055f,.065f,.08f,1); glClear(GL_COLOR_BUFFER_BIT); gfx_novo_quadro();
  if (grupo==1) nameY=FOLHA_TOPO+linhaTopo(foco)-rolagem+20+(temMarca(filtrado(foco),automaticaDaFolha())?FOLHA_MARCA_H:0)+40+FOLHA_SELO_H+12;
  seen=0; stream_folha_desenhar(clockMs);
}
static void advance(unsigned ms) {
  for (unsigned i=0;i<ms;i+=16) { clockMs+=16; frame(); }
}
static void capture(const char *label,int tile) {
  unsigned char *pixels=malloc(1920*1080*4); assert(pixels);
  SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_RGBA32); assert(s);
  glFinish(); glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  for(int y=0;y<1080;y++) memcpy((char *)s->pixels+y*s->pitch,pixels+(1079-y)*1920*4,1920*4);
  char path[1024]; snprintf(path,sizeof path,"%s/%s.png",output,label); assert(!IMG_SavePNG(s,path));
  if(tile>=0) { SDL_Rect dst={tile*960,0,960,540}; SDL_BlitScaled(s,NULL,strip,&dst); }
  free(pixels); SDL_FreeSurface(s);
}
static void fixture(int server,int description) {
  Stream sources[3]={0};
  for(int i=0;i<3;i++) {
    sources[i].altura=1080; sources[i].mp4=1; sources[i].fileIdx=-1; sources[i].tamanhoMB=3000-i*100;
    snprintf(sources[i].rotulo,sizeof sources[i].rotulo,"Torrentio 1080p");
    snprintf(sources[i].provedor,sizeof sources[i].provedor,"%s",server?"Local Jellyfin":"Torrentio");
    if(server) snprintf(sources[i].bingeGroup,sizeof sources[i].bingeGroup,"%s",i?"emby|fixture":"jellyfin|fixture");
    // No playback or network requests; P2P eligibility is unchanged.
    snprintf(sources[i].url,sizeof sources[i].url,"https://fixture.invalid/%d.mp4",i);
    snprintf(description?sources[i].descricao:sources[i].arquivo,
             description?sizeof sources[i].descricao:sizeof sources[i].arquivo,"%s",release);
  }
  stream_definir_alvo("tt735:2:5"); stream_definir_lista(sources,3);
  stream_folha_nome("A Quiet Journey"); stream_folha_contexto("Season 2 · Episode 5");
  stream_folha_canal(0); stream_folha_item(-1); stream_folha_abrir();
  watching=0; advance(960);
  // Start a fresh focus dwell after the real opening spring has settled.
  grupo=0; stream_folha_desenhar(clockMs); grupo=1;
  const Stream *s=&lista[filtrado(foco)];
  nameY=FOLHA_TOPO+linhaTopo(foco)-rolagem+20+(temMarca(filtrado(foco),automaticaDaFolha())?FOLHA_MARCA_H:0)+40+FOLHA_SELO_H+12;
  nameW=FOLHA_W-FOLHA_PAD_E-FOLHA_PAD_D-2*FOLHA_TXT;
  assert(!strcmp(arquivoDa(s),release) && "release text must not truncate before rasterization");
  watching=1; frame(); assert(seen==1);
}
int main(int argc,char **argv) {
  assert(argc==2); output=argv[1];
  SDL_SetHint("SDL_MAC_BACKGROUND_APP","1"); assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)); IMG_Init(IMG_INIT_PNG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_Window *win=SDL_CreateWindow("Sources release name",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
  assert(win && SDL_GL_CreateContext(win)); SDL_GL_SetSwapInterval(0);
  glGenTextures(1,&color); glBindTexture(GL_TEXTURE_2D,color); glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1920,1080,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
  glGenFramebuffers(1,&target); glBindFramebuffer(GL_FRAMEBUFFER,target); glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
  gfx_tamanho_alvo(1920,1080); assert(gfx_iniciar()); assert(txt_iniciar("deploy/app",1)); assert(tex_iniciar(64));
  gfx_icones_dir("deploy/app/art"); ajustes_dir(getenv("NUVIO_DADOS")); ajustes_iniciar();
  ajustes_shot_valor("fonteTextoLocal",0); ajustes_shot_valor("fonteOrdemLocal",1);
  gfx_definir_efeitos_leves(1); gfx_definir_efeitos_minimos(1);
  snprintf(release,sizeof release,"A.Quiet.Journey.S02E05.1080p.WEB-DL.H264.DDP5.1.Dual.Audio.English.Portuguese.Subtitles.Complete.Uncut.Extended.Edition.Release.Group.Final.mkv");
  strip=SDL_CreateRGBSurfaceWithFormat(0,3840,540,32,SDL_PIXELFORMAT_RGBA32); assert(strip);
  fixture(0,0);
  float start=nameX; GLuint texture=drawn.tex;
  TxtLinha full;
  { ESCALA_INI();
    full=txt_linha_bruta(TXT_PG_FIM,release,125,125,123,255);
    ESCALA_FIM(); }
  assert(full.w>nameW); float end=full.w-nameW;
  advance(768); assert(fabsf(nameX-start)<1); capture("01-dwell",0);
  int raster=txt_rasterizadas;
  advance(800); assert(nameX<start-10 && "focused filename must move after 800 ms");
  assert(drawn.tex==texture && drawn.w==full.w && "move cached full texture");
  capture("02-scrolling",1);
  advance((unsigned)(end/24*1000)-768+16); assert(fabsf(nameX-(start-end))<1);
  capture("03-end",2); advance(512); assert(fabsf(nameX-(start-end))<1);
  assert(txt_rasterizadas==raster && "no per-frame rasterization during marquee");
  advance(800); assert(fabsf(nameX-start)<1); capture("04-loop",3);
  char path[1024]; snprintf(path,sizeof path,"%s/four-frame-strip.png",output); assert(!IMG_SavePNG(strip,path));
  // Full-sheet warm frames: movement must keep drawing and fill cost bounded.
  watching=0;
  int sheetDraws[2]; double sheetFill[2], sheetMs[2];
  for(int moving=0;moving<2;moving++) {
    Uint64 begin=SDL_GetPerformanceCounter();
    for(int i=0;i<120;i++) {
      nomeRolDesde=moving?clockMs-3000:clockMs;
      frame();
    }
    sheetMs[moving]=(SDL_GetPerformanceCounter()-begin)*1000.0/SDL_GetPerformanceFrequency()/120;
    sheetDraws[moving]=gfx_n_rect; sheetFill[moving]=gfx_fill_gpu;
  }
  printf("weak-TV sheet: dwell %.4f ms, moving %.4f ms; GPU fill %.6f / %.6f; draws %d / %d\n",
         sheetMs[0],sheetMs[1],sheetFill[0],sheetFill[1],sheetDraws[0],sheetDraws[1]);
  assert(sheetDraws[0]==sheetDraws[1] && fabs(sheetFill[0]-sheetFill[1])<.0001);
  assert(sheetMs[1]<=sheetMs[0]+.1);
  // Compare the original ellipsized slot with the moving cached texture.
  { ESCALA_INI();
  double costs[2], fills[2]; int draws[2];
  for(int focused=0;focused<2;focused++) {
    for(int i=0;i<8;i++) { txt_novo_quadro(); desenharNomeFonte(release,filtrado(foco),focused,100,100,nameW,1,clockMs); }
    raster=txt_rasterizadas;
    Uint64 begin=SDL_GetPerformanceCounter();
    for(int i=0;i<180;i++) {
      txt_novo_quadro(); gfx_novo_quadro();
      desenharNomeFonte(release,filtrado(foco),focused,100,100,nameW,1,clockMs+i*16);
      assert(txt_rasterizadas==raster);
    }
    costs[focused]=(SDL_GetPerformanceCounter()-begin)*1000.0/SDL_GetPerformanceFrequency()/180;
    fills[focused]=gfx_fill_gpu; draws[focused]=gfx_n_rect;
  }
  printf("weak-TV slot: ellipsis %.4f ms, marquee %.4f ms; GPU fill %.6f / %.6f; draws %d / %d; rasterized=0\n",
         costs[0],costs[1],fills[0],fills[1],draws[0],draws[1]);
  assert(draws[0]==draws[1] && fills[1]<=fills[0]+.0001);
  assert(costs[1]<=costs[0]+.05);
  ESCALA_FIM(); }
  // Both description fallback and local media servers share this renderer.
  strcat(release,".Original.Multilingual.Release.Name.With.More.Than.192.Bytes.Preserved.Until.The.End.mkv");
  fixture(0,1); fixture(1,0); capture("local-server",-1);
  char variant[512]; snprintf(variant,sizeof variant,"%s",release); variant[strlen(variant)-5]='Z';
  { ESCALA_INI();
  TxtLinha first=txt_linha_bruta(TXT_PG_FIM,release,125,125,123,255);
  TxtLinha second=txt_linha_bruta(TXT_PG_FIM,variant,125,125,123,255);
  assert(first.tex && second.tex && first.tex!=second.tex && "long shared prefixes need distinct cached textures");
  ESCALA_FIM(); }
  // Focus loss freezes the outgoing spring row at an ellipsized name.
  float rest=nameX; grupo=0; advance(16);
  assert(drawn.w<=nameW && fabsf(nameX-rest)<1);
  watching=0; advance(960);
  grupo=1; frame(); assert(nomeRolDesde==clockMs && "refocus restarts dwell");
  foco=1; advance(16); assert(nomeRolIndice==filtrado(foco) && nomeRolDesde==clockMs);
  watching=0;
  // Live-guide sources use the same body with the channel scope enabled.
  stream_folha_canal(1); stream_filtro_tipo("tv"); stream_folha_abrir(); advance(960);
  assert(canalFolha && nomeRolIndice>=0); capture("guide-sources",-1);
  // The player island invokes the same body through its existing callback.
  CatItem item={0}; snprintf(item.tipo,sizeof item.tipo,"series"); cat_definir(&item,1);
  player_abrir(0,NULL); player_erro_fonte(); player_limpar_erro_fonte();
  stream_folha_canal(0); stream_folha_item(0); stream_folha_abrir();
  nomeRolIndice=-1;
  corpoIlhaFolha((GfxRect){1920-FOLHA_W-24,24,FOLHA_W,1032},1,NULL);
  assert(folhaIlha() && nomeRolIndice>=0);
  puts("PASS: focused release reaches end, pauses, loops, reuses texture; description and local server paths");
  return 0;
}
