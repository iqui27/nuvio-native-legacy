// #508: o desenho e a copia da arte usam player.c/detail.c reais. As
// fronteiras de textura, desenho e pagina estavel sao locais e tipadas:
// nenhuma janela, video ou rede e aberta pelo teste.
#define ajustes_abertura_fundo loading_test_ajustes_abertura_fundo
#define ajustes_abertura_estilo loading_test_ajustes_abertura_estilo
#define ajustes_animacoes_reduzidas loading_test_ajustes_animacoes_reduzidas
#define ajustes_hero_fonte loading_test_ajustes_hero_fonte
#define ajustes_hero_arte_diferente loading_test_ajustes_hero_arte_diferente
#define ajustes_acento loading_test_ajustes_acento
#define tex_falhou loading_test_tex_falhou
#define tex_obter_hero loading_test_tex_obter_hero
#define tex_obter_larg loading_test_tex_obter_larg
#define tex_obter_larg_qualquer loading_test_tex_obter_larg_qualquer
#define tex_marca_escura loading_test_tex_marca_escura
#define tex_aspecto loading_test_tex_aspecto
#define gfx_desfocado loading_test_gfx_desfocado
#define gfx_rect loading_test_gfx_rect
#define gfx_cor loading_test_gfx_cor
#define gfx_anel loading_test_gfx_anel
#define gfx_arco loading_test_gfx_arco
#define gfx_tex_aspect_atual loading_test_gfx_tex_aspect_atual
#define txt_linha_corta loading_test_txt_linha_corta
#define txt_linha loading_test_txt_linha
#define txt_desenhar_alpha loading_test_txt_desenhar_alpha
#define i18n loading_test_i18n
#define video_pronto loading_test_video_pronto
#define video_falhou loading_test_video_falhou
#define nv_layout_w loading_test_nv_layout_w
#define nv_layout_h loading_test_nv_layout_h
#define layout_modo_mobile loading_test_layout_modo_mobile
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "catalogo.h"
#include "../src/player.c"

static int fundoStyle=2, reduced, smallRequests, blurRequests, pictures, snaps;
#ifdef NV_TOUCH_UI
static int mobileMode;
#endif
static int checks, failures, textureReady=1, snapshotPoster;
static char snapshot[2048], requested[2048], failed[2048];
static GfxRect lastPicture;
static float lastAlpha, shadeAlpha, fakeAspect=16.0f/9.0f;
float gfx_tex_aspect_atual;
#ifdef NV_TOUCH_UI
float nv_layout_w=1920, nv_layout_h=1080;
int layout_modo_mobile(void) { return mobileMode; }
#endif

