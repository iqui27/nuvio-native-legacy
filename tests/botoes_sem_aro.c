// Secondary solid pills honor the owning sheet's borderless scope.
#include "../src/botoes.c"
#include <stdio.h>

int gfx_vidro_sem_aro;
static int rings, fills;
int ajustes_vidro(void) { return 0; }
float ajustes_acento_tinta(float *r,float *g,float *b) { *r=*g=*b=1; return 0; }
void gfx_anel(GfxRect r,float radius,float width,float cr,float cg,float cb,float a) { rings++; }
void gfx_cor(GfxRect r,float radius,float cr,float cg,float cb,float a) { fills++; }
void gfx_rect(GfxRect r,GLuint tex,GfxModo mode,float focus,float px,float py,float radius,float cr,float cg,float cb,float a) { }
void gfx_vidro_painel(GfxRect r,float radius,float background,float a) { }
void gfx_vidro_painel_acento(GfxRect r,float radius,float background,float a) { }
void gfx_vidro_pilula_cheia(GfxRect r,float radius,float focus,float a) { }
int gfx_vidro_tinta(float focus) { return 235; }
void gfx_icone(GfxRect r,const char *name,float cr,float cg,float cb,float a) { }
TxtLinha txt_linha(TxtEstilo style,const char *s,int r,int g,int b,int a) { return (TxtLinha){0}; }
void txt_desenhar_alpha(TxtLinha line,float x,float y,float a) { }

int main(void) {
  int failed=0;
  const float scales[]={1,.85f}, focus[]={0,.5f,1};
  for(int scale=0;scale<2;scale++) for(int f=0;f<3;f++) {
    rings=fills=0; gfx_vidro_sem_aro=1;
    botao_pilula((GfxRect){0,0,300*scales[scale],56*scales[scale]},"Action",NULL,focus[f],0,0,1);
    if(rings || fills!=1) {
      printf("FAIL: borderless solid pill at %.0f%% focus %.1f: rings=%d fills=%d\n",scales[scale]*100,focus[f],rings,fills);
      failed=1;
    }
  }
  rings=0; gfx_vidro_sem_aro=0;
  botao_pilula((GfxRect){0,0,300,56},"Action",NULL,0,0,0,1);
  if(rings!=1) { puts("FAIL: unscoped legacy pill changed"); failed=1; }
  if(!failed) puts("PASS: solid secondary pills honor borderless scope at 100% and 85%, rest through focus");
  return failed;
}
