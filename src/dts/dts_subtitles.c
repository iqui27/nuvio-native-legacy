#include "dts_subtitles.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#ifndef NV_DTS_FFMPEG
DtsSubtitles *dts_subtitles_create(const void *p) { (void)p; return NULL; }
int dts_subtitles_decode(DtsSubtitles *s, const DtsFrame *f, DtsSubtitle *out) {
  (void)s; (void)f; if (out) memset(out, 0, sizeof *out); return -1;
}
void dts_subtitles_flush(DtsSubtitles *s) { (void)s; }
void dts_subtitles_destroy(DtsSubtitles *s) { (void)s; }
#else
#include <libavcodec/avcodec.h>
#include <libavcodec/codec_par.h>
#include <libavutil/mathematics.h>

#define DTS_SUBTITLE_BYTES (16U * 1024U * 1024U)
#define DTS_SUBTITLE_RECTS 64U
struct DtsSubtitles { AVCodecContext *codec; uint8_t *rgba; };

static int add_time(int64_t base, int64_t offset, int64_t *out) {
  if (offset < 0 || base > INT64_MAX - offset) return 0;
  *out = base + offset;
  return 1;
}
static void append_text(char *out, size_t capacity, const char *text) {
  size_t used = strlen(out), length, count;
  if (!text || used + 1 >= capacity) return;
  length = strlen(text); count = length;
  if (count > capacity - used - 1) {
    count = capacity - used - 1;
    /* Never split a UTF-8 code point at the fixed text boundary. */
    while (count && ((unsigned char)text[count] & 0xc0) == 0x80) count--;
  }
  memcpy(out + used, text, count); out[used + count] = 0;
}
static void ass_text(char *out, size_t capacity, const char *event) {
  const char *p = event;
  int commas = 8, drawing = 0;
  size_t used = strlen(out);
  if (!p) return;
  if (!strncmp(p, "Dialogue:", 9)) { p += 9; commas = 9; }
  /* FFmpeg ASS event format: ReadOrder,Layer,Style,Name,Margins,Effect,Text. */
  while (commas && *p) { if (*p++ == ',') commas--; }
  if (commas) return;
  while (*p && used + 1 < capacity) {
    char unit[5] = {0};
    size_t n = 1;
    if (*p == '{') {
      const char *end = strchr(p, '}');
      if (!end) break;
      for (const char *tag = p; tag + 2 < end; tag++)
        if (tag[0] == '\\' && tag[1] == 'p' && tag[2] >= '0' && tag[2] <= '9')
          drawing = tag[2] != '0';
      p = end + 1; continue;
    }
    if (drawing) { p++; continue; }
    if (*p == '\\' && p[1]) {
      if (p[1] == 'N' || p[1] == 'n') { unit[0] = '\n'; p += 2; }
      else if (p[1] == 'h') { unit[0] = ' '; p += 2; }
      else if (p[1] == '{' || p[1] == '}' || p[1] == '\\') { unit[0] = p[1]; p += 2; }
      else { unit[0] = *p++; }
    } else {
      unsigned char c = (unsigned char)*p;
      if ((c & 0xe0) == 0xc0) n = 2;
      else if ((c & 0xf0) == 0xe0) n = 3;
      else if ((c & 0xf8) == 0xf0) n = 4;
      for (size_t i = 0; i < n; i++) if (!p[i]) { n = i; break; }
      memcpy(unit, p, n); p += n;
    }
    append_text(out, capacity, unit);
    if (strlen(out) == used) break;
    used = strlen(out);
  }
}
static int rectangle_bounds(const AVSubtitleRect *r, int canvas_w, int canvas_h,
                             int64_t *x0, int64_t *y0, int64_t *x1, int64_t *y1) {
  if (r->w <= 0 || r->h <= 0 || r->nb_colors < 1 || r->nb_colors > 256 ||
      !r->data[0] || !r->data[1] || r->linesize[0] < r->w ||
      (uint64_t)r->linesize[0] * (unsigned)r->h > DTS_SUBTITLE_BYTES) return -1;
  *x0 = r->x < 0 ? 0 : r->x; *y0 = r->y < 0 ? 0 : r->y;
  *x1 = (int64_t)r->x + r->w; *y1 = (int64_t)r->y + r->h;
  if (canvas_w > 0 && *x1 > canvas_w) *x1 = canvas_w;
  if (canvas_h > 0 && *y1 > canvas_h) *y1 = canvas_h;
  if (*x1 <= *x0 || *y1 <= *y0) return 0;
  if (*x1 > INT_MAX || *y1 > INT_MAX) return -1;
  return 1;
}
static void blend(uint8_t *dst, uint32_t argb) {
  unsigned sa = argb >> 24, da = dst[3];
  unsigned alpha = sa + (da * (255 - sa) + 127) / 255;
  if (!sa) return;
  for (int i = 0; i < 3; i++) {
    unsigned color = (argb >> (16 - i * 8)) & 255;
    dst[i] = (uint8_t)((color * sa * 255 + dst[i] * da * (255 - sa) + alpha * 127) /
                        (alpha * 255));
  }
  dst[3] = (uint8_t)alpha;
}
/* Separate conversion enables deterministic bounds/palette tests while real
 * codecs remain exercised through the public packet API. */
