// A registered touch scroll captures the drag only in Mobile mode; in TV mode
// the drag stays remote navigation (arrows), exactly as before the touch UI.
#define main ponteiro_roteiro_mouse
#include "ponteiro.c"
#undef main
#include <math.h>
#include "layout.h"

static int nRol;
static int rolFn(const PonteiroRolagem *e) { if (e->fase == PONT_ROL_INICIO) nRol++; return 1; }
static void telaRol(void) {
  ponteiro_alvo(0, 0, 1920, 1080, focar, NULL, 5, 5);
  ponteiro_rolagem(rolFn);
}
static void dedo(Uint32 tipo, float x, float y) {
  SDL_Event e; SDL_zero(e);
  e.type = tipo; e.tfinger.touchId = 3; e.tfinger.fingerId = 1;
  e.tfinger.x = x / 1920.0f; e.tfinger.y = y / 1080.0f;
  ponteiro_evento(&e, entregar);
}
static int setas(void) {
  int n = 0;
  for (int i = 0; i < nEntregues; i++) if (entregues[i].type == SDL_KEYDOWN &&
      (entregues[i].key.keysym.sym == SDLK_DOWN || entregues[i].key.keysym.sym == SDLK_UP)) n++;
  return n;
}
static void arrastar(int mobile, int *capturou, int *nSetas) {
  layout_modo_definir(mobile);
  ponteiro_teste_relogio(agora);
  ponteiro_teste_janela(1920, 1080);
  ponteiro_iniciar(); ponteiro_teste_toque(1);
  quadro(telaRol); zerar(); nRol = 0;
  dedo(SDL_FINGERDOWN, 960, 700);
  for (int y = 680; y >= 300; y -= 20) dedo(SDL_FINGERMOTION, 960, (float)y);
  dedo(SDL_FINGERUP, 960, 300);
  *capturou = nRol; *nSetas = setas();
}
int main(void) {
  int c, s;
  arrastar(0, &c, &s);
  CONFERE(c == 0 && s > 0, "TV: arrasto nao captura rolagem, vira setas");
  arrastar(1, &c, &s);
  CONFERE(c == 1 && s == 0, "Mobile: arrasto captura a rolagem registrada");
  puts("PASS: touch scroll capture is Mobile only");
  return 0;
}
