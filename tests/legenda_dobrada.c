/* Native fallback must own the primary until the collector handoff, even
 * when a worker has already published cues and then failed before UI polling. */
#include "../src/faixas.c"
#include <assert.h>

int main(void) {
  legOverlay = 0;
  legOverlayTV = 1;
  assert(!faixas_legenda_overlay());
  /* First cues acknowledged: native captions disabled, app owns primary. */
  legOverlayTV = 0;
  assert(faixas_legenda_overlay());
  /* Network fallback and retry publication must not draw over the TV. */
  legOverlayTV = 1;
  legOverlayFalhas = 1;
  assert(!faixas_legenda_overlay());
  /* External, ASS, native callback and disabled selections keep their paths. */
  legOverlay = -1;
  legOverlayTV = 0;
  assert(faixas_legenda_overlay());
  puts("primary subtitle ownership: native wait/fallback and app handoff OK");
}
