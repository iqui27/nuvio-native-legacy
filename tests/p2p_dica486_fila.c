// Exercise the real worker queue before publishing on the UI thread.
#include "../src/addons.c"
#include <assert.h>
void *p2p486_enfileirar(void *a) {
  progMarcar(0, a, 3, 2);
  return NULL;
}
void p2p486_publicar(void) {
  assert(progN == 1);
  stream_lista_acrescentar(progFila[0].a, progFila[0].n, progFila[0].idx);
  free(progFila[0].a);
  progN = 0;
}
