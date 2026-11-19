#include "dts/dts_hls_tpk.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  char url[128];
  if (argc != 3 && argc != 4 && argc != 8) return 1;
  DtsWebosHls options = { .dv7 = atoi(argv[2]), .allow_dts = !atoi(argv[2]) };
  if (argc == 4) options.buffer_mib = atoi(argv[3]);
  if (argc == 8) {
    options.external_url = argv[3];
    options.buffer_mib = atoi(argv[4]);
    options.external_buffer_mib = 16;
    options.external_stream = atoi(argv[5]);
    options.external_offset_ms = atoi(argv[6]);
    options.external_speed_ppm = atoi(argv[7]);
    options.external_only = 1;
  }
  if (!dts_hls_webos_open(argv[1], NULL, options.dv7 ? -1 : 0, &options, url, sizeof url)) return 1;
  printf("playlist %s\n", url);
  fflush(stdout);
  getchar();
  DtsBufferInfo buffer;
  dts_hls_webos_buffer_info(&buffer);
  printf("input buffer: %" PRId64 " / %" PRId64 " bytes; %d readers; %" PRIu64 " bytes received\n",
         buffer.ready_bytes, buffer.capacity_bytes, buffer.http_workers, buffer.received_bytes);
  DtsTrack audio;
  dts_hls_webos_audio_info(&audio);
  printf("input audio metadata: language=%s title=%s\n", audio.language, audio.title);
  dts_tpk_fechar();
  return 0;
}
