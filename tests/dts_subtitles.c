/* Include the implementation to exercise the bitmap conversion boundary with
 * deterministic palette rectangles, while text fixtures use actual decoders. */
#include "../src/dts/dts_subtitles.c"
#include <assert.h>
#include <stdio.h>

static DtsSubtitles *decoder(enum AVCodecID id) {
  AVCodecParameters *p = avcodec_parameters_alloc();
  DtsSubtitles *d;
  assert(p);
  p->codec_type = AVMEDIA_TYPE_SUBTITLE; p->codec_id = id;
  d = dts_subtitles_create(p);
  avcodec_parameters_free(&p);
  if (!d) fprintf(stderr, "Subtitle decoder unavailable: %d\n", id);
  assert(d);
  return d;
}
static void text_packets(void) {
  static const unsigned char srt[] = "Hello <i>from DTS</i>\nSecond line";
  static const unsigned char mov[] = {0,12,'M','P','4',' ','s','u','b','t','i','t','l','e'};
  static const unsigned char ass[] = "0,0,Default,,0,0,0,,{\\i1}Styled{\\i0}\\Nline\\hspace";
  DtsFrame f = {.kind=DTS_SUBTITLE, .stream_index=7, .data=srt, .size=sizeof srt-1,
                .pts_ns=INT64_C(4500000123), .dts_ns=INT64_MIN, .duration_ns=INT64_C(1250000000)};
  DtsSubtitle out;
  DtsSubtitles *d = decoder(AV_CODEC_ID_SUBRIP);
  assert(dts_subtitles_decode(d, &f, &out) == 1);
  assert(!strcmp(out.text, "Hello from DTS\nSecond line"));
  assert(out.start_ns == f.pts_ns && out.end_ns == f.pts_ns + f.duration_ns);
  assert(!out.rgba);
  dts_subtitles_flush(d);
  f.pts_ns = INT64_C(1000000000);
  assert(dts_subtitles_decode(d, &f, &out) == 1 && out.start_ns == f.pts_ns);
  assert(!strcmp(out.text, "Hello from DTS\nSecond line"));
  dts_subtitles_destroy(d);
  d = decoder(AV_CODEC_ID_MOV_TEXT);
  f.data=mov; f.size=sizeof mov;
  assert(dts_subtitles_decode(d, &f, &out) == 1);
  assert(!strcmp(out.text, "MP4 subtitle") && out.end_ns == f.pts_ns + f.duration_ns);
  f.size=1;
  assert(dts_subtitles_decode(d, &f, &out) == -1 && !out.text[0] && !out.rgba);
  dts_subtitles_flush(d); f.size=sizeof mov;
  assert(dts_subtitles_decode(d, &f, &out) == 1);
  dts_subtitles_destroy(d);
  d=decoder(AV_CODEC_ID_ASS); f.data=ass; f.size=sizeof ass-1;
  assert(dts_subtitles_decode(d, &f, &out) == 1);
  assert(!strcmp(out.text, "Styled\nline space"));
  f.duration_ns=0;
  assert(dts_subtitles_decode(d, &f, &out) == 1 && out.end_ns == INT64_MAX);
  f.pts_ns=INT64_MIN;
  assert(dts_subtitles_decode(d, &f, &out) == -1);
  f.pts_ns=0; f.size=16U*1024U*1024U+1;
  assert(dts_subtitles_decode(d, &f, &out) == -1);
  f.kind=DTS_AUDIO;
  assert(dts_subtitles_decode(d, &f, &out) == -1);
  dts_subtitles_destroy(d);
}
static void bitmap_palette(void) {
  uint32_t palette[] = {0x00000000, 0xffff0000, 0xff0000ff};
  uint32_t overlay_palette[] = {0x8000ff00};
  uint8_t indices[] = {0,1,2, 2,0,1}, overlay[] = {0};
  AVSubtitleRect first = {.type=SUBTITLE_BITMAP, .x=-1, .y=1, .w=3, .h=2, .nb_colors=3,
                         .data={indices,(uint8_t *)palette}, .linesize={3}};
  AVSubtitleRect second = {.type=SUBTITLE_BITMAP, .x=1, .y=1, .w=1, .h=1, .nb_colors=1,
                          .data={overlay,(uint8_t *)overlay_palette}, .linesize={1}};
  AVSubtitleRect *rects[] = {&first,&second};
  AVSubtitle sub = {.start_display_time=100, .end_display_time=800,
                    .num_rects=2, .rects=rects, .pts=AV_NOPTS_VALUE};
  DtsFrame frame = {.kind=DTS_SUBTITLE, .pts_ns=INT64_C(2000000000), .duration_ns=0};
  DtsSubtitles *d=decoder(AV_CODEC_ID_HDMV_PGS_SUBTITLE);
  DtsSubtitle out={0};
  d->codec->width=2; d->codec->height=3;
  assert(convert_subtitle(d, &sub, &frame, &out) == 1);
  assert(out.x==0 && out.y==1 && out.w==2 && out.h==2 && out.canvas_w==2 && out.canvas_h==3);
  assert(out.start_ns==INT64_C(2100000000) && out.end_ns==INT64_C(2800000000));
  assert(out.rgba[0]==255 && out.rgba[1]==0 && out.rgba[2]==0 && out.rgba[3]==255);
  assert(out.rgba[4]==0 && out.rgba[5]==128 && out.rgba[6]==127 && out.rgba[7]==255);
  assert(out.rgba[8]==0 && out.rgba[11]==0); /* transparent remains transparent */
  assert(out.rgba[12]==255 && out.rgba[15]==255);
  dts_subtitles_flush(d); assert(!d->rgba);
  memset(&out,0,sizeof out); indices[1]=9;
  assert(convert_subtitle(d, &sub, &frame, &out)==-1);
  dts_subtitles_flush(d); indices[1]=1;
  first.linesize[0]=-3;
  assert(convert_subtitle(d, &sub, &frame, &out)==-1);
  first.linesize[0]=3;
  // Clear event: PGS has no rectangles and may omit an end timestamp.
  sub.num_rects=0; sub.end_display_time=UINT32_MAX;
  memset(&out,0,sizeof out);
  assert(convert_subtitle(d, &sub, &frame, &out)==1 && !out.rgba && !out.text[0] && out.end_ns==INT64_MAX);
  frame.pts_ns=INT64_MAX-100;
  assert(convert_subtitle(d, &sub, &frame, &out)==-1);
  frame.pts_ns=INT64_C(2000000000);
  // Huge or widely separated rectangles cannot allocate more than 16 MiB.
  sub.num_rects=2; d->codec->width=0; d->codec->height=0;
  // A valid but over-budget union is skipped without allocating RGBA.
  second.x=5000000;
  assert(convert_subtitle(d, &sub, &frame, &out)==DTS_SUBTITLE_SKIPPED && !d->rgba);
  second.x=1; first.w=first.h=60000; first.linesize[0]=60000;
  assert(convert_subtitle(d, &sub, &frame, &out)==-1);
  dts_subtitles_destroy(d);
}
static void text_bounds(void) {
  DtsSubtitles *d=decoder(AV_CODEC_ID_ASS);
  char large[4096];
  DtsFrame frame={.kind=DTS_SUBTITLE, .data=(uint8_t *)large,
                  .pts_ns=INT64_C(1000000000), .duration_ns=INT64_C(1000000000)};
  DtsSubtitle out;
  size_t n;
  strcpy(large,"0,0,Default,,0,0,0,,"); n=strlen(large);
  while(n+2<sizeof large) { large[n++]=(char)0xc3; large[n++]=(char)0xa9; }
  large[n]=0; frame.size=n;
  assert(dts_subtitles_decode(d,&frame,&out)==1);
  assert(strlen(out.text)==2046 && (unsigned char)out.text[2045]==0xa9);
  dts_subtitles_destroy(d);
}
int main(void) {
  AVCodecParameters *parameters = avcodec_parameters_alloc();
  assert(parameters && !dts_subtitles_create(NULL));
  parameters->codec_type = AVMEDIA_TYPE_VIDEO; parameters->codec_id = AV_CODEC_ID_H264;
  assert(!dts_subtitles_create(parameters));
  parameters->codec_type = AVMEDIA_TYPE_SUBTITLE; parameters->codec_id = AV_CODEC_ID_NONE;
  assert(!dts_subtitles_create(parameters));
  avcodec_parameters_free(&parameters);
  text_packets(); bitmap_palette(); text_bounds();
  puts("dts_subtitles: real text decoders, timing, flush, palette, clipping and bounds passed");
  return 0;
}
