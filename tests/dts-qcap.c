// Host test of the packet-queue admission (src/dts/dts_qcap.h) used by
// dts_remux.c: per-queue bytes, per-queue packets and the budget shared by all
// generators are each refused BEFORE the packet is allocated.
#include "dts_qcap.h"
#include <stdio.h>

static int fails;
#define CHECK(c, m) do { if (c) printf("PASS %s\n", m); else { printf("FAIL %s\n", m); fails++; } } while (0)

int main(void) {
  atomic_size_t total;
  atomic_init(&total, 0);
  CHECK(dtsQcapAdmit(0, 0, 1000, 16u << 20, 16384, &total, 192u << 20) == DTS_QCAP_OK, "empty queue admits a packet");
  CHECK(dtsQcapAdmit(16u << 20, 100, 1, 16u << 20, 16384, &total, 192u << 20) == DTS_QCAP_BYTES, "queue at its byte cap refuses");
  CHECK(dtsQcapAdmit((16u << 20) - 10, 100, 10, 16u << 20, 16384, &total, 192u << 20) == DTS_QCAP_OK, "exactly at the cap still fits");
  CHECK(dtsQcapAdmit(1000, 16384, 100, 16u << 20, 16384, &total, 192u << 20) == DTS_QCAP_PKTS, "queue at its packet cap refuses");
  atomic_store(&total, (size_t)(128u << 20) - 500);
  CHECK(dtsQcapAdmit(0, 0, 1000, 16u << 20, 16384, &total, (size_t)128u << 20) == DTS_QCAP_BUDGET, "shared budget exhausted refuses");
  CHECK(dtsQcapAdmit(0, 0, 400, 16u << 20, 16384, &total, (size_t)128u << 20) == DTS_QCAP_OK, "shared budget with room admits");
  // a huge packet cannot slip past by overflowing the sum
  CHECK(dtsQcapAdmit(0, 0, (size_t)-1 - 5, 16u << 20, 16384, &total, (size_t)128u << 20) != DTS_QCAP_OK, "size_t overflow does not admit");
  printf(fails ? "RESULT: %d FAIL\n" : "RESULT: all PASS\n", fails);
  return fails != 0;
}
