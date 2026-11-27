// Demux opens skip Matroska attachments (src/dts/dts_engine.c, att_*): the
// fonts are never downloaded, the tracks and their packets are unchanged.
// Usage: dts_anexos <url> <attachment bytes> <expected audio+video streams>
#include "dts/dts_engine.h"
#include <libavformat/avformat.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  DtsEngine *e;
  AVFormatContext *ic;
  DtsEngineMetrics m;
  AVPacket *pk;
  long anexo, esperadas;
  int i, pacotes = 0, anexos = 0, fail = 0;
  if (argc < 4) return 2;
  anexo = atol(argv[2]); esperadas = atol(argv[3]);
  e = dts_engine_create();
  if (!e || dts_engine_open_demux(e, argv[1], "") < 0) { printf("FAIL open: %s\n", e ? dts_engine_error(e) : "alloc"); return 1; }
  ic = dts_engine_avformat(e);
  for (i = 0; i < (int)ic->nb_streams; i++)
    if (ic->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_ATTACHMENT) anexos++;
  dts_engine_metrics(e, &m);
  printf("streams %u (attachments %d), %llu bytes downloaded by the open, attachment %ld bytes\n", ic->nb_streams, anexos,
         (unsigned long long)m.range_bytes, anexo);
  if (anexos) { printf("FAIL the attachments were read\n"); fail = 1; }
  if ((long)ic->nb_streams != esperadas) { printf("FAIL %u streams, expected %ld\n", ic->nb_streams, esperadas); fail = 1; }
  if (anexo > 0 && m.range_bytes >= (uint64_t)anexo) { printf("FAIL the open downloaded more than the attachment\n"); fail = 1; }
  for (i = 0; i < (int)ic->nb_streams; i++) ic->streams[i]->discard = AVDISCARD_DEFAULT;
  pk = av_packet_alloc();
  while (pk && av_read_frame(ic, pk) >= 0) { pacotes++; av_packet_unref(pk); }
  av_packet_free(&pk);
  printf("%d packets read to the end\n", pacotes);
  if (pacotes < 100) { printf("FAIL too few packets\n"); fail = 1; }
  dts_engine_destroy(e);
  return fail;
}
