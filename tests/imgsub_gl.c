// Exercise the real overlay submission code with a recording graphics backend.
// This verifies draw calls/placement/clears, not a physical TV or GPU output.
#include "imgsub.h"
#include "gfx.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#undef glTexImage2D
static unsigned next_texture;
static int draws, uploads, enabled=1;
static GfxRect last;
int perftv_enabled, perftv_uploads;
int legenda_ligada_em(unsigned owner) { return enabled && owner==1; }
void glDeleteTextures(GLsizei n,const GLuint *p) { (void)n;(void)p; }
void glGenTextures(GLsizei n,GLuint *p) { while(n--) *p++=++next_texture; }
void glBindTexture(GLenum t,GLuint v) { (void)t;(void)v; }
void glTexParameteri(GLenum t,GLenum p,GLint v) { (void)t;(void)p;(void)v; }
void glTexImage2D(GLenum t,GLint l,GLint in,GLsizei w,GLsizei h,GLint b,GLenum f,GLenum ty,const GLvoid *p) {
  (void)t;(void)l;(void)in;(void)b;(void)f;(void)ty;
  assert(w==6 && h==2 && p); uploads++;
}
void gfx_tex_esquecer(GLuint t) { (void)t; }
void gfx_rect(GfxRect r,GLuint tex,GfxModo mode,float focus,float px,float py,float radius,float cr,float cg,float cb,float ca) {
  (void)focus;(void)px;(void)py;(void)radius;(void)cr;(void)cg;(void)cb;
  assert(tex && mode==GFX_TEXTO && ca==1); last=r; draws++;
}
static void quadro(unsigned dono,double ini,double fim,int w,int h) {
  PgsQuadro p={0}; p.tela_w=1920;p.tela_h=1080;p.n=1;
  p.obj[0].x=100;p.obj[0].y=900;p.obj[0].w=w;p.obj[0].h=h;
  p.obj[0].rgba=calloc(48,1); imgsub_por(dono,ini,fim,&p);
}
int main(void) {
  PgsQuadro p={0}; p.tela_w=1920;p.tela_h=1080;p.n=1;
  p.obj[0].x=100;p.obj[0].y=900;p.obj[0].w=6;p.obj[0].h=2;
  p.obj[0].rgba=calloc(48,1); imgsub_por(1,10,15,&p);
  ImgsubQuadro *f=imgsub_pegar(10); imgsub_soltar(f);
  ImgsubDiagnostico d;imgsub_diagnostico(&d);assert(d.selected==1 && !d.drawn);
  assert(imgsub_desenhar(10,20,30,960,540,1));
  assert(last.x==70 && last.y==480 && last.w==3 && last.h==1);
  assert(imgsub_desenhar(11,20,30,960,540,1));
  imgsub_diagnostico(&d);assert(d.drawn==1 && draws==2 && uploads==1);
  p.n=0;imgsub_por(1,12,-1,&p);
  assert(!imgsub_desenhar(12,20,30,960,540,1));
  assert(!imgsub_desenhar(13,20,30,960,540,1));
  imgsub_diagnostico(&d);assert(d.cleared==1 && d.drawn==1);
  assert(imgsub_desenhar(10.5,20,30,960,540,1)); // seek back
  enabled=0;assert(!imgsub_desenhar(10.6,20,30,960,540,1));
  imgsub_diagnostico(&d);assert(d.drawn==2 && d.cleared==2 && d.max_draw_lateness==0.5);

  // Empty queue is no exception to the 48 MiB cap: a single frame heavier
  // than the cap is rejected, not stored alone.
  imgsub_limpar(); enabled=1;
  quadro(1,10,15,5000,3000);              // 5000x3000x4 = 60 MB > 48 MiB
  imgsub_diagnostico(&d);
  assert(d.rejected==1 && !d.queued && !d.frames && !d.bytes);
  assert(!imgsub_pegar(10) && !imgsub_desenhar(10,20,30,960,540,1));

  // Eviction is by distance from the playhead and counted: ~2.4 MB frames
  // overflow the cap; the frame farther than all queued ones is rejected,
  // and a frame near the playhead evicts the farthest.
  int i;
  for (i=0;i<21;i++) quadro(1,i*4.0,i*4.0+2.0,2048,300);
  imgsub_diagnostico(&d);
  assert(d.queued==20 && d.rejected==2 && d.frames==20);  // ini=80 was the farthest
  assert(d.bytes<=(48u<<20));
  f=imgsub_pegar(81); assert(!f);                        // playhead near the tail
  quadro(1,84,86,2048,300);                              // closer than ini=0: evicts it
  imgsub_diagnostico(&d);
  assert(d.queued==21 && d.evicted==1 && d.frames==20);
  assert(!imgsub_pegar(1));                              // ini=0 was the farthest
  f=imgsub_pegar(84.5); assert(f && f->ini==84); imgsub_soltar(f);

  // A newer owner expels the whole queue at once; limpar() resets the counters.
  quadro(2,90,92,6,2);
  imgsub_diagnostico(&d);
  assert(d.owner==2 && d.frames==1 && d.queued==22);
  f=imgsub_pegar(90.5); assert(f && f->ini==90); imgsub_soltar(f);
  imgsub_limpar();
  imgsub_diagnostico(&d); assert(!d.queued && !d.frames && !d.bytes && !d.selected);
  imgsub_gl_limpar();puts("imgsub GL submission: ok");
}
