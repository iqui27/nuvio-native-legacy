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
int main(void) {
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
