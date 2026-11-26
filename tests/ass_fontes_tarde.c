/* LATE ATTACHED FONTS (owner 18/11, log 10, Kaguya S3E02): the script was
 * drawn before the MKV's attached fonts arrived, so libass resolved its
 * families to the app's fallback and cached that face. When the fonts arrive,
 * the next document must be drawn with them. Real libass; fixture harness from
 * tests/ass_fonte_local.c. */
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <SDL2/SDL_ttf.h>
static size_t failSize;
static void *checkedMalloc(size_t n) {
  if (failSize && n == failSize) { failSize = 0; return NULL; }
  return malloc(n);
}
#define malloc checkedMalloc
#include "../src/assrender.c"
#undef malloc
#ifndef NV_ASS_LIBASS
#error This test requires the real libass backend
#endif

int perftv_enabled, perftv_uploads;
float gfx_tex_aspect_atual;
void gfx_tex_esquecer(GLuint t) { (void)t; abort(); }
int  gfx_grupo_comecar(GfxGrupo *g, GfxRect caixa, float alpha) { (void)g; (void)caixa; (void)alpha; return 0; }
void gfx_grupo_terminar(GfxGrupo *g, float alpha) { (void)g; (void)alpha; }
void gfx_rect(GfxRect r, GLuint t, GfxModo m, float f, float px, float py,
              float radius, float cr, float cg, float cb, float ca) {
  (void)r;(void)t;(void)m;(void)f;(void)px;(void)py;(void)radius;
  (void)cr;(void)cg;(void)cb;(void)ca;abort();
}
static int checks, commits;
static const unsigned char *attached; static size_t attachedN;
#define CHECK(x) do { assert(x); checks++; } while (0)
static int accept(void *p) { (void)p; CHECK(pthread_mutex_trylock(&assTrava) == EBUSY); commits++; return 1; }
static int reject(void *p) { (void)p; commits++; return 0; }
static uint16_t u16(const unsigned char *p) { return (uint16_t)((unsigned)p[0]<<8|p[1]); }
static uint32_t u32(const unsigned char *p) { return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3]; }
static void put16(unsigned char *p, unsigned n) { p[0]=n>>8;p[1]=n; }
static void put32(unsigned char *p, uint32_t n) { p[0]=n>>24;p[1]=n>>16;p[2]=n>>8;p[3]=n; }
static uint32_t sum(const unsigned char *p, size_t n) {
  uint32_t result=0;
  for(size_t i=0;i<n;i+=4) { unsigned char word[4]={0};size_t z=n-i<4?n-i:4;memcpy(word,p+i,z);result+=u32(word); }
  return result;
}
static unsigned char *table(unsigned char *font, size_t n, const char *tag, unsigned char **entry, size_t *size) {
  assert(legfontes_sfnt_valido(font,n));
  for(unsigned i=0;i<u16(font+4);i++) {
    unsigned char *e=font+12+i*16;
    if(!memcmp(e,tag,4)) { if(entry)*entry=e;if(size)*size=u32(e+12);return font+u32(e+8); }
  }
  return NULL;
}
static void differentMetrics(unsigned char *font, size_t n) {
  size_t hheaN, metricsN, headN; unsigned char *metricEntry,*headEntry;
  unsigned char *hhea=table(font,n,"hhea",NULL,&hheaN);
  unsigned char *metrics=table(font,n,"hmtx",&metricEntry,&metricsN);
  unsigned char *head=table(font,n,"head",&headEntry,&headN);
  assert(hhea&&hheaN>=36&&metrics&&head&&headN>=12);
  unsigned count=u16(hhea+34);assert(count&&count<=metricsN/4);
  for(unsigned i=0;i<count;i++){unsigned w=u16(metrics+i*4);if(w&&w<40000)put16(metrics+i*4,w+w/2);}
  put32(metricEntry+4,sum(metrics,metricsN));
  put32(head+8,0);put32(headEntry+4,sum(head,headN));
  put32(head+8,UINT32_C(0xb1b0afba)-sum(font,n));
}
static unsigned char *readFont(const char *path, size_t *n) {
  FILE *f=fopen(path,"rb");assert(f);assert(!fseek(f,0,SEEK_END));long z=ftell(f);assert(z>0);
  rewind(f);unsigned char *b=malloc((size_t)z);assert(b);assert(fread(b,1,(size_t)z,f)==(size_t)z);fclose(f);*n=(size_t)z;return b;
}
static uint64_t pixels(ASS_Image *im) {
  uint64_t hash=UINT64_C(14695981039346656037);assert(im);
  for(;im;im=im->next){
    int geometry[]={im->w,im->h,im->dst_x,im->dst_y,(int)im->color};
    const unsigned char *g=(const unsigned char*)geometry;
    for(size_t i=0;i<sizeof geometry;i++)hash=(hash^g[i])*UINT64_C(1099511628211);
    for(int y=0;y<im->h;y++)for(int x=0;x<im->w;x++)hash=(hash^im->bitmap[y*im->stride+x])*UINT64_C(1099511628211);
  }
  return hash;
}
static uint64_t current(void) {
  pthread_mutex_lock(&assTrava);int changed;
  uint64_t h=pixels(ass_render_frame(assRenderer,assTrack,1000,&changed));
  pthread_mutex_unlock(&assTrava);return h;
}
static uint64_t oracle(const unsigned char *b,size_t n,const char *doc) {
  ASS_Library *lib=ass_library_init();assert(lib);
  ass_add_font(lib,"oracle.ttf",(const char*)b,(int)n);
  if (attached) ass_add_font(lib,"attached.ttf",(const char*)attached,(int)attachedN);
  ASS_Renderer *renderer=ass_renderer_init(lib);assert(renderer);
  ass_set_fonts(renderer,NULL,"Arial",ASS_FONTPROVIDER_AUTODETECT,NULL,1);
  ass_set_frame_size(renderer,640,360);
  ass_set_storage_size(renderer,1920,1080); ass_set_hinting(renderer,ASS_HINTING_LIGHT);
  ASS_Track *track=ass_read_memory(lib,(char*)doc,strlen(doc),"UTF-8");assert(track);int changed;
  uint64_t h=pixels(ass_render_frame(renderer,track,1000,&changed));
  ass_free_track(track);ass_renderer_done(renderer);ass_library_done(lib);return h;
}
static const char *header=
  "[Script Info]\nScriptType: v4.00+\nPlayResX: 640\nPlayResY: 360\n"
  "[V4+ Styles]\nFormat: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n"
  "Style: Default,%s,28,&H00FFFFFF,&H000000FF,&H00000000,&H64000000,0,0,0,0,100,100,0,0,1,2,0,2,20,20,20,1\n"
  "[Events]\nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n"
  "Dialogue: 0,0:00:00.00,0:00:09.00,Default,,0,0,0,,iiiiWWWW0123\n";
