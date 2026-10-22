// Exercise the actual app layer macro: TV preserves click-as-OK on old modals.
#include "../src/app.c"
#include <assert.h>

static Uint32 relogio = 1000;
static int oks, falhas;
static Uint32 agoraTeste(void) { return relogio; }
static void entregarTeste(const SDL_Event *e) {
  if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_RETURN) oks++;
}
static void camada(int mobile, int aberta) {
  layout_modo_definir(mobile);
  ponteiro_iniciar(); ponteiro_teste_toque(1);
  ponteiro_quadro(relogio);
  ponteiro_alvo(10, 10, 100, 100, NULL, NULL, 0, 0); // underlying screen
  CAMADA_SE(aberta);
  ponteiro_desenhar();
  int n = ponteiro_teste_lista(NULL);
  if (n != (aberta ? mobile : 1)) {
    printf("FALHA: modo=%d aberta=%d alvos=%d\n", mobile, aberta, n); falhas++;
  }
  SDL_Event e; SDL_zero(e);
  e.type = SDL_MOUSEBUTTONDOWN; e.button.button = SDL_BUTTON_LEFT;
  e.button.x = 960; e.button.y = 540;
  relogio += 400; oks = 0;
  ponteiro_evento(&e, entregarTeste);
  e.type = SDL_MOUSEBUTTONUP; relogio += 60;
  ponteiro_evento(&e, entregarTeste);
  if (oks != (aberta && !mobile)) {
    printf("FALHA: modo=%d aberta=%d OKs=%d\n", mobile, aberta, oks); falhas++;
  }
}
int main(void) {
  layout_tela_definir(1920, 1080);
  ponteiro_teste_relogio(agoraTeste); ponteiro_teste_janela(1920, 1080);
  camada(0, 1); camada(1, 1); camada(0, 0); camada(1, 0);
  if (!falhas) puts("PASS: TV modal layer stays empty; Mobile backstop blocks clicks");
  return falhas ? 1 : 0;
}
