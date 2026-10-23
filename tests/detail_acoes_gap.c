// Execute the production row and focus transforms with drawing/text stubs.
#include "detail_acoes.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned Uint32;
typedef struct { float x,y,w,h; } GfxRect;
typedef struct { int w,h; } TxtLinha;
typedef struct { const char *imdb; } CatItem;
static int grouped, restart, nivel, botao, maisAcoes, idx, primarioPct;
static int vidro, count, failures, total, reminder;
static float expansion, yAcoes;
static GfxRect drawn[8], maisAncora;
static CatItem item = {"test"}, *ci = &item;
static const char *rot = "Play", *morphLogo;
static int ponteiroDetalhe;
#define TXT_DET_BOTAO 0
#define BOTAO_PAD_X 24
#define BOTAO_PAD_X2 24
#define ACAO_LISTA 1
#define ACAO_LEMBRAR 2
#define ACAO_MAIS 3
#define BT_TEXTO_REP 235
#define NV_RAIO_PILL .5f
static void record(GfxRect r, int action) { (void)action; drawn[count++] = r; }
static int acoesAgrupadas(void) { return grouped; }
static int temInicio(void) { return restart; }
static int nBotoesTodos(void) { return total; }
static int nBotoes(void) { return total; }
static int detalheRetrato(void) { return 0; }
static int temLembrar(void) { return reminder; }
static int acaoEm(int j) { return j == total-1 ? ACAO_MAIS : (reminder ? ACAO_LEMBRAR : ACAO_LISTA); }
static float larguraPrimario(const char *s) { (void)s; return 321; }
static float larguraSecundario(const char *s) { (void)s; return 200; }
static GfxRect heroAcaoRect(float *x,float *y,float w,float h) { return (GfxRect){*x,*y-h*.5f,w,h}; }
static Uint32 SDL_GetTicks(void) { static Uint32 t; return t+=16; }
static void anim_expande(float *w,float *r,float target,float dt,float a,float b) { *w=expansion; *r=1; }
static float anim_suave(float f) { return f; }
static float anim_clamp(float x,float lo,float hi) { return fminf(hi,fmaxf(lo,x)); }
static float anim_mola2(float *v,float x,float target,float dt,float k) { return target; }
static int ajustes_animacoes_reduzidas(void) { return 1; }
static int ajustes_vidro(void) { return vidro; }
static float ajustes_acento_tinta(void *r,void *g,void *b) { return 1; }
static float botao_cor_foco(float *r,float *g,float *b) { *r=*g=*b=1; return 1; }
static const char *i18n(const char *s) { return s; }
static const CatItem *cat_item(int i) { return ci; }
static int agenda_lembrete(const char *s) { return 0; }
static TxtLinha txt_linha(int e,const char *s,int r,int g,int b,int a) { return (TxtLinha){100,24}; }
#define foco_anel(...) ((void)0)
#define player_morph_de(...) ((void)0)
#define ponteiro_alvo(...) ((void)0)
#define gfx_icone(...) ((void)0)
#define txt_desenhar_alpha(...) ((void)0)
static void desenhaSecundario(GfxRect,const char *,int,float);
static void desenhaLembrete(GfxRect,int,int,float);
#include "row.h"
static void desenhaSecundario(GfxRect r,const char *s,int f,float a) { botao_pilula(r,s,NULL,f,0,0,a); }
static void desenhaLembrete(GfxRect r,int on,int f,float a) { desenhaBotao(r,NULL,ACAO_LEMBRAR,f,a); }

int main(void) {
  for (grouped=0; grouped<2; grouped++)
  for (restart=0; restart<2; restart++)
  for (reminder=0; reminder<2; reminder++)
  for (vidro=0; vidro<2; vidro++)
  for (int step=0; step<=4; step++) {
    expansion=step*.25f;
    total=1+restart+3;
    for (nivel=0; nivel<=1; nivel++)
    for (botao=0; botao<total; botao++) {
      count=0;
      row();
      // Grouped circles render back to front. Compare actual surface edges.
      for (int i=0; i<count; i++)
      for (int j=i+1; j<count; j++)
        if (drawn[j].x<drawn[i].x) { GfxRect r=drawn[i]; drawn[i]=drawn[j]; drawn[j]=r; }
      for (int i=1; i<count; i++) {
        float gap=drawn[i].x-drawn[i-1].x-drawn[i-1].w;
        if (fabsf(gap-NV_DETW2_BTN_GAP)>.01f) {
          if (failures++<12) printf("FAIL grouped=%d restart=%d focus=%d level=%d expansion=%.2f neighbour=%d gap=%.3f\n",grouped,restart,botao,nivel,expansion,i,gap);
        }
      }
    }
  }
  printf("%s: production row gaps (%d failures)\n",failures?"FAIL":"PASS",failures);
  return failures?1:0;
}
