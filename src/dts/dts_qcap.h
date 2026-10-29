/* Admission check for the packet queues of dts_remux.c: a per-queue byte and
 * packet cap plus one byte budget shared by every generator (the TV has little
 * RAM, and a source whose video or audio never arrives would otherwise queue
 * the other stream without limit). Asked BEFORE the packet is allocated.
 * Header-only so the host test links it without FFmpeg. */
#ifndef NV_DTS_QCAP_H
#define NV_DTS_QCAP_H
#include <stdatomic.h>
#include <stddef.h>

enum { DTS_QCAP_OK = 0, DTS_QCAP_BYTES, DTS_QCAP_PKTS, DTS_QCAP_BUDGET };

static inline int dtsQcapAdmit(size_t qBytes, int qN, size_t add, size_t maxBytes, int maxN,
                               const atomic_size_t *total, size_t budget) {
  if (qBytes > maxBytes || add > maxBytes - qBytes) return DTS_QCAP_BYTES;
  if (qN >= maxN) return DTS_QCAP_PKTS;
  size_t used = atomic_load(total);
  if (used > budget || add > budget - used) return DTS_QCAP_BUDGET;
  return DTS_QCAP_OK;
}
#endif
