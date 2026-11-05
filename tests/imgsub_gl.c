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
  imgsub_limpar();imgsub_gl_limpar();puts("imgsub GL submission: ok");
}
