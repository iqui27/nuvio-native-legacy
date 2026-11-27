// Actual GLES snapshot/copy pixels; exercise the production wrapper while
// providing a deterministic static title page instead of a logged-in app.
#include "corviva.h"
static unsigned backdropClock;
static int backdropOpened=1,backdropPaints;
static int backdropYes(void){return 1;}
static int backdropNo(void){return 0;}
static int backdropOpen(void){return backdropOpened;}
static unsigned backdropTicks(void){return backdropClock;}
static const char *backdropStill(void){return "-";}
static void backdropLive(int p,unsigned rev,void (*draw)(void *),void *ctx){(void)p;(void)rev;draw(ctx);}
#define NV_SOURCE_BACKDROP_TEST
#define fontes_fundo testedFontesFundo
#define SDL_GetTicks backdropTicks
#define stream_folha_aberta backdropOpen
#define detail_aberto backdropYes
#define detail_cobre_tela backdropYes
#define detail_transicao backdropStill
#define player_aberto backdropNo
#define player_mini_ativo backdropNo
#define player_retido backdropNo
#define trailer_tocando backdropNo
#define trailer_aberto backdropNo
#define spainel_visivel backdropNo
#define ctx_aberto backdropNo
#define menu_aberto backdropNo
#define vertudo_aberta backdropNo
#define registro_aberto backdropNo
#define spot_cheio backdropNo
#define ajustes_vidro backdropNo
#define nv_perfil_sessao_ler backdropYes
#define corviva_retargets backdropNo
#define detail_indice backdropNo
#define ajustes_idioma backdropNo
#define cat_revisao_itens backdropNo
#define cat_versao_episodios backdropNo
#define spainel_fundo backdropLive
#include "../src/source_backdrop.inc"
#undef spainel_fundo
#undef cat_revisao_itens
#undef cat_versao_episodios
#undef ajustes_idioma
#undef detail_indice
#undef corviva_retargets
#undef nv_perfil_sessao_ler
#undef ajustes_vidro
#undef spot_cheio
#undef registro_aberto
#undef vertudo_aberta
#undef menu_aberto
#undef ctx_aberto
#undef spainel_visivel
#undef trailer_tocando
#undef trailer_aberto
#undef player_retido
#undef player_mini_ativo
#undef player_aberto
#undef detail_transicao
#undef detail_cobre_tela
#undef detail_aberto
#undef stream_folha_aberta
#undef SDL_GetTicks
#undef fontes_fundo
#undef NV_SOURCE_BACKDROP_TEST
static void backdropPattern(void *ctx){
  (void)ctx;backdropPaints++;
  gfx_cor((GfxRect){0,0,1920,1080},0,.11f,.13f,.17f,1);
  gfx_cor((GfxRect){42.25f,70.5f,720,630},.04f,.4f,.25f,.15f,1);
  gfx_cor((GfxRect){900.5f,510.25f,550,420},.04f,.12f,.45f,.25f,.7f);
  txt_desenhar(txt_linha(TXT_PG_FIM,"Static title page — full filename",230,230,230,255),60,730);
}
static void backdropForeground(int frame){
  // The source sheet/focus must stay live above the copied background.
  gfx_cor((GfxRect){1190,42,700,990},.02f,.08f,.085f,.1f,.94f);
  gfx_cor((GfxRect){1210,230+frame*31.5f,650,190},.04f,.3f,.32f,.36f,.9f);
}
static void sourceBackdropPixels(void){
  assert(tex_iniciar(64));assert(gfx_snap_iniciar(1920,1080));glViewport(0,0,1920,1080);
  int grad=nv_grad_ativo,hdr=gfx_hdr_output;float ambient=nv_ambiente_forca;
  nv_grad_ativo=gfx_hdr_output=0;nv_ambiente_forca=0;
  unsigned char *reference=malloc(1920*1080*4),*cached=malloc(1920*1080*4);assert(reference&&cached);
  backdropClock=0;testedFontesFundo(0,1,backdropPattern,NULL);
  for(int frame=0;frame<6;frame++){
    txt_novo_quadro();gfx_sem_recorte();glClear(GL_COLOR_BUFFER_BIT);
    backdropPattern(NULL);backdropForeground(frame);glFinish();
    glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,reference);
    // Also cross the former two-second repaint interval. The static title
    // page remains pixel-identical and is painted only for the first copy.
    glClear(GL_COLOR_BUFFER_BIT);backdropClock=700+frame*2500;
    int before=backdropPaints;
    testedFontesFundo(0,1,backdropPattern,NULL);backdropForeground(frame);glFinish();
    assert(backdropPaints==before+(frame==0));
    glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,cached);
    // RGB snapshot rounding may differ by a single 8-bit level.
    for(size_t i=0;i<1920*1080*4;i++)assert(abs((int)reference[i]-cached[i])<=1);
  }
  backdropOpened=0;testedFontesFundo(0,1,backdropPattern,NULL);backdropOpened=1;
  free(reference);free(cached);gfx_snap_encerrar();tex_encerrar();
  nv_grad_ativo=grad;gfx_hdr_output=hdr;nv_ambiente_forca=ambient;
  puts("PASS: actual GLES backdrop snapshot matches live background within one RGB level, while the source foreground keeps moving");
}
