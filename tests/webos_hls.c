#include "dts/dts_hls_tpk.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  char url[128];
  if (argc != 3 && argc != 4 && argc != 5 && argc != 8) return 1;
  DtsWebosHls options = { .dv7 = atoi(argv[2]), .allow_dts = !atoi(argv[2]) };
  const char *dts = getenv("NV_TEST_HLS_DTS");
  if (dts && atoi(dts)) options.allow_dts = 1;
  const char *mapping = getenv("NV_TEST_P7_MAPPING");
  if (mapping) options.preserve_mapping = atoi(mapping);
  if (argc == 4 || argc == 5) options.buffer_mib = atoi(argv[3]);
  if (argc == 8) {
    options.external_url = argv[3];
    options.buffer_mib = atoi(argv[4]);
    options.external_buffer_mib = 16;
    options.external_stream = atoi(argv[5]);
    options.external_offset_ms = atoi(argv[6]);
    options.external_speed_ppm = atoi(argv[7]);
    options.external_only = 1;
  }
  int ordinal = argc == 5 ? atoi(argv[4]) : options.dv7 ? -1 : 0;
  if (!dts_hls_webos_open(argv[1], NULL, ordinal, &options, url, sizeof url)) return 1;
  printf("playlist %s\n", url);
  fflush(stdout);
  getchar();
  DtsBufferInfo buffer;
  dts_hls_webos_buffer_info(&buffer);
  printf("input buffer: %" PRId64 " / %" PRId64 " bytes; %d readers; %" PRIu64 " bytes received\n",
         buffer.ready_bytes, buffer.capacity_bytes, buffer.http_workers, buffer.received_bytes);
  DtsHlsAudioTrack tracks[DTS_TRACK_MAX];
  int selected, count = dts_hls_webos_audio_tracks(tracks, DTS_TRACK_MAX, &selected);
  for (int i = 0; i < count; i++) {
    const DtsTrack *audio = &tracks[i].track;
    printf("input audio ordinal=%d stream=%d selected=%d codec=%s language=%s title=%s\n",
           tracks[i].ordinal, audio->stream_index, i == selected, audio->codec, audio->language, audio->title);
  }
  dts_tpk_fechar();
  return 0;
}
