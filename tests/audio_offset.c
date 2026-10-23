#define NV_DTS_FFMPEG
#define NV_DTS_REMUX_TEST
#include "../src/dts/dts_remux.c"
#include <assert.h>

static int64_t audio_clock, decode_clock, video_clock;
/* Native mux boundary: inject already-converted audio, observe presentation. */
static int capture(AVFormatContext *ctx, AVPacket *packet) {
  (void)ctx;
  if (packet->stream_index == 1) {
    audio_clock = packet->pts;
    decode_clock = packet->dts;
  } else video_clock = packet->pts;
  return 0;
}

static int64_t inject(int offset) {
  dts_remux_set_audio_offset_ms(offset);
  DtsRemux *source = dts_remux_new(NULL);
  assert(source);
  dts_remux_set_audio_offset_ms(0); /* An existing session keeps its offset. */
  DtsRemuxGen gen = { .src = source };
  AVFormatContext *ctx = avformat_alloc_context();
  AVStream *video = avformat_new_stream(ctx, NULL);
  AVStream *audio = avformat_new_stream(ctx, NULL);
  assert(video && audio);
  video->time_base = audio->time_base = (AVRational){1, 90000};
  audio->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
  audio->codecpar->codec_id = AV_CODEC_ID_AC3;
  gen.oc = ctx;
  for (int kind = 0; kind < 2; kind++) {
    AVPacket *packet = av_packet_alloc();
    assert(packet && av_new_packet(packet, 16) == 0);
    packet->pts = 480000; packet->dts = 478464; packet->duration = 1536;
    assert(writePkt(&gen, packet, (AVRational){1, 48000}, kind ? audio : video, NULL, !kind) == 0);
  }
  assert(video_clock == 900000);
  avformat_free_context(ctx);
  dts_remux_close(source);
  return audio_clock;
}

int main(void) {
  DtsRemux *defaults = dts_remux_new(NULL);
  assert(defaults && defaults->audio_offset_ms == 0);
  assert(AUDSYNC_OFFSET_STEP_MS == 20);
  dts_remux_close(defaults);
  int64_t zero = inject(0), zero_decode = decode_clock;
  assert(zero == 900000 && zero_decode == 897120);
  int64_t delayed = inject(80);
  printf("converted audio clock: 0=%lld, +80=%lld (90 kHz)\n", (long long)zero, (long long)delayed);
  fflush(stdout);
  assert(delayed - zero == 7200);
  assert(decode_clock - zero_decode == 7200);
  assert(inject(-80) - zero == -7200);
  assert(inject(500) - zero == 45000);
  assert(inject(-500) - zero == -45000);
  assert(inject(999) - zero == 45000);
  assert(inject(-999) - zero == -45000);
  assert(inject(0) == zero);
  puts("audio_offset: PASS (positive delays audio; video and default unchanged)");
}

int av_interleaved_write_frame(AVFormatContext *ctx, AVPacket *packet) {
  return capture(ctx, packet);
}
