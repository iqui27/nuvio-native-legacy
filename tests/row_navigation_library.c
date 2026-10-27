#include "../src/biblioteca.c"
#include <assert.h>

void row_navigation_library(void) {
  int cols[8] = {1,1,1,1,1,1,1,1};
  focus_iniciar(&foco, 8, cols);
  foco.fileira = BIB_FIL_GRADE + 5;
  focoOrigem = focoPainel = focoCont = estadoFoco = 0;
  // A clipped preceding row must not redirect Up into the header.
  navigationCount = 0;
  navigationTarget(100, 50, 200, 50, ponteiroModo, NULL, 0, 0);
  navigationTarget(100, 300, 150, 200, ponteiroCelula, NULL,
                   (foco.fileira - gradeIni()) * colunas(), 0);
  assert(!navigationMove(SDLK_UP));
  // Exercise both Saved and open-list rows through the public key handler.
  for (int list = 0; list < 2; list++) {
    temAberta = trilha = list;
    nCelulas = 56;
    int first = gradeIni();
    focus_iniciar(&foco, 8, cols);
    foco.fileira = first;
    for (int step = 0; step < 11; step++) {
      navigationCount = 0;
      navigationTarget(100, 50, 200, 50, ponteiroModo, NULL, 0, 0);
      navigationTarget(100, 300, 150, 200, ponteiroCelula, NULL,
                       (foco.fileira - first) * colunas(), 0);
      SDL_Event e = {0};
      e.type = SDL_KEYDOWN;
      e.key.keysym.sym = step < 5 || step == 10 ? SDLK_DOWN : SDLK_UP;
      biblioteca_evento(&e);
      int row = step < 5 ? step + 1 : step < 10 ? 9 - step : 1;
      assert(foco.fileira == first + row);
    }
  }
  temAberta = trilha = 0;
}
