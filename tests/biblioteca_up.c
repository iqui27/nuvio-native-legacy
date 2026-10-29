// Exercise the 2.1 Library handlers without GL, accounts, or network.
#define ajustes_menu_topo biblioteca_up_menu_topo
#include "../src/biblioteca.c"
#include <assert.h>

static int topMenu;
int biblioteca_up_menu_topo(void) { return topMenu; }

static void key(Uint32 type, SDL_Keycode code) {
  SDL_Event event = {0};
  event.type = type;
  event.key.keysym.sym = code;
  biblioteca_evento_(&event);
}

int main(void) {
  for (topMenu = 0; topMenu <= 1; topMenu++) {
    for (int state = EST_TITULOS; state <= EST_ITENS; state++) {
      modo = state == EST_TITULOS ? MODO_PARAVER : MODO_LISTAS;
      temAberta = state == EST_ITENS;
      for (int col = 0; col < 3; col++) {
        foco.fileira = BIB_FIL_MODO;
        foco.coluna = acaoSel = col;
        sair = pediuRelogio = focoCont = 0;
        key(SDL_KEYUP, SDLK_UP);
        assert(!biblioteca_quer_sair() && !biblioteca_pediu_relogio());
        key(SDL_KEYDOWN, SDLK_UP);
        assert(biblioteca_quer_sair() == topMenu);
        assert(biblioteca_pediu_relogio() == !topMenu);
        assert(!biblioteca_pediu_relogio());
        assert(foco.fileira == BIB_FIL_MODO && foco.coluna == col);
      }
      // 2.1 has separate filter and tab rows: Up visits the tabs first.
      if (!temAberta) {
        sair = pediuRelogio = 0;
        foco.fileira = BIB_FIL_PICK;
        foco.coluna = pickSel = 1;
        key(SDL_KEYDOWN, SDLK_UP);
        assert(!biblioteca_quer_sair() && !biblioteca_pediu_relogio());
        assert(foco.fileira == BIB_FIL_MODO);
        key(SDL_KEYDOWN, SDLK_UP);
        assert(biblioteca_quer_sair() == topMenu);
        assert(biblioteca_pediu_relogio() == !topMenu);
      }
    }
    // The 2.1 Continue button stays between list actions and navigation.
    modo = MODO_LISTAS; temAberta = 1;
    aberta.fonte = LST_TRAKT; pagProximo = 0; trilha = 0;
    foco.fileira = 0; foco.coluna = acaoSel = 2;
    focoCont = sair = pediuRelogio = 0;
    key(SDL_KEYDOWN, SDLK_UP);
    assert(focoCont && !biblioteca_quer_sair() && !biblioteca_pediu_relogio());
    key(SDL_KEYDOWN, SDLK_UP);
    assert(biblioteca_quer_sair() == topMenu);
    assert(biblioteca_pediu_relogio() == !topMenu);
    focoCont = 0; pagProximo = -1;
  }
  puts("Library Up: PASS (2.1 tabs, filters, list actions, Continue; top menu and sidebar)");
  return 0;
}
