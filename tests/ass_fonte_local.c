/* The real current-track selector, with same-name/different-pixel faces.
 * No production font is rewritten: the test alone changes fixture metrics. */
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
  assert(argc==4);size_t n;unsigned char *a=readFont(argv[1],&n),*b=malloc(n);assert(b);memcpy(b,a,n);differentMetrics(b,n);
  char face[96],doc[2048];CHECK(legfontes_nome_sfnt(a,n,face,sizeof face));snprintf(doc,sizeof doc,header,face);
  size_t extraN; unsigned char *extra=readFont(argv[3],&extraN);char extraName[96];
  CHECK(legfontes_nome_sfnt(extra,extraN,extraName,sizeof extraName));
  attached=extra;attachedN=extraN;
  uint64_t base=oracle(a,n,doc),variant=oracle(b,n,doc);CHECK(base!=variant);
  size_t boldN; unsigned char *bold=readFont(argv[2],&boldN); char boldName[96],boldDoc[2048];
  CHECK(legfontes_nome_sfnt(bold,boldN,boldName,sizeof boldName));CHECK(strcmp(face,boldName));
  snprintf(boldDoc,sizeof boldDoc,header,boldName);uint64_t boldHash=oracle(bold,boldN,boldDoc);CHECK(boldHash!=base);
  char boldPath[]="/tmp/nuvio-font-bold-XXXXXX";int boldFd=mkstemp(boldPath);assert(boldFd>=0);
  FILE *boldFile=fdopen(boldFd,"wb");assert(boldFile);assert(fwrite(bold,1,boldN,boldFile)==boldN);assert(!fclose(boldFile));
  char path[]="/tmp/nuvio-font-ass-XXXXXX";int fd=mkstemp(path);assert(fd>=0);FILE *file=fdopen(fd,"wb");assert(file);assert(fwrite(b,1,n,file)==n);assert(!fclose(file));
  assrender_geracao(1);assrender_limpar_fontes();CHECK(assrender_adicionar_fonte("original.ttf",a,n));CHECK(assrender_adicionar_fonte("attached.ttf",extra,extraN));
  CHECK(assrender_carregar(doc,strlen(doc),1));ass_set_frame_size(assRenderer,640,360);
  CHECK(current()==base);CHECK(assNFontesAtuais==2&&assBytesAtuais==n+extraN);
  // The old add-by-name behavior demonstrably retains the first face.
  pthread_mutex_lock(&assTrava);ass_add_font(assLib,"stale-import.ttf",(char*)b,(int)n);ass_aplicar_fontes_locked();pthread_mutex_unlock(&assTrava);
  CHECK(current()==base);
  unsigned before=assEpoch;CHECK(!assrender_fonte_local_transacao(path,reject,NULL));CHECK(commits==1&&assEpoch==before);CHECK(current()==base);
  CHECK(assrender_fonte_local_transacao(path,accept,NULL));CHECK(commits==2);CHECK(assrender_fonte_local_ativa(path));CHECK(current()==variant);
  CHECK(assNFontesAtuais==2&&assBytesAtuais==n+extraN); /* exact current attachment replay */
  CHECK(assrender_fonte_local_transacao(boldPath,accept,NULL));
  PlainAssStyle style={0};snprintf(style.font,sizeof style.font,"%s",boldName);style.overrides=PLAIN_ASS_FONT;
  pthread_mutex_lock(&assTrava);ass_plain_style_locked(&style);pthread_mutex_unlock(&assTrava);
  CHECK(current()==boldHash);
  CHECK(assrender_fonte_local_transacao(path,accept,NULL));
  snprintf(style.font,sizeof style.font,"%s",face);
  pthread_mutex_lock(&assTrava);ass_plain_style_locked(&style);pthread_mutex_unlock(&assTrava);CHECK(current()==variant);
  CHECK(assrender_fonte_local_transacao(NULL,accept,NULL));CHECK(!assrender_fonte_local_ativa(path));
  style.overrides=0;pthread_mutex_lock(&assTrava);ass_plain_style_locked(&style);pthread_mutex_unlock(&assTrava);CHECK(current()==base);
  CHECK(assrender_fonte_local_transacao(path,accept,NULL));CHECK(current()==variant);
  // Attachment-cache rollover clears libass memory faces. The imported face
  // must be registered again before the new attachment is admitted.
  assBytesFontesVistas = ASS_FONTES_TETO_BYTES;
  CHECK(assrender_adicionar_fonte("rollover.ttf",extra,extraN));
  CHECK(assrender_fonte_local_ativa(path));CHECK(current()==variant);
  // Untagged Arabic requires the normal system fallback for the selected
  // Latin-only fixture. An authored inline font must still resolve attachments.
  char mixed[2400];size_t docN=strlen(doc);
  snprintf(mixed,sizeof mixed,"%.*s العربية\n",(int)(docN-1),doc);
  CHECK(assrender_atualizar(mixed,strlen(mixed),1));
  int resolutions=assResolucaoFonte;
  CHECK(current()==oracle(b,n,mixed));CHECK(assResolucaoFonte>resolutions);
  snprintf(mixed,sizeof mixed,"%.*s {\\fn%s}العربية\n",(int)(docN-1),doc,extraName);
  CHECK(assrender_atualizar(mixed,strlen(mixed),1));CHECK(current()==oracle(b,n,mixed));
  CHECK(assrender_fonte_local_transacao(NULL,accept,NULL));CHECK(current()==oracle(a,n,mixed));
  CHECK(assrender_fonte_local_transacao(path,accept,NULL));CHECK(current()==oracle(b,n,mixed));
  CHECK(assrender_atualizar(doc,strlen(doc),1));CHECK(current()==variant);
  /* Cache OOM keeps the actual renderer intact and denies the commit callback. */
  failSize=n;CHECK(assrender_adicionar_fonte("another.ttf",a,n));CHECK(!assAtuaisCompletas&&assBytesAtuais==0);
  int prior=commits;CHECK(!assrender_fonte_local_transacao(NULL,accept,NULL));CHECK(commits==prior);CHECK(current()==variant);
  assrender_limpar_fontes();CHECK(assAtuaisCompletas&&assNFontesAtuais==0&&assBytesAtuais==0);
  CHECK(assrender_adicionar_fonte("original.ttf",a,n));CHECK(assrender_carregar(doc,strlen(doc),1));CHECK(current()==variant);
  CHECK(assrender_fonte_local_transacao(NULL,accept,NULL));CHECK(current()==base);
  /* Count overflow is also incomplete; the ordinary font delivery still works. */
  for(int i=0;i<ASS_ATUAIS_MAX+1;i++){char name[32];snprintf(name,sizeof name,"font-%d.ttf",i);CHECK(assrender_adicionar_fonte(name,a,n));}
  CHECK(!assAtuaisCompletas);prior=commits;CHECK(!assrender_fonte_local_transacao(path,accept,NULL));CHECK(commits==prior&&current()==base);
  assrender_limpar_fontes();CHECK(assAtuaisCompletas&&assBytesAtuais==0);
  // Byte overflow is checked before copying; no partial replay becomes eligible.
  unsigned char *large=malloc(ASS_ATUAIS_BYTES+1u);assert(large);
  pthread_mutex_lock(&assTrava);ass_atual_guardar_locked("oversize.ttf",large,ASS_ATUAIS_BYTES+1u);pthread_mutex_unlock(&assTrava);
  free(large);CHECK(!assAtuaisCompletas&&assBytesAtuais==0);
  assrender_limpar_fontes();CHECK(assAtuaisCompletas&&assBytesAtuais==0);
  /* Run registry + renderer admission together: an incomplete current batch
   * must not commit related preferences, and callback failure restores the
   * exact persistent custom selection while retaining the selected pixels. */
  CHECK(!TTF_Init()); char root[]="/tmp/nuvio-font-transaction-XXXXXX";
  CHECK(mkdtemp(root)!=NULL);legfontes_dir(root);
  char stage[1024];snprintf(stage,sizeof stage,"%s/fontes-importadas/entrada-ab.tmp",root);
  FILE *staging=fopen(stage,"wb");assert(staging);assert(fwrite(b,1,n,staging)==n);assert(!fclose(staging));
  CHECK(legfontes_importar(0,"entrada-ab.tmp"));CHECK(legfontes_selecionada(0)==0);
  CHECK(assrender_adicionar_fonte("original.ttf",a,n));CHECK(assrender_carregar(doc,strlen(doc),1));
  CHECK(current()==variant);
  failSize=n;CHECK(assrender_adicionar_fonte("oom.ttf",a,n));prior=commits;
  CHECK(!legfontes_escolher_transacao(0,-1,accept,NULL));CHECK(commits==prior);
  CHECK(legfontes_selecionada(0)==0&&current()==variant);
  assrender_limpar_fontes();CHECK(assrender_adicionar_fonte("original.ttf",a,n));
  CHECK(assrender_carregar(doc,strlen(doc),1));CHECK(current()==variant);
  char preferences[1024];snprintf(preferences,sizeof preferences,"%s/fontes-legenda.txt",root);
  size_t savedN;unsigned char *saved=readFont(preferences,&savedN);
  CHECK(!legfontes_escolher_transacao(0,-1,reject,NULL));CHECK(current()==variant);
  size_t restoredN;unsigned char *restored=readFont(preferences,&restoredN);
  CHECK(savedN==restoredN&&!memcmp(saved,restored,savedN));free(saved);free(restored);
  CHECK(legfontes_selecionada(0)==0);
  CHECK(legfontes_escolher_transacao(0,-1,accept,NULL));CHECK(current()==base);
  CHECK(legfontes_remover(0));CHECK(!unlink(preferences));
  snprintf(stage,sizeof stage,"%s/fontes-importadas",root);CHECK(!rmdir(stage));CHECK(!rmdir(root));TTF_Quit();
  CHECK(!unlink(path));CHECK(!unlink(boldPath));free(a);free(b);free(bold);free(extra);
  printf("ass_fonte_local: %d checks passed; same-name pixels, restore, replay, OOM and bounds\n",checks);
  return 0;
}
