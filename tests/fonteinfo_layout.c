// Exercise the actual title row at narrow widths and large interface fonts.
#include "../src/detail.c"
#include <assert.h>
static int drawn, textHeight;
static float limit, bottom;
static void record(float x,float y,float w,float h) {
  assert(x>=0 && x+w<=limit+0.01f);assert(h>0);
  if(y+h>bottom)bottom=y+h;
  drawn++;
}
float badges_largura_tom(uint64_t mask,float h) {(void)mask;(void)h;return 72;}
float badges_desenhar_tom(uint64_t m,float x,float y,float w,float h,float r,float g,float b,float a) {
  (void)m;(void)w;(void)r;(void)g;(void)b;(void)a;record(x,y,72,h);return 72;
}
TxtLinha txt_linha(TxtEstilo e,const char *s,int r,int g,int b,int a) {
  (void)e;(void)r;(void)g;(void)b;(void)a;return (TxtLinha){.w=(int)strlen(s)*15,.h=textHeight};
}
TxtLinha txt_linha_corta(TxtEstilo e,const char *s,int r,int g,int b,int a,float maxW) {
  TxtLinha l=txt_linha(e,s,r,g,b,a);if(l.w>maxW)l.w=(int)maxW;return l;
}
void txt_desenhar_alpha(TxtLinha l,float x,float y,float a) {(void)a;record(x,y,l.w,l.h);}
void plrui_decimal(char *s) {assert(strstr(s," GB") || strstr(s," MB"));}
int main(void) {
  FonteInfo f={.selos=31,.nTextos=3};
  snprintf(f.texto[0],sizeof f.texto[0],"12.9 GB");
  snprintf(f.texto[1],sizeof f.texto[1],"ReleaseGroup.2.0");
  snprintf(f.texto[2],sizeof f.texto[2],"2.0");
  for(int width=180;width<=1080;width+=180)for(int h=24;h<=60;h+=12) {
    limit=(float)width;textHeight=h;drawn=0;bottom=0;
    float measured=fonteInfoLinha(&f,0,0,limit,0);assert(!drawn);
    float actual=fonteInfoLinha(&f,0,0,limit,1);
    assert(drawn==8 && measured==actual && actual>=bottom);
  }
  FonteInfo empty={0};assert(fonteInfoLinha(&empty,0,0,200,1)==0);
  puts("fonteinfo_layout: real title row wraps without clipping; measurement and font-height reservation agree PASS");
}
