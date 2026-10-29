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
  puts("PASS: typing from results, no duplicate ASCII, composed Unicode, UTF-8 backspace, desktop results layout");
}
