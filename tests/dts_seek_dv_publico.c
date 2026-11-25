/* Regressao F92 (PR 749): o seek DV pela API publica dts_engine_seek() nao pode
 * esvaziar a fila de ranges. dts_engine_seek zerava range_live antes de
 * avformat_seek_file() chegar em seek_range(), entao a guarda de reuso
 * (dv && !remux && range_live) nunca era alcancada e bytes ja na fila iam para
 * um GET novo.
 *
 * avformat_seek_file e embrulhado (--wrap) por um deslocamento AVIO de um byte
 * dentro do range disponivel — o mesmo posicionamento que o seek real faz ao
 * buscar o keyframe.
 *
 * Modos (argv[1]): serial e remux cancelam como antes (controle, < 2.5 s);
 * dv-live e o posicionamento AVIO direto que ja reusava; dv-public e a API
 * publica: range_live=1 retained=1 cancelled=0 na ponta, 0/0/1 no pai.
 */
#define NV_DTS_FFMPEG
#ifndef REVIEW_SERIAL
#define NV_DTS_WEBOS_RECOVERY
#endif
#include "dts/dts_engine.c"
#include <assert.h>
#include <unistd.h>

enum { BLOCK = 1024, TARGET = 4 * BLOCK };

static int64_t seek_offset;

int __wrap_avformat_seek_file(AVFormatContext *fmt, int stream, int64_t minimum,
                              int64_t target, int64_t maximum, int flags) {
  (void)stream; (void)minimum; (void)target; (void)maximum; (void)flags;
  return fmt->pb->seek(fmt->pb->opaque, seek_offset, SEEK_SET) == seek_offset ? 0 : -1;
}

static DtsRangeJob *find(DtsEngine *e, int64_t pos) {
  for (int i = 0; i < e->range_slots; i++)
    if (e->ranges[i].state && e->ranges[i].start == pos) return &e->ranges[i];
  return NULL;
}

static void wait_job(DtsEngine *e, int64_t pos, int state) {
  uint64_t limit = clock_ns(CLOCK_MONOTONIC) + UINT64_C(2000000000);
  do {
    pthread_mutex_lock(&e->range_lock);
    DtsRangeJob *job = find(e, pos);
    int ready = job && job->state == state;
    pthread_mutex_unlock(&e->range_lock);
    if (ready) return;
    usleep(1000);
  } while (clock_ns(CLOCK_MONOTONIC) < limit);
  assert(!"range never reached expected state");
}

int main(int argc, char **argv) {
  assert(argc == 3);
  const char *mode = argv[1];
  int serial = !strcmp(mode, "serial");
  int remux = !strcmp(mode, "remux");
  int live_dv = !strcmp(mode, "dv-live");
  int public_seek = !strcmp(mode, "dv-public");
  DtsEngine *e = dts_engine_create();
  assert(e);
  e->url = strdup(argv[2]);
  e->headers = strdup("Authorization: Bearer engine-test\nX-Fixture: yes");
  e->range_bytes = BLOCK;
  e->range_slots = serial ? 8 : 2;
  e->http_workers = serial ? 1 : 2;
  e->range_live = !serial;
  e->dv = remux || live_dv || public_seek;
  e->remux = remux;
  assert(start_ranges(e));
  pthread_mutex_lock(&e->range_lock);
  queue_ranges(e);
  pthread_mutex_unlock(&e->range_lock);
  wait_job(e, serial ? 0 : BLOCK, 2);
  if (!serial) wait_job(e, 0, 3);
  usleep(200000);
  int64_t offset = serial ? TARGET : live_dv || public_seek ? 1 : 0;
  uint64_t begin = clock_ns(CLOCK_MONOTONIC);
  if (public_seek) {
    unsigned char *buf = av_malloc(32);
    e->fmt = avformat_alloc_context();
    e->in = av_packet_alloc();
    e->out = av_packet_alloc();
    assert(buf && e->fmt && e->in && e->out);
    e->fmt->pb = avio_alloc_context(buf, 32, 0, e, read_range, NULL, seek_range);
    assert(e->fmt->pb);
    // Account for FFmpeg's ownership separately on this synthetic context.
    e->passthrough = 1;
    seek_offset = offset;
    assert(dts_engine_seek(e, 0) == 0);
    av_freep(&e->fmt->pb->buffer);
    avio_context_free(&e->fmt->pb);
  } else {
    assert(seek_range(e, offset, SEEK_SET) == offset);
  }
  pthread_mutex_lock(&e->range_lock);
  DtsRangeJob *old_ready = find(e, 0), *speculative = find(e, BLOCK);
  int kept = old_ready && old_ready->state == 3 && old_ready->data;
  int obsolete_cancelled = !speculative || speculative->state == 0 ||
                           __atomic_load_n(&speculative->cancel, __ATOMIC_RELAXED);
  pthread_mutex_unlock(&e->range_lock);
  unsigned char byte;
  while (e->pos <= (serial ? TARGET : live_dv || public_seek ? 1 : BLOCK)) {
    int64_t at = e->pos;
    assert(read_range(e, &byte, 1) == 1);
    assert(byte == (unsigned char)(at % 251));
  }
  double elapsed_ms = (clock_ns(CLOCK_MONOTONIC) - begin) / 1e6;
  printf("%s elapsed_ms=%.1f retained=%d cancelled=%d range_live=%d workers=%d slots=%d\n",
         mode, elapsed_ms, kept, obsolete_cancelled, e->range_live, e->range_threads, e->range_slots);
  fflush(stdout);
  dts_engine_destroy(e);
  if (live_dv || public_seek) return kept && !obsolete_cancelled ? 0 : 1;
  return elapsed_ms < 2500 && obsolete_cancelled ? 0 : 1;
}
