// Search result rows drag-scroll only in Mobile mode; TV keeps drag-as-arrows.
// Samsung: IME=0, voice=1, phone=0 must still have a D-pad route to the mic.
#define st_ime_disponivel teste_ime
#define st_voz_disponivel teste_voz
#define st_voz_iniciar teste_iniciar_voz
#define celb_disponivel teste_cel
#include "../src/busca.c"
#undef st_ime_disponivel
#undef st_voz_disponivel
#undef st_voz_iniciar
#undef celb_disponivel

static int temIme, temVoz, temCel, vozIniciada;
int teste_ime(void) { return temIme; }
int teste_voz(void) { return temVoz; }
int teste_cel(void) { return temCel; }
int teste_iniciar_voz(int dono) { vozIniciada = dono; return 1; }
#include <assert.h>

static void arrasto(int mobile) {
  PonteiroRolagem e = { PONT_ROL_INICIO, 0, 0.0f, 0.0f, 0.0f, 0.0f };
  layout_modo_definir(mobile);
  nFil = 1; fil[0].n = 20; fil[0].pessoas = 0; fil[0].melhor = 0;
  scrollX[0] = 0.0f; toqueLivreX[0] = 0; toqueFileira = -1;
  e.x = BU_RES_X + 10.0f; e.y = BU_RES_Y + 10.0f;
  int captura = toqueBuscaRolar(&e);
  e.fase = PONT_ROL_MOVER; e.delta = -200.0f;
  toqueBuscaRolar(&e);
  if (mobile) assert(captura == 1 && scrollX[0] == 200.0f && toqueLivreX[0]);
  else assert(captura == 0 && scrollX[0] == 0.0f && !toqueLivreX[0]);
}

int main(void) {
  layout_tela_definir(1920, 1080);
  arrasto(0);
  arrasto(1);
  layout_modo_definir(0);
  toqueBuscaLimpar();
  SDL_Event e; SDL_zero(e); e.type = SDL_KEYDOWN;
  for (int modo = 0; modo < 4; modo++) {
    temIme = modo == 1; temVoz = modo == 0; temCel = modo == 2;
    painel = campoFoco = 0;
    focus_iniciar(&focoKb, 1, KB_COLUNAS);
    e.key.keysym.sym = SDLK_UP; busca_evento(&e);
    assert(campoFoco == (temIme ? 1 : temVoz ? 2 : temCel ? 3 : 0));
    if (temVoz) {
      e.key.keysym.sym = SDLK_RETURN; busca_evento(&e);
      assert(vozIniciada == ST_BUSCA);
    }
  }
  puts("PASS: Busca row drag is Mobile only; Samsung mic reachable by D-pad");
  return 0;
}
