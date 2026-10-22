#include "plainass.h"
#include "assrender.h"
#include "gfx.h"
#include <ass/ass.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); exit(1); } } while (0)
char *rede_baixar_bin(const char *url, int seconds, long *n) { (void)url; (void)seconds; *n=0; return NULL; }
void teste_glGenTextures(GLsizei n, GLuint *t) { static GLuint id=1; while(n--) *t++=id++; }
void teste_glDeleteTextures(GLsizei n, const GLuint *t) { (void)n; (void)t; }
void teste_glBindTexture(GLenum a, GLuint t) { (void)a; (void)t; }
void teste_glTexImage2D(GLenum a, GLint b, GLint c, GLsizei w, GLsizei h, GLint d, GLenum e, GLenum f, const void *p) {
  (void)a;(void)b;(void)c;(void)w;(void)h;(void)d;(void)e;(void)f;(void)p;
}
void teste_glTexParameteri(GLenum a, GLenum b, GLint c) { (void)a;(void)b;(void)c; }
float gfx_tex_aspect_atual;
void gfx_tex_esquecer(GLuint t) { (void)t; }
static float yMin, xMin, xMax; static int drawn;
void gfx_rect(GfxRect r, GLuint t, GfxModo m, float f, float radius, float b, float ba, float cr, float cg, float cb, float a) {
  (void)t;(void)m;(void)f;(void)radius;(void)b;(void)ba;(void)cr;(void)cg;(void)cb;(void)a;
  if (!drawn++ || r.y<yMin) yMin=r.y;
  if (drawn==1 || r.x<xMin) xMin=r.x;
  if (drawn==1 || r.x+r.w>xMax) xMax=r.x+r.w;
}
static int frame_at(double time) {
  int i, n=0;
  for(i=0;i<35;i++) { drawn=0; n=assrender_desenhar(time,0,1,0,0,1920,1080); usleep(10000); }
  return n;
}
static int frame(void) { return frame_at(1.5); }
static int bitmapWidth;
static uint64_t bitmap_hash(ASS_Renderer *r, ASS_Track *t) {
  int changed=0; ASS_Image *im=ass_render_frame(r,t,1500,&changed);
  unsigned char *pixels=calloc(1920u*1080u,1); uint64_t hash=1469598103934665603ULL;
  CHECK(im && pixels);
  for(;im;im=im->next) {
    int x,y;
    for(y=0;y<im->h;y++) for(x=0;x<im->w;x++) {
      int px=im->dst_x+x, py=im->dst_y+y;
      unsigned alpha=im->bitmap[y*im->stride+x]*(255u-(im->color&255u))/255u;
      if(px>=0 && px<1920 && py>=0 && py<1080) {
        size_t at=(size_t)py*1920u+(unsigned)px;
        pixels[at]=(unsigned char)(alpha+pixels[at]*(255u-alpha)/255u);
      }
    }
  }
  { int lo=1920,hi=-1;
    for(size_t i=0;i<1920u*1080u;i++) {
      hash=(hash^pixels[i])*1099511628211ULL;
      if(pixels[i]) { int x=i%1920; if(x<lo)lo=x;if(x>hi)hi=x; }
    }
    bitmapWidth=hi-lo+1;
  }
  free(pixels);return hash;
}
static char *doc(const char *s) {
  LegendaCue c={0}; c.inicio=1; c.fim=2;
  snprintf(c.texto,sizeof c.texto,"%s",s);
  return plainass_document(&c,1);
}
static void add_face(const char *name, const void *bytes, size_t n, void *u) {
  ass_add_font((ASS_Library *)u,name,(const char *)bytes,(int)n);
}
static int ink_height(ASS_Image *images, unsigned color) {
  int lo=1080,hi=-1;
  for(ASS_Image *im=images;im;im=im->next) {
    if(im->color!=color)continue;
    for(int y=0;y<im->h;y++) for(int x=0;x<im->w;x++)
      if(im->bitmap[y*im->stride+x]>=128) {
        int py=im->dst_y+y;if(py<lo)lo=py;if(py>hi)hi=py;
      }
  }
  return hi-lo+1;
}
int main(void) {
  char *a, *b; ASS_Library *lib; ASS_Renderer *renderer; ASS_Track *ta,*tb;
  PlainAssStyle style={0}; FILE *font; char *bytes; long size; float oldY, oldW;
  int joinedWidth; uint64_t joinedHash;
  const char *srt="1\n00:00:01,000 --> 00:00:02,000\nمرحبا John (123) ١٢٣!\n\n";
  CHECK(!plainass_has_arabic("Hello 123")); CHECK(plainass_has_arabic("مرحبا"));
  CHECK(!plainass_has_arabic("שלום")); CHECK(!plainass_has_arabic("\xe2\x80\x8f"));
  CHECK(!doc("English only"));
  a=doc("مرحبا {\\pos(0,0)} \\N \\h\nJohn (123) ١٢٣!"); CHECK(a);
  CHECK(strstr(a,"\\{\\\xef\xbb\xbfpos(0,0)\\}"));
  CHECK(strstr(a,"\\\xef\xbb\xbfN")); CHECK(strstr(a,"\\NJohn"));
  free(a);

  /* Arabic joining must produce a tighter ligature than explicitly
   * disconnected letters. Preserve join controls in the generated text. */
  lib=ass_library_init(); CHECK(lib); renderer=ass_renderer_init(lib); CHECK(renderer);
  font=fopen("deploy/app/fonts/NotoNaskhArabic-Regular.ttf","rb"); CHECK(font);
  fseek(font,0,SEEK_END);size=ftell(font);rewind(font);bytes=malloc(size);CHECK(bytes);
  CHECK(fread(bytes,1,size,font)==(size_t)size);fclose(font);
  ass_add_font(lib,"NotoNaskhArabic-Regular.ttf",bytes,size);free(bytes);
  assrender_ler_pasta_fontes("deploy/app/fonts",add_face,lib,NULL);
  ass_set_fonts(renderer,NULL,"Noto Naskh Arabic",ASS_FONTPROVIDER_NONE,NULL,1);
  ass_set_shaper(renderer,ASS_SHAPING_COMPLEX);
  ass_set_frame_size(renderer,1920,1080);ass_set_storage_size(renderer,1920,1080);
  a=doc("لا"); b=doc("ل‍ا"); CHECK(a&&b);
  ta=ass_read_memory(lib,a,strlen(a),"UTF-8");tb=ass_read_memory(lib,b,strlen(b),"UTF-8");CHECK(ta&&tb);
  joinedHash=bitmap_hash(renderer,ta);joinedWidth=bitmapWidth;
  CHECK(strstr(b,"‍"));CHECK(bitmap_hash(renderer,tb));
  ass_free_track(tb);free(b);b=doc("ل‌ا");CHECK(b);
  tb=ass_read_memory(lib,b,strlen(b),"UTF-8");CHECK(tb);
  CHECK(joinedHash!=bitmap_hash(renderer,tb));
  printf("lam-alef width joined %d disconnected %d\n",joinedWidth,bitmapWidth);
  CHECK(joinedWidth<bitmapWidth);
  ass_free_track(ta);ass_free_track(tb);free(a);free(b);
  /* A colored Latin run must sit to the LEFT of the preceding Arabic
   * word. This fails with ASS Encoding 1 / per-style-run LTR layout, even
   * though the individual Arabic words look joined. */
  a=doc("مرحبا John"); CHECK(a);
  {
    char *latin=strstr(a,"John"); int changed=0, nr=0,nw=0;
    long red=0,white=0; CHECK(latin);
    b=malloc(strlen(a)+80);CHECK(b);
    snprintf(b,strlen(a)+80,"%.*s{\\c&H0000FF&}John{\\c&HFFFFFF&}%s",
             (int)(latin-a),a,latin+4);
    ta=ass_read_memory(lib,b,strlen(b),"UTF-8");CHECK(ta);
    CHECK(ta->styles[ta->default_style].Encoding==-1);
    CHECK(!ass_track_set_feature(ta,ASS_FEATURE_WHOLE_TEXT_LAYOUT,1));
    CHECK(!ass_track_set_feature(ta,ASS_FEATURE_BIDI_BRACKETS,1));
    for(ASS_Image *im=ass_render_frame(renderer,ta,1500,&changed);im;im=im->next) {
      if(im->color==0xff000000u) { red+=im->dst_x+im->w/2;nr++; }
      if(im->color==0xffffff00u) { white+=im->dst_x+im->w/2;nw++; }
    }
    CHECK(nr&&nw);CHECK(red/(double)nr<white/(double)nw);
    ass_free_track(ta);free(a);free(b);
  }
  /* TV feedback: an LRM before a speaker dash makes it appear at the
   * left. Assert its visual position on BOTH lines, including mixed text. */
  {
    const char *cases[]={"- مرحبا", "\xe2\x80\x8e- مرحبا",
      "\xe2\x80\x8e- لتعنفك وما هنالك\n\xe2\x80\x8e- تبا يا رجل",
      "\xe2\x80\x8e- 5 دولارات John (123)\n- نعم"};
    for(int k=0;k<4;k++) {
      a=doc(cases[k]);CHECK(a);char *body=strstr(a,"Dialogue:");CHECK(body);
      b=malloc(strlen(a)*3+100);CHECK(b);size_t n=(size_t)(body-a);
      memcpy(b,a,n);
      while(*body) {
        if(*body=='-') {
          const char tag[]="{\\c&H0000FF&}-{\\c&HFFFFFF&}";
          memcpy(b+n,tag,sizeof tag-1);n+=sizeof tag-1;
        } else b[n++]=*body;
        body++;
      }
      b[n]=0;ta=ass_read_memory(lib,b,n,"UTF-8");CHECK(ta);
      ass_track_set_feature(ta,ASS_FEATURE_WHOLE_TEXT_LAYOUT,1);
      ass_track_set_feature(ta,ASS_FEATURE_BIDI_BRACKETS,1);
      int changed=0,markers=0;
      ASS_Image *images=ass_render_frame(renderer,ta,1500,&changed);CHECK(images);
      for(ASS_Image *dash=images;dash;dash=dash->next) {
        if(dash->color!=0xff000000u)continue;
        double dx=dash->dst_x+dash->w/2.,dy=dash->dst_y+dash->h/2.;int sameLine=0;
        for(ASS_Image *word=images;word;word=word->next) {
          double wy=word->dst_y+word->h/2.;
          if(word->color!=0xffffff00u || wy<dy-30 || wy>dy+30)continue;
          CHECK(dx>word->dst_x+word->w);sameLine++;
        }
        CHECK(sameLine);markers++;
      }
      CHECK(markers==(k<2?1:2));
      ass_free_track(ta);free(a);free(b);
    }
    a=doc("مرحبا -\n-5 درجات\n- John مرحبا\n-١٠ درجات");CHECK(a);
    CHECK(!strstr(a,"\xe2\x80\x8f"));free(a);
    a=doc("\xe2\x80\x8e– مرحبا");CHECK(a);
    CHECK(strstr(a,"\xe2\x80\x8f–"));free(a);
    puts("PASS Arabic dialogue markers on the right; numeric minus and English-led lines unchanged");
  }
  /* TV feedback: the Arabic face looked tiny beside Inter digits. Measure
   * visible ink at normal and maximum slider sizes, in the same event.
   * The chosen Arabic glyph is alef (roughly cap-height), not a descender. */
  a=doc("ا5Hا"); CHECK(a);
  {
    char *latin=strstr(a,"5H");CHECK(latin);b=malloc(strlen(a)+100);CHECK(b);
    snprintf(b,strlen(a)+100,"%.*s{\\c&H0000FF&}5{\\c&H00FF00&}H{\\c&HFFFFFF&}%s",
             (int)(latin-a),a,latin+2);
    ta=ass_read_memory(lib,b,strlen(b),"UTF-8");CHECK(ta);
    ass_track_set_feature(ta,ASS_FEATURE_WHOLE_TEXT_LAYOUT,1);
    for(int fs=40;fs<=80;fs+=40) {
      int changed=0,ah,dh,lh;ASS_Image *images;
      ta->styles[ta->default_style].FontSize=fs;
      images=ass_render_frame(renderer,ta,1500,&changed);CHECK(images);
      ah=ink_height(images,0xffffff00u);dh=ink_height(images,0xff000000u);lh=ink_height(images,0x00ff0000u);
      printf("size %d: Arabic alef %dpx, Latin digit %dpx, Latin cap %dpx\n",fs,ah,dh,lh);
      CHECK(ah>0&&dh>0&&lh>0);CHECK(ah>=dh*.85&&ah<=dh*1.15);
      CHECK(ah>=lh*.85&&ah<=lh*1.15);
    }
    ass_free_track(ta);free(a);free(b);
  }
  ass_renderer_done(renderer);ass_library_done(lib);
  puts("PASS Arabic joining, Unicode controls, mixed RTL order and balanced glyph sizes");

  /* Production parser, routing, worker, paused style updates and isolation. */
  legenda_definir_corpo(srt); CHECK(assrender_ativo()); CHECK(assrender_texto_simples());
  snprintf(style.font,sizeof style.font,"Inter Display");style.size=48;
  style.rgb=0xffffff;style.border=1;style.marginV=80;
  assrender_definir_texto_estilo(&style);
  assrender_definir_layout(0,0,1920,1080,1920,1080,1);
  CHECK(frame()>0);oldY=yMin;oldW=xMax-xMin;
  /* Same paused cue, same font/size: show controls, hide them, then change
   * subtitle position again. Height changes must not mask stale collisions. */
  for(int repeat=0;repeat<2;repeat++) {
    style.marginV=320;assrender_definir_texto_estilo(&style);
    CHECK(frame()>0);CHECK(yMin>oldY-242 && yMin<oldY-238);
    style.marginV=80;assrender_definir_texto_estilo(&style);
    CHECK(frame()>0);CHECK(yMin>oldY-2 && yMin<oldY+2);
    CHECK(xMax-xMin>oldW-2 && xMax-xMin<oldW+2);
  }
  puts("PASS position-only controls open/close on the same paused cue");
  style.marginV=320;style.size=64;style.rgb=0xffff00;style.background=4;
  style.bold=1;style.border=2;assrender_definir_texto_estilo(&style);
  CHECK(frame()>0); CHECK(yMin<oldY-150);CHECK(xMax-xMin>oldW);
  puts("PASS paused style/position changes redraw through production worker");
  {
    char overlap[4096]={0};size_t used=0;float afterOverlap;
    for(int i=0;i<5;i++)
      used+=(size_t)snprintf(overlap+used,sizeof overlap-used,
        "%d\n00:00:01,000 --> 00:00:02,000\nمرحبا\n\n",i+1);
    snprintf(overlap+used,sizeof overlap-used,
      "6\n00:00:01,500 --> 00:00:04,000\nأو احتيال أو كذب\nأو الإساءة إلى بعضهم البعض\n\n");
    style.size=48;style.marginV=80;style.rgb=0xffffff;
    style.background=0;style.bold=0;style.border=1;
    assrender_definir_texto_estilo(&style);
    legenda_definir_corpo(overlap);
    CHECK(frame_at(1.6)>0);CHECK(yMin<700);
    CHECK(frame_at(2.3)>0);afterOverlap=yMin;
    CHECK(afterOverlap>800);
    /* Seek back into the overlap, then out again, with no UI changes. */
    CHECK(frame_at(1.6)>0);CHECK(yMin<700);
    CHECK(frame_at(2.3)>0);CHECK(yMin>afterOverlap-2 && yMin<afterOverlap+2);
    legenda_definir_corpo("1\n00:00:01,500 --> 00:00:04,000\nأو احتيال أو كذب\nأو الإساءة إلى بعضهم البعض\n\n");
    CHECK(frame_at(2.3)>0);CHECK(yMin>afterOverlap-2 && yMin<afterOverlap+2);
    puts("PASS overlap ending and seeking restore the same bottom as a fresh lone cue");
  }
  legenda_definir_corpo("1\n00:00:01,000 --> 00:00:02,000\nHello English\n\n");
  CHECK(!assrender_ativo());CHECK(!assrender_texto_simples());
  a=doc("مرحبا John (123) ١٢٣!");CHECK(a);assrender_geracao(100);
  CHECK(!assrender_carregar_texto(a,strlen(a),99));
  CHECK(assrender_carregar(a,strlen(a),100));CHECK(!assrender_texto_simples());
  CHECK(frame()>0);CHECK(yMin>oldY-2 && yMin<oldY+2);
  free(a);legenda_desligar();CHECK(!assrender_ativo());
  puts("PASS LTR fallback, authored ASS isolation, stale generation and off");
  return 0;
}
