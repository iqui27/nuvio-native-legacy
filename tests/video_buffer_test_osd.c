/* Exercise the actual webOS reporter with the existing native bus fixture. */
#define NV_HTTP_BUFFER_DIAG 1
#define main video_acb_fixture_main
#include "video_acb_lg.c"
#undef main
int main(void) {
  char text[192];
  test_buffer_preset = 3;
  movie_active = 1;
  movie_snapshot = (DtsBufferInfo){.ready_bytes=84*1048576LL+524288,
                                  .capacity_bytes=128*1048576LL};
  video_buffer_test_text(text, sizeof text);
  assert(!strcmp(text, "Buffer: 84.5 / 128 MiB total (256 MiB selected)"));
  for (int cap = 64; cap <= 256; cap *= 2) {
    movie_snapshot.capacity_bytes = (int64_t)cap * 1048576;
    movie_snapshot.ready_bytes = movie_snapshot.capacity_bytes;
    video_buffer_test_text(text, sizeof text);
    char expected[192];
    snprintf(expected, sizeof expected, "Buffer: %.1f / %d MiB total%s", (double)cap, cap,
             cap == 256 ? "" : " (256 MiB selected)");
    assert(!strcmp(text, expected));
  }
  movie_snapshot = (DtsBufferInfo){0};
  video_buffer_test_text(text, sizeof text);assert(!strcmp(text, "Buffer: system"));
  movie_snapshot = (DtsBufferInfo){.ready_bytes=20*1048576LL,.capacity_bytes=64*1048576LL};
  movie_active = -1;
  video_buffer_test_text(text, sizeof text);assert(!strcmp(text, "Buffer: system"));
  movie_active = 0;
  video_buffer_test_text(text, sizeof text);assert(!strcmp(text, "Buffer: system"));
  movie_active = 1;dtsSessao = (DtsPlayback *)(uintptr_t)1;
  pump_status=(DtsPlaybackStatus){.prepared=1,
    .buffer={.ready_bytes=12*1048576LL,.capacity_bytes=132*1048576LL,
             .total=100*1048576LL,.contiguous_bytes=6*1048576LL},.buffered_until=14};
  video_buffer_test_text(text, sizeof text);
  assert(!strcmp(text, "Buffer: 12.0 / 132 MiB total (256 MiB selected)"));
  posSeg=10;durSeg=100;bufferSeg=0;
  assert(fabs(video_buffer_fim()-16)<.001); // Stop at a gap, not all 12 MiB ready.
  pump_status.buffered_until=25;
  assert(fabs(video_buffer_fim()-25)<.001); // Frames already fed to native playback.
  pump_status.buffer.contiguous_bytes=100*1048576LL;
  assert(video_buffer_fim()==100); // File-duration clamp.
  pump_status.failed=1;
  video_buffer_test_text(text, sizeof text);assert(!strcmp(text, "Buffer: system"));
  assert(video_buffer_fim()==0);
  pump_status=(DtsPlaybackStatus){0};
  dtsSessao = NULL;test_buffer_preset = 1;
  video_buffer_test_text(text, sizeof text);assert(!strcmp(text, "Buffer: 20.0 / 64 MiB total"));
  movie_snapshot.ready_bytes = -100;
  video_buffer_test_text(text, sizeof text);assert(!strcmp(text, "Buffer: 0.0 / 64 MiB total"));
  movie_snapshot.ready_bytes = 100*1048576LL;
  video_buffer_test_text(text, sizeof text);assert(!strcmp(text, "Buffer: 64.0 / 64 MiB total"));
  movie_active = 0;
  char tiny[2] = {1,1};video_buffer_test_text(tiny, sizeof tiny);assert(tiny[1] == 0);
  video_buffer_test_text(NULL, 0);
  ligado = 0;midia[0] = 0;acb = 0;pararSessao();
  puts("video_buffer_test_osd: native and SDR/DTS counters, contiguous rail, fed timestamps, fallback and reset passed");
}
