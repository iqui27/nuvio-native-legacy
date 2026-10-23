// #440: compare actual glyph pixels, not the line box (fonts have different metrics).
#include "gfx.h"
#include <SDL2/SDL_ttf.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
// Keep SDL_ttf, shaping, font selection and caching real. Capture the GPU
// upload/draw boundary so the check also runs without a WindowServer.
static int uploadedInk, uploadedW, uploadedH, draws;
static GLuint nextTexture;
static void upload(GLenum target, GLint level, GLint format, GLsizei w, GLsizei h,
                   GLint border, GLenum f, GLenum type, const void *pixels) {
  (void)target;(void)level;(void)format;(void)border;(void)f;(void)type;
  const unsigned char *p=pixels; int first=-1,last=-1;
  for(int y=0;y<h;y++)for(int x=0;x<w;x++)if(p[(y*w+x)*4+3]>5){if(first<0)first=y;last=y;break;}
  uploadedInk=last-first+1; uploadedW=w; uploadedH=h;
}
static void gen(GLsizei n, GLuint *t) { while(n--) *t++=++nextTexture; }
static void draw(GfxRect r, GLuint t) { assert(t && r.w==uploadedW && r.h==uploadedH); draws++; }
#define glGenTextures gen
#define glTexImage2D upload
#define glBindTexture(...) ((void)0)
#define glTexParameteri(...) ((void)0)
#define glDeleteTextures(...) ((void)0)
#define gfx_tex_esquecer(...) ((void)0)
#define gfx_rect(r,t,...) draw(r,t)
#include "../src/text.c"
static int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL: %s\n", #c); failures++; } } while (0)
int main(void) {
  assert(SDL_Init(SDL_INIT_TIMER)==0);
  gfx_tamanho_alvo(1920,1080); assert(txt_iniciar("deploy/app",1));
  // Arabic UI must prefer bundled Sans even when system Naskh is available.
  const char *selected = txt_fonte_da_linha(TXT_FAMILIA_INTER, TXT_DET_META, "الإعدادات");
  CHECK(selected && strstr(selected, "NotoSansArabic-Regular-Subset.ttf"));
  TTF_Font *regular = fonteDe(TXT_FAMILIA_INTER, TXT_DET_META, "الإعدادات");
  arabeCorpo = (int)(corpoAtual(TXT_DET_META) * ESC_T + 0.5f);
  TTF_Font *bold = arabeBoldFace(regular);
  CHECK(bold && !strcmp(TTF_FontFaceFamilyName(bold), "Noto Sans Arabic"));
  CHECK(bold && !strcmp(TTF_FontFaceStyleName(bold), "Bold"));
  CHECK(bold && bold != regular);
  const int percentages[]={90,120,250,300};
  for(unsigned i=0;i<sizeof percentages/sizeof *percentages;i++) {
    TxtEstilo est=(TxtEstilo)(TXT_LEG_50+(percentages[i]-50)/10);
    txt_novo_quadro();
    TxtLinha r=txt_linha_corta_enfase(est,"م",255,255,255,255,10000,TXT_FAMILIA_INTER,0);
    int rh=uploadedInk; assert(r.tex);
    TxtLinha b=txt_linha_corta_enfase(est,"م",255,255,255,255,10000,TXT_FAMILIA_INTER,TXT_ENF_NEGRITO);
    int bh=uploadedInk;
    fprintf(stderr,"Arabic %d%%: regular=%d bold=%d delta=%.1f%% (%s)\n",percentages[i],rh,bh,100.0*(bh-rh)/rh,txt_fonte_da_linha(TXT_FAMILIA_INTER,est,"م"));
    CHECK(rh>0 && abs(bh-rh)*100<=rh*5);
    TxtLinha cached=txt_linha_corta_enfase(est,"م",255,255,255,255,10000,TXT_FAMILIA_INTER,TXT_ENF_NEGRITO);
    CHECK(cached.tex==b.tex);
    const char *mixed="م A";
    TxtLinha m=txt_linha_corta_enfase(est,mixed,255,255,255,255,10000,TXT_FAMILIA_INTER,TXT_ENF_NEGRITO);
    CHECK(m.w==larguraLinha(est,mixed,TXT_FAMILIA_INTER,TXT_ENF_NEGRITO));
    for(int fam=0;fam<TXT_FAMILIA_N;fam++) {
      TTF_Font *regular=fonteLegendaDe(est,"H",(TxtFamilia)fam);
      TTF_Font *bold=fonteNegritoReal((TxtFamilia)fam,est,regular);
      if(bold) CHECK(abs(TTF_FontAscent(bold)-TTF_FontAscent(regular))*100<=TTF_FontAscent(regular)*5);
    }
  }
  for(int pct=260;pct<=300;pct+=10) {
    txt_novo_quadro();
    TxtEstilo est=(TxtEstilo)(TXT_LEG_50+(pct-50)/10);
    TxtLinha l=txt_linha_corta_enfase(est,"H",255,255,255,255,10000,TXT_FAMILIA_INTER,0);
    CHECK(l.tex && abs(uploadedInk-(int)(pct*0.4*0.73+0.5))<=2);
    txt_desenhar_alpha(l,0,0,1);
    fprintf(stderr,"%d%% rendered/drawn H: %dx%d ink=%d\n",pct,l.w,l.h,uploadedInk);
  }
  CHECK(draws==5);
  txt_encerrar(); SDL_Quit();
  return failures?1:0;
}
