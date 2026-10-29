// Exercise the 2.1 Library handlers without GL, accounts, or network.
#define ajustes_menu_topo biblioteca_up_menu_topo
#ifndef BIB_UP_SOURCE
#define BIB_UP_SOURCE "../src/biblioteca.c"
#endif
#include BIB_UP_SOURCE
#include <assert.h>

static int topMenu;
int biblioteca_up_menu_topo(void) { return topMenu; }

static void key(Uint32 type, SDL_Keycode code) {
  SDL_Event event = {0};
  event.type = type;
  event.key.keysym.sym = code;
  biblioteca_evento(&event);
}

static void sidebar(void) {
  assert(SDL_Init(SDL_INIT_VIDEO) == 0);
  SDL_Window *window = SDL_CreateWindow("Library navigation", 0, 0, 1920, 1080,
                                        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(window);
  SDL_GLContext context = SDL_GL_CreateContext(window);
  assert(context && gfx_iniciar() && txt_iniciar("deploy/app", 1));
  for (int phone = 0; phone < 2; phone++) {
    layout_modo_definir(phone);
    layout_tela_definir(phone ? 1080 : 1920, phone ? 1920 : 1080);
    gfx_tamanho_alvo(phone ? 1080 : 1920, phone ? 1920 : 1080);
    for (topMenu = 0; topMenu <= 1; topMenu++) {
      for (int tab = 0; tab < BIB_N_MODOS; tab++) {
        for (int sub = 0; sub < (tab == MODO_LISTAS ? 4 : 1); sub++) {
          for (int back = 0; back < 2; back++) {
            modo = tab; listaTab = sub; temAberta = 0;
            focoPainel = focoCont = estadoFoco = focoBotaoOrig = 0;
            origensAbertas = 1; animOrig = animTela = animVista = 1;
            sair = pediuRelogio = 0;
            // Enter through the same focus callback used by rendered rows.
            foco.fileira = BIB_FIL_GRADE; foco.coluna = 3;
            ponteiroOrigem(1, 0);
            biblioteca_desenhar(1000);
            assert(navigationCount > 0);
            key(SDL_KEYDOWN, SDLK_UP);
            assert(focoOrigem && origemSel == 0);
            biblioteca_desenhar(1000);
            key(SDL_KEYDOWN, SDLK_UP);
            if (focoOrigem || foco.fileira != BIB_FIL_MODO || foco.coluna != tab) {
              fprintf(stderr, "sidebar Up missed tabs: phone=%d tab=%d sub=%d row=%d col=%d\n",
                      phone, tab, sub, foco.fileira, foco.coluna);
              exit(1);
            }
            assert(!focoBotaoOrig && !sair && !pediuRelogio && origensAbertas);
            biblioteca_desenhar(1000);
            key(SDL_KEYDOWN, SDLK_UP);
            assert(sair == topMenu && biblioteca_pediu_relogio() == !topMenu);
            // The app's menu/clock consumes Down or Back and preserves page focus.
            biblioteca_retomar();
            assert(foco.fileira == BIB_FIL_MODO && foco.coluna == tab);
            biblioteca_desenhar(1000);
            key(SDL_KEYDOWN, back ? SDLK_AC_BACK : SDLK_DOWN);
            assert(focoOrigem && origemSel == 0 && origensAbertas);
            assert(modo == tab && listaTab == sub && !sair && !pediuRelogio);
            key(SDL_KEYDOWN, SDLK_DOWN);
            assert(focoOrigem && origemSel == 1);
          }
        }
      }
    }
  }
  txt_encerrar(); gfx_encerrar();
  SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
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
  sidebar();
  puts("Library Up: PASS (2.1 tabs, filters, list actions, Continue; top menu and sidebar)");
  return 0;
}
