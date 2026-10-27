#include "../src/detail.c"
#include <assert.h>

void row_navigation_detail(void) {
  nivel = 1;
  foco.fileira = SEC_RELACIONADOS;
  foco.coluna = 0;
  navigationCount = 0;
  navigationTarget(100, 50, 200, 50, ponteiroDetalhe, NULL, -1, 0);
  navigationTarget(100, 300, 150, 200, ponteiroDetalhe, NULL, SEC_RELACIONADOS, 0);
  assert(!navigationMove(SDLK_UP));
  // Horizontal season movement must reach every control before leaving its row.
  foco.fileira = SEC_TEMPORADAS;
  assert(!navigationMove(SDLK_RIGHT));
  const int *order = ordemSecoes();
  int cols[N_SECOES] = {0};
  for (int r = 0; r < 6; r++) cols[order[r]] = 4;
  focus_iniciar(&foco, N_SECOES, cols);
  foco.fileira = order[0];
  for (int r = 0; r < 6; r++) foco.colunaLembrada[order[r]] = r % 4;
  for (int step = 0; step < 11; step++) {
    assert(moverFileira(step < 5 || step == 10 ? 1 : -1));
    int row = step < 5 ? step + 1 : step < 10 ? 9 - step : 1;
    assert(foco.fileira == order[row] && foco.coluna == row % 4);
  }
}
