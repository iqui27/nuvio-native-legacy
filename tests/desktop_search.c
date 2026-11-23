#include "../src/layout.c"
#include "../src/busca.c"
#include <assert.h>
#include <stdio.h>
float gfx_escala_tela_txt(void) { return 1.0f; }
int menu_pilula_rect(float *x, float *y, float *w, float *h) { (void)x; (void)y; (void)w; (void)h; return 0; }
// The sidebar Search destination must show its field; results sit below it.
static void searchFieldShown(void) {
  for (painel = 0; painel < 2; painel++) {
    assert(buCampoVisivel());
    assert(buResY() >= buTopoEsq() + BU_CAMPO_H);
  }
  painel = 0;
}
static CatItem fixture;
int cat_n(void) { return 1; }
const CatItem *cat_item(int i) { return i==0 ? &fixture : NULL; }
int spotpessoa_n(const char *query) { (void)query;return 0; }
int spotpessoa_item(const char *query,int i,SpotPessoa *out) { (void)query;(void)i;(void)out;return 0; }
float gfx_poster_k(void) { return 1.0f; }
float ajustes_rail_largura_fixa(void) { return NV_DESKTOP_SIDEBAR_W; }
static void approvedBoard(void) {
  layout_janela_definir(1440,900);
  layout_tela_definir(2880,1800);
  // Board coordinates expressed on the app's 1920x1200 logical canvas.
  assert(fabsf(buX()-416)<.01f && fabsf(buDir()-1845.3333f)<.01f);
  assert(buTopoEsq()==72 && BU_CAMPO_H==52 && buResY()==173.33333f);
  assert(BU_MELHOR_H==200 && buCartazW()==192 && buPasso()==280);
  memset(fil,0,sizeof fil); sugestao=0;
  fil[0].melhor=1; fil[2].pessoas=1;
  assert(fabsf(BU_RES_Y+filTopo(1)+filKickH(1)-466.6667f)<.01f);
  assert(fabsf(BU_RES_Y+filTopo(2)-834.6667f)<.01f);
  assert(fabsf(BU_RES_Y+filTopo(3)+filKickH(3)-1005.3333f)<.01f);
  memset(fil,0,sizeof fil);
  fixture.nElenco=5;
  for(int i=0;i<5;i++) {
    fixture.elenco[i].tmdb=i+1;
    snprintf(fixture.elenco[i].nome,sizeof fixture.elenco[i].nome,"Dune %d",i);
  }
  montarPessoas("dun");assert(nPess==5);
}
int main(void) {
  approvedBoard();
  layout_janela_definir(1280,800);
  searchFieldShown();
  SDL_Event e={0};painel=1;
  e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_f;
  assert(desktopTexto(&e) && !consulta[0]);
  e.type=SDL_TEXTINPUT;snprintf(e.text.text,sizeof e.text.text,"Fallout é");
  assert(desktopTexto(&e) && !strcmp(consulta,"Fallout é") && painel==0);
  e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_BACKSPACE;
  assert(desktopTexto(&e) && !strcmp(consulta,"Fallout "));
  assert(buUmaColuna());
  layout_janela_definir(900,800);
  assert(!layout_desktop_ativo());
  painel=1;e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_f;
  assert(desktopTexto(&e) && !strcmp(consulta,"Fallout "));
  e.type=SDL_TEXTINPUT;snprintf(e.text.text,sizeof e.text.text,"café");
  assert(desktopTexto(&e) && !strcmp(consulta,"Fallout café") && painel==0);
  e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_BACKSPACE;
  assert(desktopTexto(&e) && !strcmp(consulta,"Fallout caf"));
  assert(buTecladoNativo());
  puts("PASS: search field visible on desktop; wide/narrow desktop typing, no duplicate ASCII, Unicode, UTF-8 backspace");
}
