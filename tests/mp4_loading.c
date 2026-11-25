/* Execute the production text-slot renderer. No native player or GL context. */
#include "video.h"
#include "dvtela.h"
#include "text.h"
#include "gfx.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static VideoDvFase phase;
static DvtelaEstado state;
static float alpha;
static int calls;
static char line[160];
static struct { float luz; } molas; /* Animation state supplied by the renderer. */
static int glow, rules;
static float draw_x,draw_y,draw_a;
void video_dv_fase(VideoDvFase *out) { *out=phase; }
const DvtelaEstado *dvtela_estado(void) { return &state; }
float dvtela_alfa(void) { return alpha; }
const char *i18n(const char *s) { return s; }
TxtLinha txt_linha(TxtEstilo style,const char *s,int r,int g,int b,int a) {
  (void)style;(void)r;(void)g;(void)b;(void)a;
  snprintf(line,sizeof line,"%s",s);calls++;return (TxtLinha){.w=100,.h=24};
}
void txt_desenhar_alpha(TxtLinha l,float x,float y,float a) {
  (void)l;assert(a>0);draw_x=x;draw_y=y;draw_a=a;
}
void ajustes_acento(float *r,float *g,float *b) { *r=1;*g=.8f;*b=.2f; }
void gfx_rect(GfxRect r,GLuint tex,GfxModo modo,float foco,float px,float py,float raio,
              float cr,float cg,float cb,float ca) {
  (void)r;(void)tex;(void)foco;(void)px;(void)py;(void)raio;(void)cr;(void)cg;(void)cb;
  assert(modo==GFX_SOMBRA && ca>0);glow++;
}
void gfx_cor(GfxRect r,float raio,float cr,float cg,float cb,float ca) {
  (void)raio;(void)cr;(void)cg;(void)cb;assert(r.w==56 && r.h==1 && ca>0);rules++;
}
#define ZONA(z,r) ((void)0)
#include "mp4-loading-render.c"
static void expect(const char *text,int corner) {
  calls=0;line[0]=0;float height=dvtela_abertura_linha(corner,960,800,1);
  if(text) {assert(calls==1 && !strcmp(line,text));assert(height==34);}
  else assert(calls==0 && height==0);
}
static void mark(int visible,int lit,int corner) {
  calls=glow=rules=0;line[0]=0;
  dvtela_abertura_marca(corner,960,400,1);
  if(!visible) { assert(calls==0 && glow==0 && rules==0);return; }
  assert(calls==1 && !strcmp(line,"DOLBY VISION"));
  assert(draw_x==(corner?960:910) && draw_y==348);
  assert(draw_a>.6f && draw_a<=1 && glow==lit && rules==(corner?1:2));
  if(lit)assert(draw_a==1);
}
int main(void) {
  for(int corner=0;corner<=1;corner++) {
    phase=(VideoDvFase){0};state=(DvtelaEstado){0};alpha=0;
    expect(NULL,corner); // Healthy native MP4 and ordinary SDR opening.
    mark(0,0,corner);
    phase.mp4Inicial=1;expect("Lendo o arquivo",corner);
    mark(1,0,corner); // Same subdued mark while source metadata is unknown.
    phase.caminho=1;expect("Abrindo a fonte",corner);
    phase.fonteAberta=1;mark(1,1,corner); // Verified source uses the existing accent glow.
    phase.carregado=1;expect("Preparando a imagem",corner);
    phase.mp4Inicial=0;expect(NULL,corner); // First picture, failure, seek/audio reload or new native source.
    mark(0,0,corner);
    alpha=1;state.ativa=1;state.passo=DVT_PASSO_IMAGEM;
    expect("Preparando a imagem",corner); // Existing MKV screen unchanged.
    state.passo=DVT_PASSO_ABRIR;expect("Abrindo a fonte",corner);
    molas.luz=1;mark(1,1,corner);molas.luz=0;
  }
  puts("PASS MP4 startup text and Dolby Vision mark: both layouts, exact existing placement/rules/glow, verified-source accent, no native-source or post-startup display, existing MKV stages retained");
  return 0;
}
