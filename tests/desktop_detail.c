// Desktop rows stay readable while TV retains section focus dimming.
#include <assert.h>
#include <string.h>
#include "../src/layout.c"
#include "../src/detail.c"
static int homeLayout = HOME_LAYOUT_PADRAO;
int ajustes_home_layout(void) { return homeLayout; }
float ajustes_conteudo_x(void) { return 432; }
float gfx_escala_tela_txt(void) { return 1; }
static int trailerCount;
static CatItem title = {.tipo="series"};
const CatItem *cat_item(int i) { (void)i; return &title; }
int trailer_suportado(void) { return 0; }
int extras_n_trailers(void) { return trailerCount; }
int extras_temporadas(void) { return 0; }
int extras_progresso_serie(int *watched, int *aired) { return 0; }
int main(void) {
  layout_janela_definir(1920,1080);
  nivel=1; foco.fileira=SEC_NOTAS;
  for(int r=0;r<N_SECOES;r++) {
#ifdef NV_DESKTOP_UI
    assert(secaoAlvo(r)==1.f);
#else
    assert(secaoAlvo(r)==(r==SEC_NOTAS?1.f:.42f));
#endif
  }
#ifdef NV_DESKTOP_UI
  assert(dicaSecao(SEC_TRAILERS)==NULL);
#else
  assert(!strcmp(dicaSecao(SEC_TRAILERS),"OK abre no player, com som"));
#endif
  // The approved desktop detail always has four landscape cards with captions,
  // independently of Home's TV layout preference. Their complete text fits.
#ifdef NV_DESKTOP_UI
  layout_janela_definir(1440,900);
  for (homeLayout=0; homeLayout<=2; homeLayout++) {
    assert(epApple() && !epModerno());
    assert(fabsf(epThumbH()/epCardW()-9.f/16)<.001f);
    assert(NV_DETP_X+3*epCardPasso()+epCardW()<=NV_TELA_W-48);
    assert(NV_TELA_H*.55f+112+epCardH()<NV_TELA_H-16);
  }
  // Pointer and keyboard use the same action order, with/without a trailer.
  assert(acaoEm(0)==ACAO_PRIMARIO);
  assert(acaoEm(1)==ACAO_LISTA && acaoEm(2)==ACAO_MAIS);
  trailerCount=1;
  assert(acaoEm(1)==ACAO_TRAILER);
  assert(acaoEm(2)==ACAO_LISTA && acaoEm(3)==ACAO_MAIS);
  layout_janela_definir(800,1000);
  homeLayout=HOME_LAYOUT_PADRAO;
  assert(!epApple());
  assert(epCardW()==NV_DETP_EP_W);
#else
  assert(!epApple() && !epModerno());
  assert(epCardW()==NV_DETP_EP_W);
  assert(epThumbH()==NV_DETP_EP_THUMB_H);
#endif
  return 0;
}
