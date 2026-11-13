/* Exercise the actual webOS reporter with the existing native bus fixture,
 * built with the release macros (no NV_HTTP_BUFFER_DIAG): review C-R2/C-R4. */
#define main video_acb_fixture_main
#include "video_acb_lg.c"
#undef main
int main(void) {
  char text[192]; // The exact player OSD capacity (player.c bufferDadosDesenhar).
  // Root regression first: old formatter has neither byte range nor time
  // ahead, even when diagnostics are enabled and both measurements exist.
  movie_active=1;posSeg=100;bufferSeg=142;
  movie_snapshot=(DtsBufferInfo){.capacity_bytes=256*1048576LL,
    .position=812*1048576LL,.contiguous_bytes=128*1048576LL,.total=4120*1048576LL,.http_workers=1};
  video_buffer_test_text(text,sizeof text);
  assert(strstr(text,"812–940 de 4120 MiB") && "missing measured byte range");
  assert(strstr(text,"À frente: 42 s") && "missing measured time ahead");
  posSeg=bufferSeg=0;
  test_buffer_preset = 3;
  movie_active = 1;
  movie_snapshot = (DtsBufferInfo){.ready_bytes=84*1048576LL+524288,
                                  .capacity_bytes=128*1048576LL,.http_workers=1};
  video_buffer_test_text(text, sizeof text);
  assert(!strcmp(text, "Buffer: 84.5 / 128 MiB no total (256 MiB escolhidos) · Trecho: indisponível · À frente: indisponível · HTTP: 1 (Nativo)"));
  for (int cap = 64; cap <= 256; cap *= 2) {
    movie_snapshot.capacity_bytes = (int64_t)cap * 1048576;
    movie_snapshot.ready_bytes = movie_snapshot.capacity_bytes;
    video_buffer_test_text(text, sizeof text);
    char expected[192];
    snprintf(expected, sizeof expected, "Buffer: %.1f / %d MiB no total%s · Trecho: indisponível · À frente: indisponível · HTTP: 1 (Nativo)", (double)cap, cap,
             cap == 256 ? "" : " (256 MiB escolhidos)");
    assert(!strcmp(text, expected));
  }
  for(int workers=1;workers<=4;workers++) {
    movie_snapshot.http_workers=workers;video_buffer_test_text(text,sizeof text);
    char suffix[40];snprintf(suffix,sizeof suffix,"HTTP: %d (Nativo)",workers);
    assert(strstr(text,suffix)); // Snapshot wins over the current getter (1).
  }
  movie_snapshot = (DtsBufferInfo){0};
  video_buffer_test_text(text, sizeof text);assert(!strcmp(text, "Buffer: sistema · Trecho: indisponível · À frente: indisponível"));
  movie_snapshot = (DtsBufferInfo){.ready_bytes=20*1048576LL,.capacity_bytes=64*1048576LL,.http_workers=1};
  movie_active = -1;
  video_buffer_test_text(text, sizeof text);assert(!strcmp(text, "Buffer: sistema · Trecho: indisponível · À frente: indisponível"));
  movie_active = 0;
  video_buffer_test_text(text, sizeof text);assert(!strcmp(text, "Buffer: sistema · Trecho: indisponível · À frente: indisponível"));
  movie_active = 1;dtsSessao = (DtsPlayback *)(uintptr_t)1;
  pump_status=(DtsPlaybackStatus){.prepared=1,
    .buffer={.ready_bytes=12*1048576LL,.capacity_bytes=132*1048576LL,
             .total=100*1048576LL,.contiguous_bytes=6*1048576LL,.http_workers=1},.buffered_until=14};
  video_buffer_test_text(text, sizeof text);
  assert(!strcmp(text, "Buffer: 12.0 / 132 MiB no total (256 MiB escolhidos) · Trecho: 0–6 de 100 MiB · À frente: 14 s · HTTP: 1 (DTS/DV)"));
  for(int workers=1;workers<=4;workers++) {
    pump_status.buffer.http_workers=workers;video_buffer_test_text(text,sizeof text);
    char suffix[40];snprintf(suffix,sizeof suffix,"HTTP: %d (DTS/DV)",workers);
    assert(strstr(text,suffix)); // Snapshot wins over the current getter (4).
  }
  posSeg=10;durSeg=100;bufferSeg=0;
  assert(fabs(video_buffer_fim()-16)<.001); // Stop at a gap, not all 12 MiB ready.
  pump_status.buffered_until=25;
  assert(fabs(video_buffer_fim()-25)<.001); // Frames already fed to native playback.
  pump_status.buffer.contiguous_bytes=100*1048576LL;
  assert(video_buffer_fim()==100); // File-duration clamp.
  bufferandoDesde=1;pausaPedida=0;pump_status.buffering_ms=7300;
  bombearDts();assert(dtsRefillMs==7300);
  assert(video_bufferando_ms()>=7300 && video_bufferando_ms()<7400);
  dtsRefillEm-=1000;
  assert(video_bufferando_ms()>=8300); // The UI timer advances during a blocked HTTP read.
  pausaPedida=1;assert(!video_bufferando_ms());
  pausaPedida=0;pump_status.buffering_ms=0;
  bombearDts();assert(!dtsRefillMs && !bufferandoDesde);
  assert(!video_bufferando_ms()); // Refill completion survives retired callback queues.
  bufferandoDesde=1;pausaPedida=0;pump_status.buffering_ms=7300;
  bombearDts();assert(dtsRefillMs==7300);
  assert(video_bufferando_ms()>=7300 && video_bufferando_ms()<7400);
  dtsRefillEm-=1000;
  assert(video_bufferando_ms()>=8300); // The UI timer advances during a blocked HTTP read.
  pausaPedida=1;assert(!video_bufferando_ms());
  pausaPedida=0;pump_status.buffering_ms=0;
  bombearDts();assert(!dtsRefillMs && !bufferandoDesde);
  assert(!video_bufferando_ms()); // Refill completion survives retired callback queues.
  pump_status.failed=1;
  video_buffer_test_text(text, sizeof text);assert(!strcmp(text, "Buffer: sistema · Trecho: indisponível · À frente: indisponível"));
  assert(video_buffer_fim()==0);
  pump_status=(DtsPlaybackStatus){0};
  dtsSessao = NULL;test_buffer_preset = 1;
  video_buffer_test_text(text, sizeof text);assert(!strcmp(text, "Buffer: 20.0 / 64 MiB no total · Trecho: indisponível · À frente: indisponível · HTTP: 1 (Nativo)"));
  movie_snapshot.ready_bytes = -100;
  video_buffer_test_text(text, sizeof text);assert(!strcmp(text, "Buffer: 0.0 / 64 MiB no total · Trecho: indisponível · À frente: indisponível · HTTP: 1 (Nativo)"));
  movie_snapshot.ready_bytes = 100*1048576LL;
  video_buffer_test_text(text, sizeof text);assert(!strcmp(text, "Buffer: 64.0 / 64 MiB no total · Trecho: indisponível · À frente: indisponível · HTTP: 1 (Nativo)"));
  // Range and time ahead come only from measurements (C-R4): the native ring's
  // contiguous window and the uMS bufferRange; no bitrate estimate.
  movie_snapshot = (DtsBufferInfo){.ready_bytes=128*1048576LL,.capacity_bytes=128*1048576LL,
    .position=812*1048576LL,.contiguous_bytes=128*1048576LL,.total=4120*1048576LL,.http_workers=2};
  test_buffer_preset = 2;posSeg=10;bufferSeg=52;
  video_buffer_test_text(text, sizeof text);
  assert(!strcmp(text, "Buffer: 128.0 / 128 MiB no total · Trecho: 812–940 de 4120 MiB · À frente: 42 s · HTTP: 2 (Nativo)"));
  movie_snapshot.total=0;movie_snapshot.position=0;bufferSeg=8; // behind the playhead: nothing ahead
  video_buffer_test_text(text, sizeof text);
  assert(!strcmp(text, "Buffer: 128.0 / 128 MiB no total · Trecho: 0–128 MiB · À frente: 0 s · HTTP: 2 (Nativo)"));
  movie_snapshot.position=4120*1048576LL;movie_snapshot.total=4120*1048576LL; // past EOF: no range
  video_buffer_test_text(text, sizeof text);assert(strstr(text, " · Trecho: indisponível · "));
  movie_active = 0;bufferSeg=40; // TV's own buffer: capacity unknown, time ahead still measured
  video_buffer_test_text(text, sizeof text);
  assert(!strcmp(text, "Buffer: sistema · Trecho: indisponível · À frente: 30 s"));
  bufferSeg=0;
  movie_active = 0;
  char tiny[2] = {1,1};video_buffer_test_text(tiny, sizeof tiny);assert(tiny[1] == 0);
  video_buffer_test_text(NULL, 0);
  ligado = 0;midia[0] = 0;acb = 0;pararSessao();
  puts("video_buffer_test_osd: native and SDR/DTS counters, contiguous rail, fed timestamps, fallback and reset passed");
}
