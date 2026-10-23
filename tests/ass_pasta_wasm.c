// #370: legenda arabe em texto simples (SRT) na Samsung .wgt saia em quadradinhos.
// No wasm o libass nao recebia o NotoNaskhArabic: SDL_GetBasePath devolve "/", a
// pasta "/fonts" nao existe e a de reserva "deploy/app/fonts" tambem nao; as
// fontes do app vivem em /app/fonts. O log do relato mostrava
// "fontselect: (Noto Naskh Arabic, 400, 0) -> /app/fonts/InterDisplay-Regular.ttf".
//
//   bash tests/ass_pasta_wasm.sh
#include "../src/assrender.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  char o[640];
  assrender_pasta_fontes_app("/", 1, o, sizeof o);
  printf("  wasm base=/ -> %s\n", o);
  assert(!strcmp(o, "/app/fonts"));
  assrender_pasta_fontes_app(NULL, 1, o, sizeof o);
  assert(!strcmp(o, "/app/fonts"));
  // fora do wasm nada muda: sem <base>fonts cai na pasta do repositorio
  assrender_pasta_fontes_app("/nao/existe/", 0, o, sizeof o);
  assert(!strcmp(o, "deploy/app/fonts"));
  assrender_pasta_fontes_app(NULL, 0, o, sizeof o);
  assert(!strcmp(o, "deploy/app/fonts"));
  puts("ok");
  return 0;
}