static int convert_subtitle(DtsSubtitles *decoder, const AVSubtitle *sub,
                             const DtsFrame *frame, DtsSubtitle *out) {
  int64_t base = frame->pts_ns, x0 = INT64_MAX, y0 = INT64_MAX, x1 = 0, y1 = 0;
  int has_bitmap = 0, oversized;
  size_t bytes;
  if (sub->num_rects > DTS_SUBTITLE_RECTS || (sub->num_rects && !sub->rects)) return -1;
  if (base == INT64_MIN) base = frame->dts_ns;
  if (base == INT64_MIN && sub->pts != AV_NOPTS_VALUE)
    base = av_rescale_q(sub->pts, AV_TIME_BASE_Q, (AVRational){1,1000000000});
  if (base == INT64_MIN) return -1;
  if (!add_time(base, (int64_t)sub->start_display_time * 1000000, &out->start_ns)) return -1;
  if (sub->end_display_time > sub->start_display_time && sub->end_display_time != UINT32_MAX) {
    if (!add_time(base, (int64_t)sub->end_display_time * 1000000, &out->end_ns)) return -1;
  } else if (frame->duration_ns > 0) {
    if (!add_time(base, frame->duration_ns, &out->end_ns) || out->end_ns < out->start_ns) return -1;
  } else out->end_ns = INT64_MAX;
  out->canvas_w = decoder->codec->width; out->canvas_h = decoder->codec->height;
  for (unsigned i = 0; i < sub->num_rects; ++i) {
    const AVSubtitleRect *r = sub->rects[i];
    int64_t a,b,c,d;
    int visible;
    if (!r) return -1;
    if (r->type == SUBTITLE_TEXT || r->type == SUBTITLE_ASS) {
      if (out->text[0]) append_text(out->text, sizeof out->text, "\n");
      if (r->type == SUBTITLE_TEXT) append_text(out->text, sizeof out->text, r->text);
      else ass_text(out->text, sizeof out->text, r->ass);
      continue;
    }
    if (r->type != SUBTITLE_BITMAP) continue;
    visible = rectangle_bounds(r, out->canvas_w, out->canvas_h, &a,&b,&c,&d);
    if (visible < 0) return -1;
    if (!visible) continue;
    has_bitmap = 1;
    if (a < x0) x0 = a;
    if (b < y0) y0 = b;
    if (c > x1) x1 = c;
    if (d > y1) y1 = d;
  }
  if (!has_bitmap) return 1; /* Also carries explicit bitmap clearing events. */
  oversized = x1 - x0 > DTS_SUBTITLE_BYTES / 4 || y1 - y0 > DTS_SUBTITLE_BYTES / 4 ||
      (uint64_t)(x1 - x0) * (uint64_t)(y1 - y0) > DTS_SUBTITLE_BYTES / 4;
  out->x = (int)x0; out->y = (int)y0; out->w = (int)(x1 - x0); out->h = (int)(y1 - y0);
  if (!oversized) {
    bytes = (size_t)out->w * out->h * 4;
    decoder->rgba = calloc(1, bytes);
    if (!decoder->rgba) return -1;
  }
  for (unsigned i = 0; i < sub->num_rects; ++i) {
    const AVSubtitleRect *r = sub->rects[i];
    int64_t a,b,c,d;
    if (r->type != SUBTITLE_BITMAP || rectangle_bounds(r, out->canvas_w, out->canvas_h, &a,&b,&c,&d) <= 0) continue;
    for (int64_t y = b; y < d; ++y) {
      const uint8_t *row = r->data[0] + (size_t)(y - r->y) * r->linesize[0];
      for (int64_t x = a; x < c; ++x) {
        unsigned index = row[x - r->x];
        uint32_t argb;
        if (index >= (unsigned)r->nb_colors) return -1;
        /* Oversized output still validates indices, but never allocates RGBA. */
        if (!oversized) {
          memcpy(&argb, r->data[1] + index * 4, sizeof argb);
          blend(decoder->rgba + ((size_t)(y - y0) * out->w + (size_t)(x - x0)) * 4, argb);
        }
      }
    }
  }
  if (oversized) return DTS_SUBTITLE_SKIPPED;
  out->rgba = decoder->rgba;
  return 1;
}
DtsSubtitles *dts_subtitles_create(const void *parameters) {
  const AVCodecParameters *p = parameters;
  const AVCodec *codec;
  DtsSubtitles *s;
  if (!p || p->codec_type != AVMEDIA_TYPE_SUBTITLE) return NULL;
  codec = avcodec_find_decoder(p->codec_id);
  if (!codec) return NULL;
  s = calloc(1, sizeof *s);
  if (!s) return NULL;
  s->codec = avcodec_alloc_context3(codec);
  if (!s->codec || avcodec_parameters_to_context(s->codec, p) < 0) { dts_subtitles_destroy(s); return NULL; }
  s->codec->pkt_timebase = AV_TIME_BASE_Q;
  if (avcodec_open2(s->codec, codec, NULL) < 0) { dts_subtitles_destroy(s); return NULL; }
  return s;
}
int dts_subtitles_decode(DtsSubtitles *s, const DtsFrame *frame, DtsSubtitle *out) {
  AVSubtitle sub = {0};
  AVPacket *packet;
  int got = 0, result;
  if (out) memset(out, 0, sizeof *out);
  if (!s || !frame || !out || frame->kind != DTS_SUBTITLE ||
      frame->size > DTS_SUBTITLE_BYTES || (frame->size && !frame->data)) return -1;
  free(s->rgba); s->rgba = NULL;
  packet = av_packet_alloc();
  if (!packet || av_new_packet(packet, (int)frame->size) < 0) { av_packet_free(&packet); return -1; }
  if (frame->size) memcpy(packet->data, frame->data, frame->size);
  packet->pts = frame->pts_ns == INT64_MIN ? AV_NOPTS_VALUE :
    av_rescale_q(frame->pts_ns, (AVRational){1,1000000000}, AV_TIME_BASE_Q);
  packet->dts = frame->dts_ns == INT64_MIN ? AV_NOPTS_VALUE :
    av_rescale_q(frame->dts_ns, (AVRational){1,1000000000}, AV_TIME_BASE_Q);
  packet->duration = frame->duration_ns > 0 ?
    av_rescale_q(frame->duration_ns, (AVRational){1,1000000000}, AV_TIME_BASE_Q) : 0;
  sub.pts = AV_NOPTS_VALUE;
  result = avcodec_decode_subtitle2(s->codec, &sub, &got, packet);
  av_packet_free(&packet);
  if (result >= 0) result = got ? convert_subtitle(s, &sub, frame, out) : 0;
  avsubtitle_free(&sub);
  if (result != 1) {
    free(s->rgba); s->rgba = NULL; memset(out, 0, sizeof *out);
    if (result < 0) return -1;
  }
  return result;
}
void dts_subtitles_flush(DtsSubtitles *s) {
  if (!s) return;
  avcodec_flush_buffers(s->codec); free(s->rgba); s->rgba = NULL;
}
void dts_subtitles_destroy(DtsSubtitles *s) {
  if (!s) return;
  avcodec_free_context(&s->codec); free(s->rgba); free(s);
}
#endif
