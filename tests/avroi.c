// AVPlay source-crop bookkeeping: leaving zoom must restore the whole frame.
#include <stdio.h>
#include "../src/avroi.h"
static int bad;
#define EXPECT(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); bad = 1; } } while (0)
int main(void) {
  int o[4];
  // zoom crop is sent as is
  EXPECT(avroi_pedido(0, 100, 0, 1600, 1080, 1920, 1080, o) == 1 && o[0] == 100 && o[2] == 1600);
  // zoom off / PiP / plain destination after a crop: whole frame goes back
  EXPECT(avroi_pedido(1, 0, 0, 1920, 1080, 1920, 1080, o) == 2 && o[2] == 1920 && o[3] == 1080);
  EXPECT(avroi_pedido(1, 0, 0, 0, 0, 3840, 2160, o) == 2 && o[2] == 3840 && o[3] == 2160);
  // nothing to undo
  EXPECT(avroi_pedido(0, 0, 0, 0, 0, 1920, 1080, o) == 0);
  // frame size unknown: cannot restore yet, stays dirty for a later retry
  EXPECT(avroi_pedido(1, 0, 0, 0, 0, 0, 0, o) == 0);
  if (!bad) puts("PASS avroi");
  return bad;
}
