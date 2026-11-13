/* Offline PGS display set: one indexed, opaque white rectangle. */
#ifndef NV_TEST_PGS_PACKET_H
#define NV_TEST_PGS_PACKET_H
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
static size_t pgs_packet(uint8_t *out, size_t capacity, unsigned w, unsigned h) {
  size_t n = 0;
  assert(w && w < 16384 && h && 11 + 6*h <= 65535);
  assert(capacity >= 3+19 + 3+7 + 3+11+6*h + 3);
#define PGS_BYTE(v) out[n++] = (uint8_t)(v)
#define PGS_WORD(v) do { PGS_BYTE((v)>>8); PGS_BYTE(v); } while (0)
  PGS_BYTE(0x16); PGS_WORD(19); /* presentation composition */
  PGS_WORD(w); PGS_WORD(h); PGS_BYTE(0x10); PGS_WORD(0);
  PGS_BYTE(0x80); PGS_BYTE(0); PGS_BYTE(0); PGS_BYTE(1);
  PGS_WORD(0); PGS_BYTE(0); PGS_BYTE(0); PGS_WORD(0); PGS_WORD(0);
  PGS_BYTE(0x14); PGS_WORD(7); /* palette */
  PGS_BYTE(0); PGS_BYTE(0); PGS_BYTE(1);
  PGS_BYTE(235); PGS_BYTE(128); PGS_BYTE(128); PGS_BYTE(255);
  PGS_BYTE(0x15); PGS_WORD(11+6*h); /* object with RLE rows */
  PGS_WORD(0); PGS_BYTE(0); PGS_BYTE(0xc0);
  PGS_BYTE((4+6*h)>>16); PGS_WORD(4+6*h); PGS_WORD(w); PGS_WORD(h);
  for (unsigned y=0; y<h; y++) {
    PGS_BYTE(0); PGS_BYTE(0xc0 | (w>>8)); PGS_BYTE(w); PGS_BYTE(1);
    PGS_BYTE(0); PGS_BYTE(0);
  }
  PGS_BYTE(0x80); PGS_WORD(0); /* end display set */
#undef PGS_WORD
#undef PGS_BYTE
  return n;
}
#endif
