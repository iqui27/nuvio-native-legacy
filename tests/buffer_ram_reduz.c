// Review C-R3: the "reduce buffer on low memory" setting only chooses between
// stepping down the 256/128/64 ladder (ON) and keeping the chosen size or
// falling back to the TV's Automatic buffer (OFF). In BOTH modes the result is
// 0 or a ladder step <= the request, and a nonzero result always leaves
// CB_BUFFER_PISO_MB free after the buffer and DTS's active 4 MiB range.
#include "../src/cacheboost.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

static int reduz = 1;
int ajustes_buffer_ram_reduz(void) { return reduz; }

int main(void) {
  for (reduz = 0; reduz <= 1; reduz++) {
    for (int option = -1; option <= 4; option++) {
      int pedido = option >= 1 && option <= 3 ? 32 << option : 0;
      const long probes[] = {-1, LONG_MIN, 0, 99, 100, 150, 167, 168, 200, 231, 232,
                             359, 360, 4096, LONG_MAX};
      for (size_t i = 0; i < sizeof probes / sizeof probes[0]; i++) {
        long freeMb = probes[i];
        int mb = cacheboost_buffer_mb(option, freeMb);
        assert(mb == 0 || mb == 64 || mb == 128 || mb == 256);
        assert(mb <= pedido);                                 // never grows
        assert(!mb || (freeMb >= 0 && freeMb - mb - 4 >= CB_BUFFER_PISO_MB));
        if (freeMb < 0) assert(mb == 0);                      // unknown RAM -> Auto
        if (!reduz) assert(mb == 0 || mb == pedido);          // OFF never rescales
      }
      for (long freeMb = 0; freeMb <= 600; freeMb++) {
        int mb = cacheboost_buffer_mb(option, freeMb);
        assert(!mb || freeMb - mb - 4 >= CB_BUFFER_PISO_MB);
      }
    }
  }
  reduz = 0;
  // The reported case: 200 MiB free, 256 MiB requested, setting OFF.
  int off = cacheboost_buffer_mb(3, 200);
  printf("OFF 256 MiB @ 200 MiB free -> %d MiB\n", off);
  assert(off == 0);
  assert(cacheboost_buffer_mb(3, 360) == 256);   // fits: explicit budget kept
  assert(cacheboost_buffer_mb(3, 359) == 0);     // boundary: one MiB short
  assert(cacheboost_buffer_mb(1, 168) == 64);    // small budget, exact fit
  assert(cacheboost_buffer_mb(1, 167) == 0);
  assert(cacheboost_buffer_mb(2, 300) == 128);
  reduz = 1;
  assert(cacheboost_buffer_mb(3, 200) == 64);    // ON: 128 needs 232, 64 needs 168
  assert(cacheboost_buffer_mb(3, 250) == 128);   // ON steps down the ladder
  assert(cacheboost_buffer_mb(3, 359) == 128);
  assert(cacheboost_buffer_mb(3, 180) == 64);
  puts("buffer_ram_reduz: ok (floor+4 MiB enforced ON and OFF; OFF keeps the chosen size or Auto)");
  return 0;
}