int main(int argc,char **argv) {
  assert(argc==4);
  size_t n, boldN; unsigned char *a=readFont(argv[1],&n), *bold=readFont(argv[2],&boldN);
  char face[96], boldName[96], doc[2048];
  CHECK(legfontes_nome_sfnt(a,n,face,sizeof face));
  CHECK(legfontes_nome_sfnt(bold,boldN,boldName,sizeof boldName));CHECK(strcmp(face,boldName));
  snprintf(doc,sizeof doc,header,boldName);
  uint64_t comFonte=oracle(bold,boldN,doc);   // what the script looks like with its font
  // 1) The document arrives first: only the app-side font, the attached one not yet.
  assrender_geracao(1);assrender_limpar_fontes();
  CHECK(assrender_adicionar_fonte("app.ttf",a,n));
  CHECK(assrender_carregar(doc,strlen(doc),1));ass_set_frame_size(assRenderer,640,360);
  uint64_t semFonte=current();
  CHECK(semFonte!=comFonte);
  CHECK(__atomic_load_n(&assFallbacks,__ATOMIC_RELAXED)>0);
  // 2) The attached font arrives; the next delivery carries the same script.
  CHECK(assrender_adicionar_fonte("attached-late.ttf",bold,boldN));
  CHECK(assrender_carregar(doc,strlen(doc),1));ass_set_frame_size(assRenderer,640,360);
  CHECK(current()==comFonte);
  CHECK(__atomic_load_n(&assFallbacks,__ATOMIC_RELAXED)==0&&!assFonteNovaTarde);
  // 3) No fallback since: a later font does not rebuild the selector again.
  CHECK(assrender_adicionar_fonte("another.ttf",a,n));
  CHECK(assrender_carregar(doc,strlen(doc),1));ass_set_frame_size(assRenderer,640,360);
  CHECK(current()==comFonte&&assFonteNovaTarde);
  printf("ass_fontes_tarde: ok (%d checks)\n",checks);
  return 0;
}
