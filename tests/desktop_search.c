#include "../src/layout.c"
#include "../src/busca.c"
#include <assert.h>
#include <stdio.h>
int main(void) {
  layout_janela_definir(1280,800);
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
  puts("PASS: wide/narrow desktop typing, no duplicate ASCII, Unicode, UTF-8 backspace");
}
