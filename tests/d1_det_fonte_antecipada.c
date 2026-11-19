// #457: espelha toggle da linha "Fonte preparada" na ficha.
#include <assert.h>
enum { AJ_DET_FONTE_ANTECIP = 126 };
static int valor[256];
static int ajustes_det_fonte_antecipada(void) { return !valor[AJ_DET_FONTE_ANTECIP]; }
int main(void) {
  valor[AJ_DET_FONTE_ANTECIP] = 1;
  assert(!ajustes_det_fonte_antecipada());
  valor[AJ_DET_FONTE_ANTECIP] = 0;
  assert(ajustes_det_fonte_antecipada());
  puts("d1_det_fonte_antecipada: ok");
  return 0;
}
