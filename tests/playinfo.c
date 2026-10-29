#include "playinfo.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  Stream s = {0}; VideoStats vs; VideoInfo vi = {0}; P2pmEstado p = {0};
  PlayInfo m; VideoFaixa audio = {0}, sub = {0};
  video_stats_vazio(&vs);
  snprintf(s.provedor, sizeof s.provedor, "Jellyfin");
  snprintf(s.bingeGroup, sizeof s.bingeGroup, "jellyfin|DirectPlay");
  snprintf(s.arquivo, sizeof s.arquivo, "/private/library/Film.mkv");
  snprintf(vi.codec, sizeof vi.codec, "HEVC"); vi.fps = 23.976; vi.kbps = 8000;
  snprintf(audio.codec, sizeof audio.codec, "EAC3"); audio.canais = 6;
  snprintf(audio.idioma, sizeof audio.idioma, "en");
  snprintf(sub.rotulo, sizeof sub.rotulo, "English");
  vs.bufferSeg = 12.5; vs.quadrosPerdidos = 0;
  playinfo_model(&m, &s, &vs, &vi, 1920, 1080, "HDR10", &audio, &sub, 0, &p, 5000);
  assert(!strcmp(m.value[PI_MODE], "Direct Play"));
  assert(!strcmp(m.value[PI_FILE], "Film.mkv"));
  assert(!strcmp(m.value[PI_CONTAINER], "MKV"));
  assert(!strcmp(m.value[PI_CODEC], "HEVC"));
  assert(strstr(m.value[PI_FPS], "23.976"));
  assert(strstr(m.value[PI_BITRATE], "8.00"));
  assert(strstr(m.value[PI_AUDIO], "6") && strstr(m.value[PI_AUDIO], "en"));
  assert(!strcmp(m.value[PI_SUBTITLE], "English"));
  assert(!strcmp(m.value[PI_DROPPED], "0"));
  snprintf(s.bingeGroup, sizeof s.bingeGroup, "emby|Transcode");
  memset(&vi, 0, sizeof vi);
  playinfo_model(&m, &s, &vs, &vi, 0, 0, "desconhecido", NULL, NULL, 0, &p, 5000);
  assert(!strcmp(m.value[PI_MODE], "Transcoding"));
  assert(!strcmp(m.value[PI_CONTAINER], "—")); // original file is not the transcode container
  assert(!strcmp(m.value[PI_CODEC], "—"));
  assert(!strcmp(m.value[PI_HDR], "—"));
  assert(!strcmp(m.value[PI_BITRATE], "—")); // original bitrate is not output bitrate
  snprintf(s.bingeGroup, sizeof s.bingeGroup, "plex|DirectStream");
  playinfo_model(&m, &s, &vs, &vi, 0, 0, "", NULL, NULL, 0, &p, 5000);
  assert(!strcmp(m.value[PI_MODE], "Direct Stream"));
  s.bingeGroup[0] = 0; s.urlP2P = 1; p.ativo = 1; p.pares = 12; p.baixando_bps = 2500000;
  playinfo_model(&m, &s, &vs, &vi, 0, 0, "", NULL, NULL, 0, &p, 5000);
  assert(!strcmp(m.value[PI_MODE], "Direct"));
  assert(m.p2p && !strcmp(m.value[PI_PEERS], "12"));
  assert(strstr(m.value[PI_DOWNLOAD], "2.50 MB/s"));
  assert(strstr(m.value[PI_BITRATE], "5.00"));
  snprintf(s.arquivo, sizeof s.arquivo, "https://user:password@example.test/Film.mkv?token=secret");
  snprintf(s.provedor, sizeof s.provedor, "https://example.test/?token=secret");
  playinfo_model(&m, &s, &vs, &vi, 0, 0, "none", NULL, NULL, 0, &p, 0);
  assert(!strcmp(m.value[PI_FILE], "—") && !strcmp(m.value[PI_SOURCE], "—"));
  playinfo_model(&m, NULL, &vs, &vi, 0, 0, "", NULL, NULL, 0, NULL, 0);
  assert(!m.p2p && !strcmp(m.value[PI_PEERS], "—"));
  assert(!strcmp(m.value[PI_SUBTITLE], "—"));
  puts("PASS: playback info from fake player state, server modes, P2P, unknowns and redaction");
  return 0;
}
