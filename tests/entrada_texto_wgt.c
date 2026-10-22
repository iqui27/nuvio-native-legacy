// Arnes do teste do .wgt (tests/entrada_texto_wgt.cjs): src/entrada_texto.c
// compilado em WASM, falando com a ponte JS de tools/tizen-shell.html.
#include <SDL2/SDL.h>
#include <emscripten.h>
#include <string.h>
#include "../src/entrada_texto.h"

static char ultimo[300];
static int nTexto, nFim, fimConfirmou = -1;

EMSCRIPTEN_KEEPALIVE int t_iniciar(void) { return SDL_Init(SDL_INIT_EVENTS); }
EMSCRIPTEN_KEEPALIVE int t_disponivel(void) { return texto_sistema_disponivel(); }
EMSCRIPTEN_KEEPALIVE void t_mic_modo(int m) { texto_sistema_mic_modo(m); }
EMSCRIPTEN_KEEPALIVE void t_abrir(const char *s, int voz) { texto_sistema_abrir(s, voz); }
EMSCRIPTEN_KEEPALIVE void t_fechar(void) { texto_sistema_fechar(); }
EMSCRIPTEN_KEEPALIVE int t_aberto(void) { return texto_sistema_aberto(); }
// Um quadro do app: o modulo le a ponte, e o "campo" consome os avisos.
EMSCRIPTEN_KEEPALIVE void t_quadro(void) {
  SDL_Event e;
  texto_sistema_quadro();
  while (SDL_PollEvent(&e)) {
    texto_sistema_observar(&e);
    if (e.type != texto_sistema_evento()) continue;
    if (e.user.code == TS_EV_TEXTO) { nTexto++; snprintf(ultimo, sizeof ultimo, "%s", texto_sistema_valor()); }
    if (e.user.code == TS_EV_FIM) { nFim++; fimConfirmou = e.user.data1 != NULL; }
  }
}
EMSCRIPTEN_KEEPALIVE const char *t_valor(void) { return ultimo; }
EMSCRIPTEN_KEEPALIVE int t_n_texto(void) { return nTexto; }
EMSCRIPTEN_KEEPALIVE int t_n_fim(void) { return nFim; }
EMSCRIPTEN_KEEPALIVE int t_fim_confirmou(void) { return fimConfirmou; }