void fixture_detail_set(const CatItem *, const char *, int);
void fixture_detail_backdrop(const char *);
void fixture_detail_poster(const char *);
void fixture_detail_ownership_checks(void (*)(int,const char *));
int ajustes_abertura_fundo(void) {return fundoStyle;}
int ajustes_abertura_estilo(void) {return 0;}
int ajustes_animacoes_reduzidas(void) {return reduced;}
int ajustes_hero_fonte(void) {return ARTEHERO_CATALOGO;}
int ajustes_hero_arte_diferente(void) {return 0;}
void ajustes_acento(float *r,float *g,float *b) {*r=.3f;*g=.6f;*b=1;}
int tex_falhou(const char *u) {return u && failed[0] && !strcmp(u,failed);}
GLuint tex_obter_hero(const char *u) {snprintf(requested,sizeof requested,"%s",u);return textureReady?11:0;}
GLuint tex_obter_larg(const char *u,float w) {(void)u;assert(w==480);smallRequests++;return textureReady?12:0;}
GLuint tex_obter_larg_qualquer(const char *u,float w) {(void)u;(void)w;return 0;}
int tex_marca_escura(const char *u) {(void)u;return 0;}
float tex_aspecto(const char *u) {(void)u;return fakeAspect;}
GLuint gfx_desfocado(GLuint t,const char *u) {assert(t==12&&u&&*u);blurRequests++;return 13;}
void gfx_rect(GfxRect r,GLuint tex,GfxModo mode,float f,float x,float y,float radius,float red,float green,float blue,float alpha) {
  (void)f;(void)x;(void)y;(void)radius;(void)red;(void)green;(void)blue;
  if((mode==GFX_CARD || mode==GFX_SNAP) && tex==11){pictures++;lastPicture=r;lastAlpha=alpha;if(mode==GFX_SNAP)shadeAlpha=f;}
  if(mode==GFX_SNAP && tex==13)snaps++;
  if(mode==GFX_VEU_BAIXO && !tex && r.y==540 && r.h==540)shadeAlpha=alpha;   // A3: half-screen veil
}
void gfx_anel(GfxRect r,float radius,float e,float red,float green,float blue,float alpha) {(void)r;(void)radius;(void)e;(void)red;(void)green;(void)blue;(void)alpha;}
void gfx_arco(GfxRect r,float e,float i,float f,float red,float green,float blue,float alpha) {(void)r;(void)e;(void)i;(void)f;(void)red;(void)green;(void)blue;(void)alpha;}
void gfx_cor(GfxRect r,float radius,float red,float green,float blue,float alpha) {
  (void)radius;
  if(r.w==1920&&r.h==1080&&red==0&&green==0&&blue==0)shadeAlpha=alpha;
}
TxtLinha txt_linha_corta(TxtEstilo e,const char *s,int r,int g,int b,int a,float w) {
  (void)e;(void)s;(void)r;(void)g;(void)b;(void)a;(void)w;return(TxtLinha){.w=200,.h=50};
}
TxtLinha txt_linha(TxtEstilo e,const char *s,int r,int g,int b,int a) {
  return txt_linha_corta(e,s,r,g,b,a,200);
}
void txt_desenhar_alpha(TxtLinha l,float x,float y,float a) {(void)l;(void)x;(void)y;(void)a;}
const char *i18n(const char *s) {return s;}
int video_pronto(void) {return 0;}
int video_falhou(void) {return 0;}
static void verify(int ok,const char *s) {checks++;printf("%s %s\n",ok?"PASS":"FAIL",s);if(!ok)failures++;}
static void resetDraw(void) {smallRequests=blurRequests=pictures=snaps=0;requested[0]=0;shadeAlpha=lastAlpha=0;}
static void begin(const CatItem *c) {
  fixture_detail_set(c,snapshot,snapshotPoster);
  cinemaArteIniciar(c);
  cine=(AbreCine){.viva=1,.veu=1,.foco=1,.desde=1000};
  aberto=1;esperandoFonte=1;comVideo=0;reduced=0;textureReady=1;failed[0]=0;fakeAspect=16.0f/9.0f;
  resetDraw();
}
static void draw(const CatItem *c) {desenharCinema(2000,c,0);}
int main(void) {
  CatItem c={0};snprintf(c.imdb,sizeof c.imdb,"tt1234567");snprintf(c.tipo,sizeof c.tipo,"movie");
  snprintf(c.backdrop,sizeof c.backdrop,"https://example.invalid/catalog.jpg");
  snprintf(c.backdropCatalogo,sizeof c.backdropCatalogo,"%s",c.backdrop);
  snprintf(c.poster,sizeof c.poster,"https://example.invalid/poster.jpg");
  artehero_definir_falhou(tex_falhou);
  fundoStyle=2;snapshot[0]=0;begin(&c);draw(&c);
  verify(smallRequests==1&&blurRequests==1&&snaps==1,"default Cinema keeps cached blur rendering");
  fundoStyle=0;begin(&c);draw(&c);
  verify(!requested[0]&&!smallRequests&&!blurRequests&&!pictures&&!snaps,"saved Off choice draws no artwork or blur requests");
  fundoStyle=1;begin(&c);draw(&c);
  verify(!smallRequests&&!blurRequests&&!snaps&&pictures==1,"saved Art choice skips blur and keeps the full sharp image");
  verify(lastPicture.w==1920&&lastPicture.h==1080&&lastAlpha==1,"clear landscape artwork fills the screen");
  verify(shadeAlpha>0.0f&&shadeAlpha<=.851f,"clear Cinema retains readable darkening");
  snprintf(snapshot,sizeof snapshot,"https://example.invalid/selected-art.jpg");begin(&c);draw(&c);
  verify(!strcmp(requested,snapshot),"Play reuses the exact selected title-page URL");
  snapshot[0]=0;c.backdrop[0]=c.backdropCatalogo[0]=0;begin(&c);draw(&c);
  verify(strstr(requested,"metahub.space/background/")!=NULL,"empty backdrop still uses same-title IMDb artwork fallback");
  c.imdb[0]=0;begin(&c);fakeAspect=2.0f/3.0f;draw(&c);
  verify(!strcmp(requested,c.poster)&&lastPicture.w==720&&lastPicture.h==1080&&lastPicture.x==600,"portrait fallback is contained without distortion");
  snapshotPoster=1;snprintf(snapshot,sizeof snapshot,"%s",c.poster);begin(&c);fakeAspect=2.0f/3.0f;draw(&c);
  verify(lastPicture.w==720&&lastPicture.x==600,"captured title-page poster keeps its portrait layout");
  snprintf(c.imdb,sizeof c.imdb,"tt1234567");
  snprintf(c.backdrop,sizeof c.backdrop,"%s",c.poster);
  snprintf(c.backdropCatalogo,sizeof c.backdropCatalogo,"%s",c.poster);
  begin(&c);draw(&c);
  fixture_detail_backdrop("https://example.invalid/late-real-backdrop.jpg");resetDraw();draw(&c);
  verify(!strcmp(requested,"https://example.invalid/late-real-backdrop.jpg"),"temporary poster captured at Play upgrades when same-title metadata arrives");
  fixture_detail_backdrop("https://example.invalid/later-different-backdrop.jpg");resetDraw();draw(&c);
  verify(!strcmp(requested,"https://example.invalid/late-real-backdrop.jpg"),"accepted real loading backdrop stays frozen during later metadata changes");
  c.imdb[0]=c.backdrop[0]=c.backdropCatalogo[0]=0;
  snapshotPoster=0;snapshot[0]=0;c.poster[0]=0;begin(&c);draw(&c);
  verify(!requested[0]&&!pictures&&!smallRequests&&!blurRequests,"missing artwork stays neutral without delaying the loading screen");
  snprintf(snapshot,sizeof snapshot,"https://example.invalid/previous-title.jpg");begin(&c);
  snapshot[0]=0;snprintf(c.backdrop,sizeof c.backdrop,"https://example.invalid/next-title.jpg");
  snprintf(c.backdropCatalogo,sizeof c.backdropCatalogo,"%s",c.backdrop);begin(&c);draw(&c);
  verify(!strcmp(requested,c.backdrop),"Next/direct opening discards previous title-page snapshot");
  snprintf(snapshot,sizeof snapshot,"https://example.invalid/dead-snapshot.jpg");begin(&c);
  snprintf(failed,sizeof failed,"%s",snapshot);draw(&c);
  verify(!strcmp(requested,c.backdrop),"failed captured artwork falls through same-title artwork policy");
  snapshot[0]=0;begin(&c);textureReady=0;draw(&c);
  verify(!pictures&&!smallRequests,"clear loading tolerates an asynchronous texture miss");
  snprintf(snapshot,sizeof snapshot,"https://example.invalid/private-title.jpg");cinemaArteIniciar(&c);
  cinemaArteIniciar(NULL);verify(!cinemaArte[0]&&!cinemaPoster,"session teardown clears copied artwork state");
  fundoStyle=2;begin(&c);cine.foco=.25f;draw(&c);
  verify(pictures==1&&snaps==1&&lastAlpha==.75f,"default sharp-to-blurred transition is unchanged");
  verify(gfx_tex_aspect_atual==0,"renderer restores shared texture aspect state");
  fundoStyle=1; snapshotPoster=1;
  snprintf(c.imdb,sizeof c.imdb,"local:owned");
  c.backdrop[0]=c.backdropCatalogo[0]=0;
  snprintf(c.poster,sizeof c.poster,"https://example.invalid/original-portrait.jpg");
  snprintf(snapshot,sizeof snapshot,"%s",c.poster); begin(&c);
  fixture_detail_poster("https://example.invalid/new-poster-only.jpg");
  fakeAspect=2.0f/3.0f; draw(&c);
  verify(!strcmp(requested,snapshot)&&cinemaPoster&&lastPicture.w==720&&lastPicture.x==600,
         "loading keeps frozen old poster contained after poster-only enrichment");
  fixture_detail_backdrop("https://example.invalid/promoted-landscape.jpg"); resetDraw(); draw(&c);
  verify(!strcmp(requested,"https://example.invalid/promoted-landscape.jpg")&&!cinemaPoster,
         "loading promotes the same temporary portrait when a real backdrop arrives");
  fixture_detail_ownership_checks(verify);
#ifdef NV_TOUCH_UI
  // Galaxy 1440x3120: a tela logica tem aresta curta de 1080.
  mobileMode=1; nv_layout_w=1080; nv_layout_h=2340;
  fundoStyle=1; snapshotPoster=1;
  snprintf(c.poster,sizeof c.poster,"https://example.invalid/mobile-poster.jpg");
  snprintf(snapshot,sizeof snapshot,"%s",c.poster); begin(&c);
  fakeAspect=2.0f/3.0f; draw(&c);
  verify(lastPicture.w==1080 && lastPicture.h==1620 && lastPicture.x==0 && lastPicture.y==360,
         "mobile portrait contains the poster at its native ratio and centers it");
  nv_layout_w=2340; nv_layout_h=1080; resetDraw(); draw(&c);
  verify(lastPicture.w==720 && lastPicture.h==1080 && lastPicture.x==810 && lastPicture.y==0,
         "mobile landscape contains the poster without retaining portrait geometry");
  nv_layout_w=1920; nv_layout_h=1080; mobileMode=0;
#endif
  printf("RESULT checks=%d failures=%d\n",checks,failures);return failures?1:0;
}
