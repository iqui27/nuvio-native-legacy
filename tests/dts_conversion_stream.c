/* Conversion over one continuous request (test option, webOS buffer
 * settings) against a real HTTP origin: the same frames, timestamps and seeks
 * as the range reader, with a handful of requests instead of one per range. */
#include "../src/dts/dts_engine.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
static const char *HEADERS = "X-Fixture: yes";
static void check(DtsEngine *e, int r) { if (r < 0) { fprintf(stderr, "%s\n", dts_engine_error(e)); exit(1); } }
static void frame(FILE *out, const DtsFrame *f) {
  assert(fwrite(&f->kind, sizeof f->kind, 1, out) == 1 && fwrite(&f->size, sizeof f->size, 1, out) == 1);
  assert(fwrite(&f->pts_ns, sizeof f->pts_ns, 1, out) == 1 && fwrite(f->data, 1, f->size, out) == f->size);
}
static unsigned long long run(const char *url, const char *path, int stream) {
  FILE *out = fopen(path, "wb"); assert(out);
  dts_engine_set_conversion_stream(stream);
  dts_engine_set_ahead_mib(16);
  DtsEngine *e = dts_engine_create(); assert(e);
  check(e, dts_engine_open(e, url, HEADERS, -1, 0, 0));
  DtsFrame f; int r, video = 0;
  while ((r = dts_engine_next(e, &f)) > 0) { frame(out, &f); video += f.kind == DTS_VIDEO; }
  check(e, r); assert(video > 400);
  const double targets[] = { 12.0, 1.5, 17.0, 6.0 };
  for (unsigned i = 0; i < sizeof targets / sizeof *targets; i++) {
    check(e, dts_engine_seek(e, targets[i]));
    for (int n = 0; n < 150 && (r = dts_engine_next(e, &f)) > 0; n++) frame(out, &f);
    check(e, r);
  }
  /* Track change keeps the demuxer and the stream. */
  check(e, dts_engine_audio(e, dts_engine_info(e)->audio_stream, 0, 9.0));
  for (int n = 0; n < 150 && (r = dts_engine_next(e, &f)) > 0; n++) frame(out, &f);
  check(e, r);
  DtsEngineMetrics m; dts_engine_metrics(e, &m);
  DtsBufferInfo b; dts_engine_buffer_info(e, &b);
  if (stream) assert(b.http_workers == 1 && b.capacity_bytes == 16 * 1024 * 1024);
  dts_engine_destroy(e); assert(!fclose(out));
  return (unsigned long long)m.range_requests;
}
static void *cancel_later(void *e) { usleep(200000); dts_engine_cancel(e); return NULL; }
int main(int argc, char **argv) {
  assert(argc == 5);
  unsigned long long ranges = run(argv[1], argv[2], 0), streams = run(argv[1], argv[3], 1);
  printf("[stream-test] rangeRequests=%llu streamRequests=%llu\n", ranges, streams);
  printf("[stream-test] ranges=%llu streams=%llu\n", ranges, streams); fflush(stdout);
  assert(streams > 1 && streams * 3 < ranges);   /* seeks outside the ring restart the one request */
  /* A refused source fails at once; a source that never answers is cancelled. */
  dts_engine_set_conversion_stream(1);
  DtsEngine *e = dts_engine_create();
  struct timespec t0, t1; clock_gettime(CLOCK_MONOTONIC, &t0);
  assert(dts_engine_open(e, argv[4], HEADERS, -1, 0, 0) == -1 && strstr(dts_engine_error(e), "Source probe"));
  dts_engine_destroy(e);
  char hold[1024]; snprintf(hold, sizeof hold, "%s", argv[1]);
  char *slash = strrchr(hold, '/'); assert(slash); strcpy(slash, "/hold/x.mkv");
  e = dts_engine_create(); pthread_t th; assert(!pthread_create(&th, NULL, cancel_later, e));
  assert(dts_engine_open(e, hold, HEADERS, -1, 0, 0) == -1); pthread_join(th, NULL);
  clock_gettime(CLOCK_MONOTONIC, &t1);
  assert((t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9 < 3.0);
  dts_engine_destroy(e);
  puts("PASS conversion stream: identical frames/seeks/track change vs ranges, few requests, refused and cancelled sources");
  return 0;
}
