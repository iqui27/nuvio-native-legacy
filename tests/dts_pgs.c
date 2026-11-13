/* Real FFmpeg PGS decoding and production conversion, no decoder mocks. */
#include "../src/dts/dts_subtitles.c"
#include "dts_pgs_packet.h"
#include <stdio.h>
int main(void) {
  AVCodecParameters *params = avcodec_parameters_alloc();
  assert(params);
  params->codec_type = AVMEDIA_TYPE_SUBTITLE;
  params->codec_id = AV_CODEC_ID_HDMV_PGS_SUBTITLE;
  DtsSubtitles *d = dts_subtitles_create(params);
  avcodec_parameters_free(&params);
  assert(d);
  uint8_t packet[16000];
  DtsFrame frame = {.kind=DTS_SUBTITLE, .data=packet, .pts_ns=1000000000,
                    .duration_ns=1000000000};
  DtsSubtitle out;
  frame.size = pgs_packet(packet,sizeof packet,3840,2160);
  /* A valid 8,294,400-byte plane would expand to 33,177,600 RGBA bytes. */
  assert(dts_subtitles_decode(d,&frame,&out)==2);
  assert(!out.rgba && !out.text[0] && !d->rgba);
  frame.size = pgs_packet(packet,sizeof packet,2,2);
  assert(dts_subtitles_decode(d,&frame,&out)==1);
  assert(out.rgba && out.w==2 && out.h==2 && out.rgba[3]==255);
  dts_subtitles_flush(d);
  /* Malformed indexed data still fails, even when its output is oversized. */
  uint8_t *indices = calloc(3840,2160);
  uint32_t palette[1] = {0};
  assert(indices); indices[3840*2160-1]=1;
  AVSubtitleRect rect = {.type=SUBTITLE_BITMAP,.w=3840,.h=2160,.nb_colors=1,
                         .data={indices,(uint8_t *)palette},.linesize={3840}};
  AVSubtitleRect *rects[] = {&rect};
  AVSubtitle sub = {.num_rects=1,.rects=rects,.pts=AV_NOPTS_VALUE};
  d->codec->width=3840; d->codec->height=2160;
  memset(&out,0,sizeof out);
  assert(convert_subtitle(d,&sub,&frame,&out)==-1 && !d->rgba);
  rect.linesize[0]=-1;
  assert(convert_subtitle(d,&sub,&frame,&out)==-1);
  free(indices);
  /* Packet-level bound stays fatal too. */
  frame.size=16u*1024u*1024u+1u;
  assert(dts_subtitles_decode(d,&frame,&out)==-1 && !out.rgba);
  dts_subtitles_destroy(d);
  puts("PASS real PGS: oversized output skipped before RGBA allocation, next cue decodes, malformed data fails");
  return 0;
}
