/* Exercise the real remux queues without allocating large packet payloads. */
#define NV_TPK
#define NV_DTS_FFMPEG
#include "../src/dts/dts_remux.c"
#include <assert.h>

static AVPacket *packet(int size) {
  AVPacket *p = av_packet_alloc();
  assert(p);
  p->size = size;
  return p;
}
int main(void) {
  Q a = {0}, b = {0};
  AVPacket *r = packet(193 << 20);
  assert(qPush(&b, r, 0, 1) < 0);
  r->size = 1;
  dts_remux_set_queue_budget_mib(128);
  assert(qPush(&a, packet(64 << 20), 0, 1) == 0);
  assert(qPush(&b, packet(64 << 20), 0, 1) == 0);
  assert(qPush(&b, r, 0, 1) < 0);
  assert(b.bytes == (64u << 20));
  qClear(&a); qClear(&b);
  assert(atomic_load(&gQueued) == 0);
  b.n = VQ_MAX_PKTS;
  r->size = 0;
  assert(qPush(&b, r, 0, 1) < 0);
  b.n = 0;
  r->size = 17 << 20;
  assert(qPush(&b, r, 0, 0) < 0);
  assert(b.n == 0 && atomic_load(&gQueued) == 0);
  av_packet_free(&r);
  free(a.v); free(b.v);
  puts("PASS shared queue budget, pre-enqueue byte/packet caps, cleanup");
  return 0;
}
