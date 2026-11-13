// #735: real Sources rendering, offline release names, weak-TV effects.
// The focused release name wraps in UTF-8 over as many lines as it needs
// (marquee removed 12/11): every byte reaches the renderer, no ellipsis.
#include "text.h"
static void probe(TxtLinha l, float x, float y, float a);
static TxtLinha probeLinha(TxtEstilo e, const char *s, int r, int g, int b, int a);
#define txt_desenhar_alpha probe
#define txt_linha probeLinha
#include "../src/streams.c"
#undef txt_desenhar_alpha
#undef txt_linha
#include <assert.h>
#include <SDL2/SDL_image.h>

static const char *output;
static Uint32 clockMs=100000;
static GLuint target, color;
static float nameW;
static char release[512];
// Filename pieces: arquivoDesenhar is the only TXT_PG_FIM 125/125/123 line.
typedef struct { char t[2048]; float x, y, w; } Piece;
static Piece rec[128];
static int nrec, pending;
static char pendingText[2048];
static TxtLinha probeLinha(TxtEstilo e, const char *s, int r, int g, int b, int a) {
  pending = e==TXT_PG_FIM && r==125 && g==125 && b==123;
  if (pending) snprintf(pendingText,sizeof pendingText,"%s",s);
  return txt_linha(e,s,r,g,b,a);
}
static void probe(TxtLinha l, float x, float y, float a) {
  if (pending && nrec < (int)(sizeof rec/sizeof *rec)) {
    Piece *p=&rec[nrec++]; snprintf(p->t,sizeof p->t,"%s",pendingText); p->x=x; p->y=y; p->w=(float)l.w;
    assert(glIsEnabled(GL_SCISSOR_TEST));
  }
  pending=0;
  txt_desenhar_alpha(l,x,y,a);
}
static float nameYOf(int row) {
  return FOLHA_TOPO+linhaTopo(row)-rolagem+20+(temMarca(filtrado(row),automaticaDaFolha())?FOLHA_MARCA_H:0)+40+FOLHA_SELO_H+12;
}
// Lines drawn from y0 down, one FOLHA_ARQ_H apart; checks they rebuild text.
static int rowPieces(float y0, const char *text) {
  char all[2048]=""; int k=0;
  for (;;k++) {
    int hit=-1;
    for (int j=0;j<nrec;j++) if (fabsf(rec[j].y-(y0+k*FOLHA_ARQ_H))<.1f) hit=j;
    if (hit<0) break;
    assert(rec[hit].w<=nameW+.5f && "wrapped line wider than the row");
    assert(!strstr(rec[hit].t,"…") && "release line ellipsized");
    strncat(all,rec[hit].t,sizeof all-strlen(all)-1);
  }
  if (k) assert(!strcmp(all,text) && "full release name must reach the renderer");
  return k;
}
// Same, for any position (guide, player island): all pieces of this frame.
static void allPieces(const char *text) {
  char all[2048]=""; assert(nrec>0);
  for (int j=0;j<nrec;j++) {
    assert(j==0 || (fabsf(rec[j].x-rec[0].x)<.1f && fabsf(rec[j].y-rec[j-1].y-FOLHA_ARQ_H)<.1f));
    strncat(all,rec[j].t,sizeof all-strlen(all)-1);
  }
  assert(!strcmp(all,text));
}
static void frame(void) {
  txt_novo_quadro(); tex_novo_quadro(); tex_bombear(8);
  stream_folha_atualizar(.016f,clockMs);
  glBindFramebuffer(GL_FRAMEBUFFER,target); glViewport(0,0,1920,1080);
  glClearColor(.055f,.065f,.08f,1); glClear(GL_COLOR_BUFFER_BIT); gfx_novo_quadro();
  nrec=0; stream_folha_desenhar(clockMs);
}
static void advance(unsigned ms) {
  for (unsigned i=0;i<ms;i+=16) { clockMs+=16; frame(); }
}
static void capture(const char *label) {
  unsigned char *pixels=malloc(1920*1080*4); assert(pixels);
  SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_RGBA32); assert(s);
  glFinish(); glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
  for(int y=0;y<1080;y++) memcpy((char *)s->pixels+y*s->pitch,pixels+(1079-y)*1920*4,1920*4);
  char path[1024]; snprintf(path,sizeof path,"%s/%s.png",output,label); assert(!IMG_SavePNG(s,path));
  free(pixels); SDL_FreeSurface(s);
}
// Opens the sheet on three copies of `release`; returns the wrapped line count.
static int fixture(int server,int description) {
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
  advance(960);
  const Stream *s=&lista[filtrado(foco)];
  nameW=FOLHA_W-FOLHA_PAD_E-FOLHA_PAD_D-2*FOLHA_TXT;
  assert(!strcmp(arquivoDa(s),release) && "release text must not truncate before rasterization");
  assert(!strcmp(arquivoVisivel(s,nameW),release) && "MP4 row never shows the description: name must draw");
  int lines=arquivoLayout(release,nameW)->n;
  frame(); assert(rowPieces(nameYOf(foco),release)==lines && nrec==lines);
  // The focused row grows by the wrapped lines (height and draw agree).
  assert(linhaAltura(foco)>=FOLHA_LINHA_H+lines*FOLHA_ARQ_H);
  return lines;
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
  int lines=fixture(0,0);
  assert(lines>1 && "long release must wrap, not ellipsize");
  // Settled focus: the name stays put (no marquee) and nothing re-rasterizes.
  float y0=rec[0].y, x0=rec[0].x; int raster=txt_rasterizadas;
  advance(1600);
  assert(rowPieces(nameYOf(foco),release)==lines && fabsf(rec[0].x-x0)<.1f && fabsf(rec[0].y-y0)<.1f);
  assert(txt_rasterizadas==raster && "no per-frame rasterization for the wrapped name");
  capture("01-wrapped");
  // Weak-TV sheet: settled frames keep the same draw count and fill.
  { int draws[2]; double fill[2], ms[2];
    for(int pass=0;pass<2;pass++) {
      Uint64 begin=SDL_GetPerformanceCounter();
      for(int i=0;i<120;i++) { clockMs+=16; frame(); }
      ms[pass]=(SDL_GetPerformanceCounter()-begin)*1000.0/SDL_GetPerformanceFrequency()/120;
      draws[pass]=gfx_n_rect; fill[pass]=gfx_fill_gpu;
    }
    printf("weak-TV sheet: %.4f / %.4f ms; GPU fill %.6f / %.6f; draws %d / %d\n",
           ms[0],ms[1],fill[0],fill[1],draws[0],draws[1]);
    assert(draws[0]==draws[1] && fabs(fill[0]-fill[1])<.0001 && txt_rasterizadas==raster); }
  // Spring active: the opening row draws every wrapped line, never an ellipsis;
  // the outgoing row keeps its full name while it closes.
  int old=foco; foco=1; frame();
  assert(rowPieces(nameYOf(old),release)==lines && "outgoing row must keep the full name while closing");
  int opened=0;
  for(int i=0;i<30;i++) { clockMs+=16; frame();
    int k=rowPieces(nameYOf(foco),release); assert(k==0 || k==lines); opened|=k==lines; }
  assert(opened && "opening row draws all lines during the spring");
  // Interrupted motion: back to the first row mid-spring, then settle.
  foco=old; clockMs+=16; frame(); advance(960);
  assert(rowPieces(nameYOf(foco),release)==lines && rowPieces(nameYOf(1),release)==0 && nrec==lines);
  capture("02-refocused");
  // UTF-8: accents, CJK and flags wrap on code points with nothing lost.
  snprintf(release,sizeof release,"Ação.Épica.東京物語.東京物語.東京物語.🇯🇵🇧🇷🇺🇸.2160p.WEB-DL.DDP5.1.Atmos.DV.HDR10.HEVC-GRUPO.Edição.Estendida.Versão.Final.mkv");
  lines=fixture(0,0);
  for(int j=0;j<nrec;j++) assert(((unsigned char)rec[j].t[0]&0xC0)!=0x80 && "piece starts mid code point");
  capture("03-utf8");
  // Both description fallback and local media servers share this renderer.
  snprintf(release,sizeof release,"A.Quiet.Journey.S02E05.1080p.WEB-DL.H264.DDP5.1.Dual.Audio.English.Portuguese.Subtitles.Complete.Uncut.Extended.Edition.Release.Group.Final.mkv"
           ".Original.Multilingual.Release.Name.With.More.Than.192.Bytes.Preserved.Until.The.End.mkv");
  fixture(0,1); fixture(1,0); capture("local-server");
  char variant[512]; snprintf(variant,sizeof variant,"%s",release); variant[strlen(variant)-5]='Z';
  { ESCALA_INI();
  TxtLinha first=txt_linha_bruta(TXT_PG_FIM,release,125,125,123,255);
  TxtLinha second=txt_linha_bruta(TXT_PG_FIM,variant,125,125,123,255);
  assert(first.tex && second.tex && first.tex!=second.tex && "long shared prefixes need distinct cached textures");
  ESCALA_FIM(); }
  // Focus loss closes the name; refocus brings the whole name back.
  grupo=0; advance(960); assert(nrec==0);
  grupo=1; advance(960); assert(rowPieces(nameYOf(foco),release)>1 && nrec==rowPieces(nameYOf(foco),release));
  // Live-guide sources use the same body with the channel scope enabled.
  stream_folha_canal(1); stream_filtro_tipo("tv"); stream_folha_abrir(); advance(960);
  assert(canalFolha); allPieces(release); capture("guide-sources");
  // The player island invokes the same body through its existing callback.
  CatItem item={0}; snprintf(item.tipo,sizeof item.tipo,"series"); cat_definir(&item,1);
  player_abrir(0,NULL); player_erro_fonte(); player_limpar_erro_fonte();
  stream_folha_canal(0); stream_folha_item(0); stream_folha_abrir();
  nrec=0; pending=0; corpoIlhaFolha((GfxRect){1920-FOLHA_W-24,24,FOLHA_W,1032},1,NULL);
  assert(folhaIlha()); allPieces(release);
  puts("PASS: focused release wraps in full (UTF-8), stable while settled, full lines through spring and interrupted focus; description, local server, guide and island paths");
  return 0;
}
