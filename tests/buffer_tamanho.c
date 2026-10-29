// Buffer requests retain the 100 MiB floor plus DTS's active 4 MiB range.
#include "../src/cacheboost.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
  const long available[] = {300, 250, 150, -1};
  const int expected[] = {128, 128, 0, 0};
  for (size_t i = 0; i < sizeof available / sizeof available[0]; i++) {
    assert(cacheboost_buffer_mb(2, available[i]) == expected[i]);
    assert(cacheboost_buffer_mb(3, available[i]) == expected[i]);
    assert(cacheboost_buffer_mb(0, available[i]) == 0);
  }
  assert(CB_BUFFER_PISO_MB == 100);
  for (long freeMb = 0; freeMb <= 500; freeMb++) {
    for (int option = 1; option <= 3; option++) {
      int mb = cacheboost_buffer_mb(option, freeMb);
      assert(!mb || freeMb - mb - 4 >= 100);
    }
  }
  assert(cacheboost_buffer_mb(3, 359) == 128);
  assert(cacheboost_buffer_mb(3, 360) == 256);
  assert(cacheboost_buffer_mb(2, 231) == 64);
  assert(cacheboost_buffer_mb(2, 232) == 128);
  assert(cacheboost_buffer_mb(1, 167) == 0);
  assert(cacheboost_buffer_mb(1, 168) == 64);
  assert(cacheboost_buffer_mb(4, 4096) == 0);
  assert(cacheboost_buffer_mb(-1, 4096) == 0);
  puts("buffer_tamanho: ok");
  return 0;
}
