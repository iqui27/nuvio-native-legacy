// Real GL proof: row opacity and scissors must follow the morphing island.
#include "text.h"
static void checkedAlpha(TxtLinha l, float x, float y, float a);
static void checkedTom(TxtLinha l, float x, float y, float r, float g, float b, float a);
#define txt_desenhar_alpha checkedAlpha
#define txt_desenhar_tom checkedTom
#include "../src/streams.c"
#undef txt_desenhar_alpha
#undef txt_desenhar_tom
#include <assert.h>
#include <SDL2/SDL_image.h>

static int probing, rowDraws, checkedFrames;
static float maxAlpha;
static GLuint fbo, color;
static Uint32 now=100000;
static SDL_Surface *strip;
static const char *output;

static void checkText(TxtLinha l, float x, float y, float a) {
  (void)l; (void)x;
  if (!probing || y<FOLHA_TOPO || a<.001f) return;
  GfxRect panel=ilha_corpo_rect();
  float coverage=fminf(panel.w/FOLHA_W,panel.h/(folhaBase+16-NV_ILHA_Y));
  float reveal=anim_clamp((coverage-.96f)/.04f,0,1);
  assert(a<=reveal+.002f && "row alpha must wait for the island to settle");
  GLint clip[4]; glGetIntegerv(GL_SCISSOR_BOX,clip);
  float s=gfx_escala();
  assert(glIsEnabled(GL_SCISSOR_TEST));
  assert(clip[0]>=floorf(panel.x*s)-1 && clip[1]>=floorf(1080-(panel.y+panel.h)*s)-1);
  assert(clip[0]+clip[2]<=ceilf((panel.x+panel.w)*s)+1);
  assert(clip[1]+clip[3]<=ceilf(1080-panel.y*s)+1 && "row text escapes the panel clip");
  rowDraws++;
  if (a>maxAlpha) maxAlpha=a;
}
static void checkedAlpha(TxtLinha l, float x, float y, float a) {
  checkText(l,x,y,a); txt_desenhar_alpha(l,x,y,a);
}
static void checkedTom(TxtLinha l, float x, float y, float r, float g, float b, float a) {
  checkText(l,x,y,a); txt_desenhar_tom(l,x,y,r,g,b,a);
}
static void frame(void) {
  now+=16; SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(16);
  stream_folha_atualizar(.016f,now);
  glBindFramebuffer(GL_FRAMEBUFFER,fbo); glViewport(0,0,1920,1080);
  glClear(GL_COLOR_BUFFER_BIT); gfx_novo_quadro();
  gfx_cor((GfxRect){0,0,1920,1080},0,.10f,.13f,.18f,1);
  ilha_ancorar(1856,36,1); ilha_relogio_visivel(!stream_folha_aberta());
  rowDraws=0; maxAlpha=0;
  stream_folha_desenhar(now); ilha_desenhar(now);
  if (probing) checkedFrames++;
}
static void capture(int tile) {
  unsigned char *pixels=malloc(1920*1080*4); assert(pixels);
  SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_RGBA32); assert(s);
  glFinish(); glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  for(int y=0;y<1080;y++) memcpy((char *)s->pixels+y*s->pitch,pixels+(1079-y)*1920*4,1920*4);
  SDL_Rect dst={tile*960,0,960,540}; SDL_BlitScaled(s,NULL,strip,&dst);
  char path[1024]; snprintf(path,sizeof path,"%s/open-%03d.png",output,(tile+1)*25);
  assert(!IMG_SavePNG(s,path)); SDL_FreeSurface(s); free(pixels);
}
int main(int argc,char **argv) {
  assert(argc==3); output=argv[1]; int cold=!strcmp(argv[2],"cold");
  int reduced=!strcmp(argv[2],"reduced");
  SDL_SetHint("SDL_MAC_BACKGROUND_APP","1"); assert(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)); IMG_Init(IMG_INIT_PNG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_Window *win=SDL_CreateWindow("Sources motion",0,0,64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
  assert(win && SDL_GL_CreateContext(win)); SDL_GL_SetSwapInterval(0);
  glGenTextures(1,&color); glBindTexture(GL_TEXTURE_2D,color);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1920,1080,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
  glGenFramebuffers(1,&fbo); glBindFramebuffer(GL_FRAMEBUFFER,fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
  gfx_tamanho_alvo(1920,1080); assert(gfx_iniciar()); assert(txt_iniciar("deploy/app",1)); assert(tex_iniciar(64));
  gfx_icones_dir("deploy/app/art"); badges_carregar("deploy/app/art");
  ajustes_dir(getenv("NUVIO_DADOS")); ajustes_iniciar();
  ajustes_shot_valor("relogioTelaLocal",0); ajustes_definir_vidro(strcmp(argv[2],"solid")!=0);
  gfx_definir_efeitos_minimos(0);
  gfx_definir_efeitos_leves(strcmp(argv[2],"full")!=0); // TCL and full spring paths.
  anim_politica_reduzida=reduced;
  AddonRemoto addon={.nome="AIOStreams",.url="https://example.invalid/manifest.json",.ativo=1};
  addons_definir_lista(&addon,1);
  Stream sources[5]={0};
  for(int i=0;i<5;i++) {
    snprintf(sources[i].rotulo,sizeof sources[i].rotulo,"Fallout source %d",i+1);
    snprintf(sources[i].provedor,sizeof sources[i].provedor,"AIOStreams");
    snprintf(sources[i].url,sizeof sources[i].url,"https://example.invalid/%d.mkv",i);
    sources[i].tamanhoMB=9600-i*1300; sources[i].altura=i<3?2160:1080; sources[i].fileIdx=-1;
  }
  // A retained live request must not hide a movie/episode's source actions.
  stream_filtro_tipo("tv"); stream_folha_canal(1);
  stream_definir_alvo("tt12637874:1:3"); stream_definir_lista(sources,5); stream_definir_atual(0);
  CatItem title={0}; snprintf(title.tipo,sizeof title.tipo,"series");
  snprintf(title.imdb,sizeof title.imdb,"tt12637874"); cat_definir(&title,1);
  stream_folha_item(0); stream_folha_canal(0);
  stream_folha_nome("Fallout"); stream_folha_contexto("Fallout · S1E3");
  assert(nBotoes()==7 && "VOD must restore MP4/cache/audio and AIO actions after Live TV");
  if (!cold) for(int i=0;i<70;i++) frame();
  float px=0,py=0,pw=0,ph=0;
  if (!cold) assert(ilha_rect(&px,&py,&pw,&ph));
  stream_folha_abrir();
  assert(folhaRelogio && "clock-enabled Sources must morph even before the first clock frame");
  strip=SDL_CreateRGBSurfaceWithFormat(0,3840,540,32,SDL_PIXELFORMAT_RGBA32); assert(strip);
  probing=1;
  int tile=0;
  for(int i=0;i<100;i++) {
    frame();
    // Samples at 25/50/75/100% of the existing 640 ms opening window.
    if (!strcmp(argv[2],"warm") && (i==9 || i==19 || i==29 || i==39)) capture(tile++);
    if (!i && !cold && !reduced) {
      GfxRect panel=ilha_corpo_rect();
      assert(panel.w>pw && panel.w<pw+100 && panel.h>ph);
      assert(fabsf(panel.x+panel.w-px-pw)<1 && fabsf(panel.y-py)<1);
    }
    if (i==19 && !reduced) assert(maxAlpha<.99f && "mid-open rows must not pop at full opacity");
  }
  assert(rowDraws>0 && maxAlpha>.99f);
  int buttons=0, rows=0;
  for(int i=0;i<navigationCount;i++) {
    if(navigation[i].group==-1) buttons|=1<<botaoDe(navigation[i].id);
    if(navigation[i].group==1) rows++;
  }
  assert(buttons==((1<<BT_SO_MP4)|(1<<BT_CACHE)|(1<<BT_DUB)|(1<<BT_RECARREGAR)|
                   (1<<BT_AIO)|(1<<BT_AIO_GERENCIAR)|(1<<BT_FECHAR)));
  assert(rows>0 && "settled Sources buttons and rows must be focusable");
  SDL_Event ok={0}; ok.type=SDL_KEYDOWN; ok.key.keysym.sym=SDLK_RETURN;
  grupo=-1;
  for(int b=BT_SO_MP4;b<=BT_DUB;b++) {
    for(foco=0;foco<nBotoes() && botaoDe(foco)!=b;foco++);
    stream_folha_evento(&ok);
    assert(botaoLigado(b)); stream_folha_evento(&ok); assert(!botaoLigado(b));
  }
  for(foco=0;foco<nBotoes() && botaoDe(foco)!=BT_RECARREGAR;foco++);
  stream_folha_evento(&ok); assert(stream_folha_recarregar());
  grupo=1; foco=0; ok.key.keysym.sym=SDLK_i; stream_folha_evento(&ok);
  assert(stream_detalhe_aberto()); ok.key.keysym.sym=SDLK_ESCAPE; stream_folha_evento(&ok);
  assert(!stream_detalhe_aberto() && aberta);
  ok.key.keysym.sym=SDLK_RETURN; stream_folha_evento(&ok);
  int chosen=-1; assert(!aberta && stream_folha_escolheu(&chosen) && chosen>=0);
  stream_folha_abrir(); for(int i=0;i<100;i++) frame();
  if (!strcmp(argv[2],"warm")) { char path[1024]; snprintf(path,sizeof path,"%s/open-strip.png",output); assert(!IMG_SavePNG(strip,path)); }
  SDL_Event back={0}; back.type=SDL_KEYDOWN; back.key.keysym.sym=SDLK_ESCAPE; stream_folha_evento(&back);
  assert(!aberta);
  for(int i=0;i<100;i++) frame();
  assert(ilha_corpo_t()==0 && !rowDraws);
  assert(checkedFrames==300);
  printf("PASS: %s clock morph, mid-open alpha, clipped rows, reverse close (%d frames)\n",argv[2],checkedFrames);
  return 0;
}
