/* Model the real interval between collector publication and UI handoff. */
#define NV_TPK
#include "../src/faixas.c"
void test_primary_native_wait(int waiting) {
  legOverlay = waiting ? 0 : -1;
  legOverlayTV = waiting;
}
