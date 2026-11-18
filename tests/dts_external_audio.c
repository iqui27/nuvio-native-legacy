/* External audio (dts_engine_external_audio): the video of one file with the
 * audio of another, interleaved, shifted by the offset and trimmed on seek.
 * Local files stand in for both HTTP transports: ranges for the main source,
 * the single streaming read for the second one. */
#include "../src/dts/dts_engine.h"
#ifndef EXT_STALL_S
#define EXT_STALL_S 8
#endif
#include <assert.h>
#include <pthread.h>
#include <time.h>
#include <libavformat/avformat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

static const char *HEADERS = "X-Fixture: yes";
static unsigned streams_opened;
static int64_t hang_at;          /* >0: the stream of hang_url stops sending there */
static char hang_url[4096];
/* trickle_left connections of trickle_url that reach trickle_at go on at
 * about 20 KB/s (owner's logs 14/11: TorBox CDN connections at 15-25 KB/s
 * while a fresh one got MB/s); trickle_hits counts them. */
static int64_t trickle_at;
static int trickle_left, trickle_hits;
static char trickle_url[4096];
/* "addon:<file>" stands for an add-on link that redirects to <file> (the CDN):
 * addon_hits counts the trips through it; cdn_dead fails the next direct
 * request (an expired or gone server address). */
static unsigned addon_hits;
static int cdn_dead;

char *rede_range_headers(const char *raw, int public_only) {
  (void)public_only; return strdup(raw ? raw : "");
}
int rede_fluxo64(const char *url, const char *headers, int64_t start,
                 int (*sink)(void *, const char *, size_t), void *opaque,
                 int64_t *total, int *status, volatile int *cancel,
                 char *final, int *cross, long stall_ms, int64_t *bytes) {
  (void)stall_ms;
  assert(!strcmp(headers, HEADERS));
  if (final) final[0] = 0;
  if (cross) *cross = 0;
  *bytes = 0; *status = 0;
  if (!strncmp(url, "addon:", 6)) {
    __atomic_add_fetch(&addon_hits, 1, __ATOMIC_RELAXED);
    url += 6;
    if (final) snprintf(final, 8192, "%s", url);
  } else if (__atomic_exchange_n(&cdn_dead, 0, __ATOMIC_RELAXED)) { *status = 410; return -1; }
  FILE *f = fopen(url, "rb");
  if (!f) return -1;
  __atomic_add_fetch(&streams_opened, 1, __ATOMIC_RELAXED);
  fseeko(f, 0, SEEK_END); *total = ftello(f); fseeko(f, start, SEEK_SET);
  *status = 206;
  char buf[65536];
  int64_t at = start;
  int slow = -1;   /* not decided until this connection reaches trickle_at */
  for (;;) {
    if (__atomic_load_n(cancel, __ATOMIC_RELAXED)) { fclose(f); return -1; }
    if (slow < 0 && trickle_at > 0 && at >= trickle_at && !strcmp(url, trickle_url)) {
      slow = __atomic_sub_fetch(&trickle_left, 1, __ATOMIC_RELAXED) >= 0;
      if (slow) __atomic_add_fetch(&trickle_hits, 1, __ATOMIC_RELAXED);
    }
    if (slow == 1) {
      size_t n = fread(buf, 1, 1024, f);
      if (!n) break;
      at += (int64_t)n;
      if (!sink(opaque, buf, n)) { fclose(f); return 1; }
      *bytes += (int64_t)n;
      usleep(50000);
      continue;
    }
    /* A connection that goes quiet past hang_at (owner's log 14/11): open,
     * no bytes, until dropped. */
    int64_t h = __atomic_load_n(&hang_at, __ATOMIC_RELAXED);
    if (h > 0 && !strcmp(url, hang_url) && at >= h) {
      while (!__atomic_load_n(cancel, __ATOMIC_RELAXED) && __atomic_load_n(&hang_at, __ATOMIC_RELAXED) > 0) usleep(10000);
      continue;
    }
    size_t want = sizeof buf;
    if (h > 0 && !strcmp(url, hang_url) && at < h && (int64_t)want > h - at) want = (size_t)(h - at);
    size_t n = fread(buf, 1, want, f);
    at += (int64_t)n;
    if (!n) break;
    if (!sink(opaque, buf, n)) { fclose(f); return 1; }
    *bytes += (int64_t)n;
  }
  fclose(f);
  return 0;
}
char *rede_baixar_trecho64_cab(const char *url, const char *headers, int64_t start, int64_t end,
                               long *size, int64_t *total, int *status, volatile int *cancelled) {
  assert(!strcmp(headers, HEADERS));
  *size = 0; *status = 0; *total = -1;
  if (__atomic_load_n(cancelled, __ATOMIC_RELAXED)) return NULL;
  FILE *f = fopen(url, "rb");
  if (!f) return NULL;
  fseeko(f, 0, SEEK_END); *total = ftello(f);
  if (start >= *total) { *status = 416; fclose(f); return NULL; }
  fseeko(f, start, SEEK_SET);
  long n = (long)(end - start + 1);
  if (n > *total - start) n = (long)(*total - start);
  char *data = malloc((size_t)n); assert(data);
  *size = (long)fread(data, 1, (size_t)n, f); fclose(f); *status = 206;
  return data;
}
char *rede_baixar_trecho64_final(const char *url, const char *headers, int64_t start, int64_t end,
                                 long *size, int64_t *total, int *status, volatile int *cancelled,
                                 char *final, int *cross) {
  if (final) final[0] = 0;
  if (cross) *cross = 0;
  return rede_baixar_trecho64_cab(url, headers, start, end, size, total, status, cancelled);
}
char *rede_baixar_trecho64_vigiado(const char *u, const char *h, int64_t s, int64_t e, long *n,
                                   int64_t *t, int *status, volatile int *c, char *f, int *x,
                                   volatile int *urgent, unsigned idle, int *stalled) {
  (void)urgent; (void)idle; if (stalled) *stalled = 0;
  return rede_baixar_trecho64_final(u, h, s, e, n, t, status, c, f, x);
}
char *rede_baixar_trecho64_relato(const char *u, const char *h, int64_t s, int64_t e, long *n,
                                  int64_t *t, int *status, volatile int *c, char *f, int *x,
                                  volatile int *urgent, unsigned idle, int *stalled, int *retry_after) {
  (void)urgent; (void)idle; if (stalled) *stalled = 0; if (retry_after) *retry_after = -1;
  return rede_baixar_trecho64_final(u, h, s, e, n, t, status, c, f, x);
}

/* The app passes ling_casa; here "pl" also matches "pol". */
static int lang_match(const char *code, const char *pref) {
  if (!strcasecmp(code, pref)) return 1;
  return !strcasecmp(pref, "pl") && !strcasecmp(code, "pol");
}

/* Packets of the chosen stream in the other file, to check bytes and times. */
typedef struct { int64_t pts_ns; int size; unsigned sum; } Ref;
static Ref refs[4096];
static int nrefs;
static unsigned checksum(const unsigned char *d, size_t n) {
  unsigned s = 2166136261u;
  for (size_t i = 0; i < n; i++) s = (s ^ d[i]) * 16777619u;
  return s;
}
/* The host FFmpeg has no file protocol: read through a stdio AVIO. */
static int file_read(void *o, uint8_t *b, int n) {
  size_t k = fread(b, 1, (size_t)n, o);
  return k ? (int)k : AVERROR_EOF;
}
static int64_t file_seek(void *o, int64_t off, int whence) {
  FILE *f = o;
  if (whence == AVSEEK_SIZE) { int64_t at = ftello(f), end; fseeko(f, 0, SEEK_END); end = ftello(f); fseeko(f, at, SEEK_SET); return end; }
  return fseeko(f, off, whence & ~AVSEEK_FORCE) ? -1 : ftello(f);
}
static void load_refs(const char *path, const char *lang) {
  FILE *file = fopen(path, "rb");
  assert(file);
  AVFormatContext *fmt = avformat_alloc_context();
  AVIOContext *io = avio_alloc_context(av_malloc(32768), 32768, 0, file, file_read, NULL, file_seek);
  assert(fmt && io);
  fmt->pb = io; fmt->flags |= AVFMT_FLAG_CUSTOM_IO;
  assert(avformat_open_input(&fmt, NULL, NULL, NULL) == 0);
  assert(avformat_find_stream_info(fmt, NULL) >= 0);
  int k = -1;
  for (unsigned i = 0; i < fmt->nb_streams; i++) {
    AVDictionaryEntry *t = av_dict_get(fmt->streams[i]->metadata, "language", NULL, 0);
    if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && t && !strcmp(t->value, lang)) k = (int)i;
  }
  assert(k >= 0);
  AVPacket *p = av_packet_alloc(); nrefs = 0;
  while (av_read_frame(fmt, p) >= 0) {
    if (p->stream_index == k && nrefs < 4096) {
      AVRational tb = fmt->streams[k]->time_base;
      refs[nrefs++] = (Ref){av_rescale_q(p->pts, tb, (AVRational){1, 1000000000}), p->size,
                            checksum(p->data, (size_t)p->size)};
    }
    av_packet_unref(p);
  }
  av_packet_free(&p); avformat_close_input(&fmt);
  av_freep(&io->buffer); avio_context_free(&io); fclose(file);
  assert(nrefs > 50);
}
static const Ref *ref_at(int64_t pts_ns) {
  for (int i = 0; i < nrefs; i++)
    if (llabs(refs[i].pts_ns - pts_ns) < 1000000) return &refs[i];
  return NULL;
}

typedef struct { int audio, video, subs; int64_t first_audio, last_audio, max_video, worst_lead; } Run;
/* Reads to the end; every external packet must match the other file. */
static int checkBytes = 1;
static Run play(DtsEngine *e, int64_t offset_ns, int limit) {
  Run r = {0, 0, 0, INT64_MIN, INT64_MIN, INT64_MIN, INT64_MIN};
  DtsFrame f;
  int got;
  while ((got = dts_engine_next(e, &f)) == 1 && (limit <= 0 || r.audio + r.video < limit)) {
    if (f.kind == DTS_VIDEO) {
      r.video++;
      if (f.pts_ns > r.max_video) r.max_video = f.pts_ns;
    } else if (f.kind == DTS_AUDIO) {
      assert(f.stream_index == DTS_ENGINE_EXTERNAL_STREAM);
      assert(r.max_video != INT64_MIN);            /* never before the first picture */
      int64_t lead = f.pts_ns - r.max_video;
      if (lead > r.worst_lead) r.worst_lead = lead;
      assert(r.last_audio == INT64_MIN || f.pts_ns > r.last_audio);
      if (checkBytes) {
        const Ref *ref = ref_at(f.pts_ns - offset_ns);
        assert(ref && ref->size == (int)f.size && ref->sum == checksum(f.data, f.size));
      }
      if (r.first_audio == INT64_MIN) r.first_audio = f.pts_ns;
      r.last_audio = f.pts_ns; r.audio++;
    } else r.subs++;
  }
  if (got < 0) { fprintf(stderr, "next: %s\n", dts_engine_error(e)); exit(1); }
  return r;
}

/* Asks for a change (dts_engine_external_wake) once the worker waits for
 * the other version, as a track change or seek from the player would. */
typedef struct { DtsEngine *e; double waited; } Wake;
static void *wake_when_waiting(void *arg) {
  Wake *w = arg;
  struct timespec t0, t1; clock_gettime(CLOCK_MONOTONIC, &t0);
  for (;;) {
    usleep(300000);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    if (dts_engine_external_wake(w->e)) break;
    assert(t1.tv_sec - t0.tv_sec < 120);
  }
  w->waited = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
  return NULL;
}
int main(int argc, char **argv) {
  assert(argc == 36);
  const char *main_file = argv[1], *other = argv[2], *dts_only = argv[3], *short_clip = argv[4], *mp4 = argv[5];
  const char *aac44 = argv[6], *main60 = argv[7], *long_idx = argv[8], *long_noidx = argv[9];
  const char *match_main = argv[10], *match_dub = argv[11];
  const char *main_avi = argv[12], *other_avi = argv[13], *other_ts = argv[14], *main_xvid = argv[15], *film = argv[16];
  const char *cut_main = argv[17], *cut_dub = argv[18], *pal_dub = argv[19], *pair_dub = argv[20];
  const char *follow_ext = argv[21], *follow_th = argv[22], *lang_main = argv[23], *lang_pl = argv[24];
  const char *dub_ru = argv[25], *dub_pl = argv[26], *follow_ext_ru = argv[27], *follow_th_pl = argv[28];
  const char *shift_main = argv[29], *shift_dub = argv[30], *gap_main = argv[31], *gap_dub = argv[32], *two_en = argv[33];
  const char *step_main = argv[34], *step_dub = argv[35];
  dts_engine_set_lang_match(lang_match);
  load_refs(other, "pol");

  /* 1. Preferred language, main audio hidden, 8 MiB ring for the other file. */
  DtsEngineRings rings;
  dts_engine_rings(&rings, 1);
  DtsEngine *e = dts_engine_create();
  dts_engine_external_audio(e, other, HEADERS, "de,pl", 0);
  if (dts_engine_open(e, main_file, HEADERS, -1, 0, 0) < 0) {
    fprintf(stderr, "open: %s\n", dts_engine_error(e)); return 1;
  }
  const DtsMediaInfo *info = dts_engine_info(e);
  assert(info->audio_stream == DTS_ENGINE_EXTERNAL_STREAM);
  assert(!strcmp(info->audio_codec, "ac3") && info->channels == 6 && info->sample_rate == 48000);
  /* Every audio track of the other version is listed for the menu: the one
   * playing as DTS_ENGINE_EXTERNAL_STREAM, the English one as + 1 + index. */
  int audio_tracks = 0, playing = 0, other_eng = 0;
  for (int i = 0; i < info->n_tracks; i++)
    if (info->tracks[i].kind == DTS_AUDIO) {
      audio_tracks++;
      if (info->tracks[i].stream_index == DTS_ENGINE_EXTERNAL_STREAM)
        playing += !strcmp(info->tracks[i].language, "pol");
      else other_eng += info->tracks[i].stream_index == DTS_ENGINE_EXTERNAL_STREAM + 1 + 1 &&
                        !strcmp(info->tracks[i].language, "eng") && !strcmp(info->tracks[i].codec, "eac3");
    }
  assert(audio_tracks == 2 && playing == 1 && other_eng == 1);
  Run r = play(e, 0, 0);
  assert(r.video == 100 && r.subs >= 1);
  assert(r.first_audio >= 0 && r.first_audio < 40000000);
  assert(r.last_audio > 3900000000LL);
  assert(r.worst_lead <= 500000000LL);
  dts_engine_rings(&rings, 0);
  assert(rings.peak == 1 && rings.peak_bytes == 8LL * 1024 * 1024);
  printf("plain: %d video, %d audio, worst lead %.0f ms, ring peak %lld MiB\n", r.video, r.audio,
         r.worst_lead / 1e6, rings.peak_bytes >> 20);

  /* 2. Seek with a negative offset (the other version runs ahead). */
  dts_engine_external_offset(e, -500);
  assert(dts_engine_seek(e, 2.0) == 0);
  r = play(e, -500000000LL, 0);
  assert(r.first_audio >= 1950000000LL && r.first_audio < 2050000000LL);
  assert(r.last_audio <= 4000000000LL);
  printf("seek 2.0 s, offset -500 ms: first audio at %.3f s\n", r.first_audio / 1e9);

  /* 3. A track change of the main file is refused while external is on. */
  assert(dts_engine_audio(e, 1, 0, 0) < 0);
  dts_engine_destroy(e);

  /* 4. Positive offset from the start of playback, open at 1.0 s. */
  e = dts_engine_create();
  dts_engine_external_audio(e, other, HEADERS, "pl", 1000);
  if (dts_engine_open(e, main_file, HEADERS, -1, 0, 1.0) < 0) {
    fprintf(stderr, "open: %s\n", dts_engine_error(e)); return 1;
  }
  r = play(e, 1000000000LL, 0);
  assert(r.first_audio >= 990000000LL && r.first_audio < 1100000000LL);
  printf("open at 1.0 s, offset +1000 ms: first audio at %.3f s\n", r.first_audio / 1e9);
  dts_engine_destroy(e);

  /* 5. No preferred language present: the first usable track (E-AC-3, eng). */
  e = dts_engine_create();
  dts_engine_external_audio(e, other, HEADERS, "fr", 0);
  assert(dts_engine_open(e, main_file, HEADERS, -1, 0, 0) == 0);
  assert(!strcmp(dts_engine_info(e)->audio_codec, "eac3"));
  dts_engine_destroy(e);

  /* 6. Only DTS in the other file: decoded and re-encoded, stereo AAC by
   * default and AC-3 with the 5.1 setting; seek and offset still apply. */
  checkBytes = 0;
  for (int ac3 = 0; ac3 < 2; ac3++) {
    dts_engine_set_ac3(ac3);
    e = dts_engine_create();
    dts_engine_external_audio(e, dts_only, HEADERS, "", 250);
    if (dts_engine_open(e, main_file, HEADERS, -1, 0, 0) < 0) { fprintf(stderr, "open: %s\n", dts_engine_error(e)); return 1; }
    assert(!strcmp(dts_engine_info(e)->audio_codec, ac3 ? "ac3" : "aac"));
    assert(dts_engine_info(e)->sample_rate == 48000);
    int dtsTrack = 0;
    for (int i = 0; i < dts_engine_info(e)->n_tracks; i++)
      if (dts_engine_info(e)->tracks[i].kind == DTS_AUDIO) { dtsTrack = !strcmp(dts_engine_info(e)->tracks[i].codec, "dts"); }
    assert(dtsTrack);   /* the menu shows the source codec */
    r = play(e, 250000000LL, 0);
    assert(r.video == 100 && r.audio > 100);
    assert(r.first_audio >= 200000000LL && r.first_audio < 330000000LL);
    assert(r.last_audio > 3800000000LL && r.worst_lead <= 500000000LL);
    assert(dts_engine_seek(e, 2.0) == 0);
    r = play(e, 250000000LL, 0);
    assert(r.first_audio >= 1950000000LL && r.first_audio < 2100000000LL);
    printf("DTS donor -> %s: %d audio packets, seek 2.0 s first at %.3f s\n", ac3 ? "AC-3" : "AAC", r.audio, r.first_audio / 1e9);
    dts_engine_destroy(e);
  }
  dts_engine_set_ac3(0);
  /* DTS conversion off in Settings: the DTS donor is refused. */
  dts_engine_set_dts_off(1);
  e = dts_engine_create();
  dts_engine_external_audio(e, dts_only, HEADERS, "", 0);
  assert(dts_engine_open(e, main_file, HEADERS, -1, 0, 0) < 0);
  assert(strstr(dts_engine_error(e), "External audio: no audio track"));
  dts_engine_destroy(e);
  dts_engine_set_dts_off(0);
  checkBytes = 1;

  /* 7. A much shorter other file (a debrid "not cached" clip) is refused. */
  e = dts_engine_create();
  dts_engine_external_audio(e, short_clip, HEADERS, "", 0);
  assert(dts_engine_open(e, main_file, HEADERS, -1, 0, 0) < 0);
  assert(strstr(dts_engine_error(e), "not cached"));
  dts_engine_destroy(e);
  /* 7b. The same short clip with an audio track for an 11-minute picture (owner's
   * log 13/11: TorBox's 30 s clip, even for a version marked cached): said
   * as the debrid placeholder, the case the player tries again. The header
   * read of the hold-OK track list says it too (-3), a real file its tracks. */
  e = dts_engine_create();
  dts_engine_external_audio(e, short_clip, HEADERS, "", 0);
  assert(dts_engine_open(e, film, HEADERS, -1, 0, 0) < 0);
  assert(strstr(dts_engine_error(e), "(placeholder clip)"));
  dts_engine_destroy(e);
  { DtsTrack tk[8];
    assert(dts_engine_probe_audio(short_clip, HEADERS, tk, 8) == -3);
    assert(dts_engine_probe_audio(film, HEADERS, tk, 8) >= 1); }

  /* 8. Audio only: an MP4 donor fetches its audio chunks, not its video. */
  {
    int64_t total = 0, bytes;
    load_refs(mp4, "pol");
    for (int only = 0; only < 2; only++) {
      e = dts_engine_create();
      dts_engine_external_audio(e, mp4, HEADERS, "pl", 0);
      dts_engine_external_audio_only(e, only);
      if (dts_engine_open(e, main_file, HEADERS, -1, 0, 0) < 0) { fprintf(stderr, "open: %s\n", dts_engine_error(e)); return 1; }
      r = play(e, 0, 0);
      assert(r.video == 100 && r.audio > 100 && r.last_audio > 3800000000LL);
      bytes = dts_engine_external_bytes(e, &total);
      printf("MP4 donor %s: %lld of %lld bytes downloaded\n", only ? "audio only" : "whole file", (long long)bytes, (long long)total); fflush(stdout);
      /* The header read at open fills the donor's 8 MiB ring once (nothing
       * next to a real file); after it only the audio chunks are fetched, so
       * the 25 MiB donor stays well under half. */
      if (only) assert(bytes < total / 2 && bytes < (int64_t)(DTS_ENGINE_EXTERNAL_RING_MIB + 2) * 1048576);
      /* A seek back reads the earlier chunks again. */
      assert(dts_engine_seek(e, 1.0) == 0);
      r = play(e, 0, 0);
      assert(r.first_audio >= 950000000LL && r.first_audio < 1100000000LL);
      dts_engine_destroy(e);
    }
    /* Matroska has no audio index: audio only reads the whole file. */
    load_refs(other, "pol");
    e = dts_engine_create();
    dts_engine_external_audio(e, other, HEADERS, "pl", 0);
    dts_engine_external_audio_only(e, 1);
    assert(dts_engine_open(e, main_file, HEADERS, -1, 0, 0) == 0);
    r = play(e, 0, 0);
    assert(r.audio > 100);
    dts_engine_destroy(e);
  }

  /* 9. AAC at 44.1 kHz (a common WEB-DL dub) is converted, where the build
   * has the AAC decoder (the TV and CI builds do). */
  if (avcodec_find_decoder(AV_CODEC_ID_AAC)) {
    checkBytes = 0;
    e = dts_engine_create();
    dts_engine_external_audio(e, aac44, HEADERS, "pl", 0);
    if (dts_engine_open(e, main_file, HEADERS, -1, 0, 0) < 0) { fprintf(stderr, "open: %s\n", dts_engine_error(e)); return 1; }
    assert(!strcmp(dts_engine_info(e)->audio_codec, "aac") && dts_engine_info(e)->sample_rate == 48000);
    r = play(e, 0, 0);
    assert(r.audio > 100 && r.last_audio > 3800000000LL);
    printf("AAC 44.1 kHz donor converted: %d packets\n", r.audio);
    dts_engine_destroy(e);
    checkBytes = 1;
  } else printf("AAC decoder not in this FFmpeg build: 44.1 kHz case skipped\n");

  /* 10. Turning it off restores the main file's own track selection. */
  e = dts_engine_create();
  dts_engine_external_audio(e, other, HEADERS, "pl", 0);
  dts_engine_external_audio(e, NULL, NULL, NULL, 0);
  assert(!dts_engine_external_active(e));
  assert(dts_engine_open(e, main_file, HEADERS, -1, 0, 0) == 0);
  assert(dts_engine_info(e)->audio_stream == 1);
  dts_engine_destroy(e);

  dts_engine_rings(&rings, 0);
  assert(rings.alive == 0 && rings.bytes == 0);
  assert(streams_opened >= 4);
  /* Track picked in the menu: the English E-AC-3 (stream 1) over the
   * preferred Polish one. */
  e = dts_engine_create();
  dts_engine_external_audio(e, other, HEADERS, "pl", 0);
  dts_engine_external_track(e, 1);
  if (dts_engine_open(e, main_file, HEADERS, -1, 0, 0) < 0) { fprintf(stderr, "open: %s\n", dts_engine_error(e)); return 1; }
  info = dts_engine_info(e);
  assert(!strcmp(info->audio_codec, "eac3") && info->channels == 2);
  load_refs(other, "eng");   /* the packets are the English track's, byte for byte */
  r = play(e, 0, 0);
  assert(r.video == 100 && r.audio > 100);
  dts_engine_destroy(e);
  /* Swap of the other version mid-film: this source stays open (no new
   * open of the main file), the new file's track plays from the position. */
  e = dts_engine_create();
  dts_engine_external_audio(e, other, HEADERS, "pl", 0);
  if (dts_engine_open(e, main_file, HEADERS, -1, 0, 0) < 0) { fprintf(stderr, "open: %s\n", dts_engine_error(e)); return 1; }
  load_refs(other, "pol");
  r = play(e, 0, 40);
  assert(r.video > 10);
  assert(dts_engine_external_replace(e, other, HEADERS, "pl", 0, 1, 0, 2.0) == 0);
  info = dts_engine_info(e);
  assert(!strcmp(info->audio_codec, "eac3") && info->audio_stream == DTS_ENGINE_EXTERNAL_STREAM);
  { int n_audio = 0;
    for (int i = 0; i < info->n_tracks; i++) n_audio += info->tracks[i].kind == DTS_AUDIO;
    assert(n_audio == 2); }   /* rebuilt, not doubled */
  load_refs(other, "eng");
  r = play(e, 0, 0);
  assert(r.audio > 20 && r.first_audio >= 1950000000LL && r.first_audio < 2100000000LL);
  assert(r.last_audio > 3800000000LL);
  /* Without an open session the swap refuses (the app starts a new one). */
  DtsEngine *closed = dts_engine_create();
  assert(dts_engine_external_replace(closed, other, HEADERS, "pl", 0, -1, 0, 0) < 0);
  dts_engine_destroy(closed);
  dts_engine_destroy(e);
  /* Live offset (CH+/CH-): no seek of the main source, the audio timestamps
   * continue where they were and the content is the other file's 0.5 s
   * earlier (offset +500 ms = audio later). */
  e = dts_engine_create();
  dts_engine_external_audio(e, other, HEADERS, "pl", 0);
  if (dts_engine_open(e, main_file, HEADERS, -1, 0, 0) < 0) { fprintf(stderr, "open: %s\n", dts_engine_error(e)); return 1; }
  load_refs(other, "pol");
  { Run a = play(e, 0, 60);
    assert(a.audio > 5);
    assert(dts_engine_external_shift(e, 500) == 0);
    Run b = play(e, 500000000LL, 0);      /* bytes checked against the file 0.5 s earlier */
    assert(b.audio > 20 && b.first_audio > a.last_audio && b.first_audio - a.last_audio < 100000000LL);
    assert(b.video > 0 && b.last_audio > 3800000000LL); }
  dts_engine_destroy(e);
  /* Seeking the other version to 50 s of 60 reads around the target, not the
   * file from the start: with its index (video Cues) and, without one, by a
   * byte estimate (owner's logs 07/11: up to 162 s of black at 6187 s). */
  checkBytes = 0;
  for (int v = 0; v < 2; v++) {
    e = dts_engine_create();
    dts_engine_external_audio(e, v ? long_noidx : long_idx, HEADERS, "pl", 0);
    if (dts_engine_open(e, main60, HEADERS, -1, 0, 0) < 0) { fprintf(stderr, "open: %s\n", dts_engine_error(e)); return 1; }
    int64_t total = 0, before = dts_engine_external_bytes(e, &total);
    assert(dts_engine_seek(e, 50.0) == 0);
    r = play(e, 0, 60);
    int64_t after = dts_engine_external_bytes(e, &total);
    fprintf(stderr, "seek 50 s, %s: %lld of %lld bytes read (first audio %.2f s)\n", v ? "no index" : "index",
            (long long)(after - before), (long long)total, r.first_audio / 1e9);
    assert(r.audio > 5 && r.first_audio >= 49900000000LL / 1000 && r.first_audio < 51000000000LL);
    /* Fixed costs (index probe, cluster search, read-ahead) are a few MiB:
     * a lot next to this 50 MB file, nothing next to a real one. */
    assert(after - before < (v ? total / 2 : total / 8));
    dts_engine_destroy(e);
  }
  checkBytes = 1;
  /* "Match automatically": the dub's bed is 0.7 s late, so its timestamps
   * need -700 ms; found from both audios while playing. */
  e = dts_engine_create();
  dts_engine_external_audio(e, match_dub, HEADERS, "pl", 0);
  if (dts_engine_open(e, match_main, HEADERS, -1, 0, 0) < 0) { fprintf(stderr, "open: %s\n", dts_engine_error(e)); return 1; }
  { DtsMatchInfo mi;
    assert(dts_engine_external_match_poll(e, &mi) == 0);
    assert(dts_engine_external_match_arm(e) == 0);
    checkBytes = 0;
    r = play(e, 0, 0);
    checkBytes = 1;
    int res = dts_engine_external_match_poll(e, &mi);
    fprintf(stderr, "match: result %d offset %d ms score %d how %d\n", res, mi.offset_ms, mi.score, mi.how);
    assert(res == 1 && mi.offset_ms > -740 && mi.offset_ms < -660 && mi.how == 1 && mi.ratio_ppm == 0);
    assert(dts_engine_external_match_poll(e, &mi) == 0);   /* reported once */
    assert(!mi.automatic); }
  dts_engine_destroy(e);
  /* Settings: "Match automatically at start": the same find, by itself, marked automatic. */
  e = dts_engine_create();
  dts_engine_external_automatch(e, 1);
  dts_engine_external_audio(e, match_dub, HEADERS, "pl", 0);
  assert(dts_engine_open(e, match_main, HEADERS, -1, 0, 0) == 0);
  { DtsMatchInfo mi; DtsFrame f; int got, res = 0, autom = 0, off = 0;
    checkBytes = 0;
    while ((got = dts_engine_next(e, &f)) == 1)
      if (!res && (res = dts_engine_external_match_poll(e, &mi)) != 0) { autom = mi.automatic; off = mi.offset_ms; }
    checkBytes = 1;
    fprintf(stderr, "match at start: result %d offset %d ms automatic %d\n", res, off, autom);
    assert(got == 0 && res == 1 && autom && off > -740 && off < -660); }
  dts_engine_destroy(e);
  /* The other version goes quiet mid-file (owner's log 14/11: the converter,
   * picture included, waited on it until a track change, which waited too).
   * A request stops that wait at once; the reader reconnects at the same
   * place EXT_STALL_TRIES times, then gives up and the picture plays on. */
  /* A Torrentio link (owner, 14/11): three redirects before the CDN at every
   * open. The probe's or the first reader's final address serves the next
   * engines of that version; a dead one goes back through the add-on; a slow
   * reopen forgets it. */
  checkBytes = 0;
  { char via[4200]; DtsTrack tr[4];
    snprintf(via, sizeof via, "addon:%s", long_idx);
    unsigned h0 = addon_hits;
    (void)tr;   /* the probe opens the same way; the fixtures are under its 2 min */
    e = dts_engine_create();
    dts_engine_external_audio(e, via, HEADERS, "pl", 0);
    assert(dts_engine_open(e, main60, HEADERS, -1, 0, 0) == 0);
    r = play(e, 0, 5);
    assert(r.audio > 0 && addon_hits == h0 + 1);   /* the first open pays the redirects */
    dts_engine_destroy(e);
    e = dts_engine_create();
    dts_engine_external_audio(e, via, HEADERS, "pl", 0);
    assert(dts_engine_open(e, main60, HEADERS, -1, 0, 0) == 0);
    r = play(e, 0, 5);
    assert(r.audio > 0 && addon_hits == h0 + 1);   /* straight to the server */
    dts_engine_destroy(e);
    cdn_dead = 1;
    e = dts_engine_create();
    dts_engine_external_audio(e, via, HEADERS, "pl", 0);
    assert(dts_engine_open(e, main60, HEADERS, -1, 0, 0) == 0);
    r = play(e, 0, 5);
    assert(r.audio > 0 && addon_hits == h0 + 2 && !cdn_dead);   /* gone: the add-on again */
    dts_engine_destroy(e);
    dts_engine_forget_final(via, HEADERS);
    e = dts_engine_create();
    dts_engine_external_audio(e, via, HEADERS, "pl", 0);
    assert(dts_engine_open(e, main60, HEADERS, -1, 0, 0) == 0);
    r = play(e, 0, 5);
    assert(r.audio > 0 && addon_hits == h0 + 3);   /* forgotten: through the add-on */
    dts_engine_destroy(e);
    /* A short clip behind the add-on (the debrid placeholder, owner's log
     * 14/11): its address is not kept, every retry asks the add-on again. */
    snprintf(via, sizeof via, "addon:%s", short_clip);
    h0 = addon_hits;
    for (int t = 0; t < 2; t++) {
      e = dts_engine_create();
      dts_engine_external_audio(e, via, HEADERS, "pl", 0);
      assert(dts_engine_open(e, main60, HEADERS, -1, 0, 0) < 0);
      dts_engine_destroy(e);
    }
    assert(addon_hits == h0 + 2);
    fprintf(stderr, "remembered server address: first open pays the redirects, the next goes straight, a dead one, a forgotten one and a placeholder go through the add-on\n");
  }
  { DtsFrame f; int got, video, audio, opened;
    snprintf(hang_url, sizeof hang_url, "%s", long_idx);
    /* Woken: the read stops at once, the picture goes on without the other
     * audio, the seek after the request reads it again. */
    e = dts_engine_create();
    dts_engine_external_audio(e, long_idx, HEADERS, "pl", 0);
    assert(dts_engine_open(e, main60, HEADERS, -1, 0, 0) == 0);
    int64_t total = 0; dts_engine_external_bytes(e, &total); assert(total > 16 * 1024 * 1024);
    __atomic_store_n(&hang_at, total / 2, __ATOMIC_RELAXED);
    pthread_t waker; Wake w = { e, 0 };
    assert(pthread_create(&waker, NULL, wake_when_waiting, &w) == 0);
    video = audio = 0;
    while ((got = dts_engine_next(e, &f)) == 1 && !dts_engine_external_woken(e)) {}
    pthread_join(waker, NULL);
    assert(got == 1 && dts_engine_external_woken(e) && !dts_engine_external_failed(e));
    double waited = w.waited;
    for (int i = 0; i < 50 && (got = dts_engine_next(e, &f)) == 1; i++) {
      video += f.kind == DTS_VIDEO; audio += f.kind == DTS_AUDIO;
    }
    assert(got == 1 && video == 50 && audio == 0);   /* the picture moves on alone */
    __atomic_store_n(&hang_at, 0, __ATOMIC_RELAXED);
    assert(dts_engine_seek(e, 40.0) == 0 && !dts_engine_external_woken(e));
    r = play(e, 0, 60);
    assert(r.audio > 5 && r.first_audio >= 39900000000LL / 1000);
    fprintf(stderr, "quiet other version: woken after %.1f s waiting, %d video packets alone, audio back after the seek\n",
            waited, video);
    dts_engine_destroy(e);
    /* Nobody asks: reconnections at the same place, then it gives up; the
     * picture plays to the end, the failure reported for the app. */
    e = dts_engine_create();
    dts_engine_external_audio(e, long_idx, HEADERS, "pl", 0);
    assert(dts_engine_open(e, main60, HEADERS, -1, 0, 0) == 0);
    __atomic_store_n(&hang_at, total / 2, __ATOMIC_RELAXED);
    opened = (int)__atomic_load_n(&streams_opened, __ATOMIC_RELAXED);
    struct timespec t0, t1; clock_gettime(CLOCK_MONOTONIC, &t0);
    video = audio = 0;
    while ((got = dts_engine_next(e, &f)) == 1) { video += f.kind == DTS_VIDEO; audio += f.kind == DTS_AUDIO; }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double took = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
    int reconnected = (int)__atomic_load_n(&streams_opened, __ATOMIC_RELAXED) - opened;
    fprintf(stderr, "quiet other version, nobody asks: %d reconnections, gave up in %.1f s, %d video %d audio\n",
            reconnected, took, video, audio);
    assert(got == 0 && dts_engine_external_failed(e) && video == 1500 && audio > 0);
    assert(reconnected >= 3 && took >= 3 * EXT_STALL_S && took < 6 * EXT_STALL_S);
    __atomic_store_n(&hang_at, 0, __ATOMIC_RELAXED);
    dts_engine_destroy(e);
  }
  /* A trickling connection (owner's logs 14/11): the speed guard reconnects
   * at the same place, the second time with a fresh link; the third
   * connection is a good one and everything plays to the end. The other
   * version first, then the picture source of a conversion. */
  { Run tr; struct timespec t0, t1; double took;
    snprintf(trickle_url, sizeof trickle_url, "%s", long_idx);
    e = dts_engine_create();
    dts_engine_external_audio(e, long_idx, HEADERS, "pl", 0);
    assert(dts_engine_open(e, main60, HEADERS, -1, 0, 0) == 0);
    int64_t total = 0; dts_engine_external_bytes(e, &total);
    trickle_hits = 0; trickle_left = 2; __atomic_store_n(&trickle_at, total / 2, __ATOMIC_RELAXED);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    tr = play(e, 0, 0);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    took = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
    fprintf(stderr, "trickling other version: %d slow connections, %.1f s, %d video %d audio, last audio %.1f s\n",
            trickle_hits, took, tr.video, tr.audio, tr.last_audio / 1e9);
    assert(trickle_hits == 2 && !dts_engine_external_failed(e) && tr.video == 1500);
    assert(tr.last_audio > tr.max_video - 2000000000LL && took < 6.0 * SG_WINDOW_MS / 1000);
    dts_engine_destroy(e);
    __atomic_store_n(&trickle_at, 0, __ATOMIC_RELAXED);
    dts_engine_set_conversion_stream(1);
    dts_engine_set_ahead_mib(8);
    e = dts_engine_create();
    dts_engine_external_audio(e, main60, HEADERS, "", 0);
    trickle_hits = 0; trickle_left = 2;
    { FILE *lf = fopen(long_idx, "rb"); assert(lf); fseeko(lf, 0, SEEK_END); total = ftello(lf); fclose(lf); }
    __atomic_store_n(&trickle_at, total / 2, __ATOMIC_RELAXED);
    assert(dts_engine_open(e, long_idx, HEADERS, -1, 0, 0) == 0);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    tr = play(e, 0, 0);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    took = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
    fprintf(stderr, "trickling picture source: %d slow connections, %.1f s, %d video, last video %.1f s\n",
            trickle_hits, took, tr.video, tr.max_video / 1e9);
    assert(trickle_hits == 2 && tr.video > 0 && took < 6.0 * SG_WINDOW_MS / 1000);
    dts_engine_destroy(e);
    __atomic_store_n(&trickle_at, 0, __ATOMIC_RELAXED);
    dts_engine_set_ahead_mib(0);
    dts_engine_set_conversion_stream(0);
  }
  checkBytes = 1;
  /* AVI and MPEG-TS (owner's log 07/11: a 4.3 GB AVI picture source; small
   * AVI/TS versions with other audio). The picture from an H.264 AVI, the
   * audio from an AVI (MP3) and a TS (AC-3) version; seek in both. */
  checkBytes = 0;
  const char *donors[] = { other_avi, other_ts };
  for (int d = 0; d < 2; d++) {
    e = dts_engine_create();
    dts_engine_external_audio(e, donors[d], HEADERS, "pl", 0);
    if (dts_engine_open(e, d ? main_file : main_avi, HEADERS, -1, 0, 0) < 0) {
      fprintf(stderr, "open %s: %s\n", d ? "ts" : "avi", dts_engine_error(e)); return 1;
    }
    r = play(e, 0, 0);
    assert(r.video == 100 && r.audio > 50);
    assert(r.first_audio >= 0 && r.first_audio < 200000000LL && r.last_audio > 3700000000LL);
    assert(dts_engine_seek(e, 2.0) == 0);
    r = play(e, 0, 0);
    assert(r.audio > 20 && r.first_audio >= 1900000000LL && r.first_audio < 2200000000LL);
    printf("%s: picture %s, audio %s, seek 2.0 s first audio at %.3f s\n", d ? "TS donor" : "AVI main + AVI donor",
           d ? "MKV" : "AVI", dts_engine_info(e)->audio_codec, r.first_audio / 1e9);
    dts_engine_destroy(e);
  }
  /* An old XviD picture: a clear refusal, not "codec unsupported". */
  e = dts_engine_create();
  dts_engine_external_audio(e, other, HEADERS, "pl", 0);
  assert(dts_engine_open(e, main_xvid, HEADERS, -1, 0, 0) < 0);
  assert(strstr(dts_engine_error(e), "XviD"));
  dts_engine_destroy(e);
  /* A placeholder clip as the picture source, a full version for the audio. */
  e = dts_engine_create();
  dts_engine_external_audio(e, film, HEADERS, "pl", 0);
  assert(dts_engine_open(e, short_clip, HEADERS, -1, 0, 0) < 0);
  assert(strstr(dts_engine_error(e), "placeholder"));
  dts_engine_destroy(e);
  checkBytes = 1;
  /* Another cut: the other version has 40 s more at the start. Not near
   * the current offset: a reader of its own tries what the lengths say. */
  checkBytes = 0;
  e = dts_engine_create();
  dts_engine_external_audio(e, cut_dub, HEADERS, "pl", 0);
  if (dts_engine_open(e, cut_main, HEADERS, -1, 0, 0) < 0) { fprintf(stderr, "open: %s\n", dts_engine_error(e)); return 1; }
  { DtsMatchInfo mi; int res = 0;
    assert(dts_engine_external_match_arm(e) == 0);
    r = play(e, 0, 0);
    for (int i = 0; i < 600 && !(res = dts_engine_external_match_poll(e, &mi)); i++) usleep(50000);
    fprintf(stderr, "another cut: result %d offset %d ms how %d score %d\n", res, mi.offset_ms, mi.how, mi.score);
    assert(res == 1 && mi.how == 2 && mi.offset_ms > -40080 && mi.offset_ms < -39920 && mi.ratio_ppm == 0); }
  dts_engine_destroy(e);
  /* The same language in both files: the other version plays Polish, its
   * English (3.2 s later) is compared with this file's English. */
  e = dts_engine_create();
  dts_engine_external_audio(e, pair_dub, HEADERS, "pl", 0);
  assert(dts_engine_open(e, cut_main, HEADERS, -1, 0, 0) == 0);
  { DtsMatchInfo mi; int res = 0;
    assert(dts_engine_external_match_arm(e) == 0);
    r = play(e, 0, 0);
    for (int i = 0; i < 200 && !(res = dts_engine_external_match_poll(e, &mi)); i++) usleep(50000);
    fprintf(stderr, "same language: result %d offset %d ms how %d score %d\n", res, mi.offset_ms, mi.how, mi.score);
    assert(res == 1 && mi.how == 1 && mi.offset_ms > -3240 && mi.offset_ms < -3160 && mi.score >= 60); }
  dts_engine_destroy(e);
  /* Following another cut: the extended picture has 25 s at 60 s the
   * theatrical audio lacks. The first match starts by itself; in those 25 s
   * this file's own audio plays (timestamps continuous), then the other
   * version resumes 25 s later. */
  e = dts_engine_create();
  dts_engine_external_follow(e, 1);
  dts_engine_external_audio(e, follow_th, HEADERS, "en", 0);
  assert(dts_engine_open(e, follow_ext, HEADERS, -1, 0, 0) == 0);
  assert(!strcmp(dts_engine_info(e)->audio_codec, "aac"));   /* converted, to switch source */
  { DtsFrame f; int got, fill = 0, resumed = 0, resume_ms = 0, state, ms;
    int64_t last = INT64_MIN, worst_gap = 0, fill_at = 0, resume_at = 0;
    DtsMatchInfo mi; int automatch = 0, found_ms = 0, shifted = 0, fills = 0;
    while ((got = dts_engine_next(e, &f)) == 1) {
      if (f.kind == DTS_AUDIO) {
        assert(last == INT64_MIN || f.pts_ns > last);
        if (last != INT64_MIN && f.pts_ns - last > worst_gap) worst_gap = f.pts_ns - last;
        last = f.pts_ns;
      }
      int k = dts_engine_external_follow_poll(e, &state, &ms);
      if (k == 1 && !fill) { fill = 1; fill_at = last; }
      if (k == 2 && !resumed) { resumed = 1; resume_ms = ms; resume_at = last; }
      if (dts_engine_external_match_poll(e, &mi) == 1 && mi.automatic) { automatch = 1; found_ms = mi.offset_ms; }
      /* Owner's log 08/11 (Chicago Fire): a new find moves the other version
       * live by a few ms; that must not read as a missing scene ~15 s later. */
      if (resumed && !shifted && last > resume_at + 2e9) {
        shifted = 1; assert(dts_engine_external_shift(e, resume_ms + 6) == 0);
      }
      if (k == 1) fills++;
    }
    assert(got == 0);
    fprintf(stderr, "follow: auto match %d, own audio at %.1f s, resumed at %.1f s with %d ms, last audio %.1f s, worst gap %.0f ms\n",
            automatch, fill_at / 1e9, resume_at / 1e9, resume_ms, last / 1e9, worst_gap / 1e6);
    assert(automatch && shifted && fill && resumed && fills == 1);
    (void)found_ms;
    assert(fill_at > 60e9 && fill_at < 72e9);
    assert(resume_at > 85e9 && resume_at < 110e9);
    assert(resume_ms > 24900 && resume_ms < 25100);
    assert(last > 118e9 && worst_gap < 200000000LL); }
  dts_engine_destroy(e);
  /* Following, the same cut in other languages: this file's Russian dub
   * (default) and English original against a Polish voice-over on the
   * original, 30 s longer (credits). The English track is the one compared,
   * and dialogue never counts as a missing scene. */
  e = dts_engine_create();
  dts_engine_external_follow(e, 1);
  dts_engine_external_audio(e, lang_pl, HEADERS, "pl", 0);
  assert(dts_engine_open(e, lang_main, HEADERS, -1, 0, 0) == 0);
  { DtsFrame f; int got, switches = 0, state, ms, found = 0; DtsMatchInfo mi;
    while ((got = dts_engine_next(e, &f)) == 1) {
      int k = dts_engine_external_follow_poll(e, &state, &ms);
      if (k == 1 || k == 2) switches++;
      if (dts_engine_external_match_poll(e, &mi) == 1) found = 1;
    }
    fprintf(stderr, "follow, other languages: found %d, switches %d\n", found, switches);
    assert(got == 0 && found && switches == 0); }
  dts_engine_destroy(e);
  /* Only two dubs (Russian, Polish) sharing the effects: a weak find, and
   * the dialogue alone must not count as a missing scene. */
  e = dts_engine_create();
  dts_engine_external_follow(e, 1);
  dts_engine_external_audio(e, dub_pl, HEADERS, "pl", 0);
  assert(dts_engine_open(e, dub_ru, HEADERS, -1, 0, 0) == 0);
  { DtsFrame f; int got, switches = 0, state, ms, found = 0; DtsMatchInfo mi;
    while ((got = dts_engine_next(e, &f)) == 1) {
      int k = dts_engine_external_follow_poll(e, &state, &ms);
      if (k == 1 || k == 2) switches++;
      if (dts_engine_external_match_poll(e, &mi) == 1) found = 1;
    }
    fprintf(stderr, "follow, two dubs: found %d, switches %d\n", found, switches);
    assert(got == 0 && found && switches == 0); }
  dts_engine_destroy(e);
  /* The match at start cut short by a jump (owner's log 11/11, S9E16: the
   * automatic intro skip): no failure; it starts again from there and finds. */
  e = dts_engine_create();
  dts_engine_external_automatch(e, 1);
  dts_engine_external_audio(e, gap_dub, HEADERS, "en", 0);
  assert(dts_engine_open(e, gap_main, HEADERS, -1, 0, 0) == 0);
  { DtsFrame f; DtsMatchInfo mi; int got, fails = 0, found = 0, autom = 0, off = 99999, jumped = 0;
    while ((got = dts_engine_next(e, &f)) == 1) {
      if (!jumped && f.kind == DTS_AUDIO && f.pts_ns > 5e9) { jumped = 1; assert(dts_engine_seek(e, 60.0) == 0); continue; }
      int r = dts_engine_external_match_poll(e, &mi);
      if (r < 0) fails++;
      if (r == 1 && !found) { found = 1; autom = mi.automatic; off = mi.offset_ms; }
      if (found && f.kind == DTS_AUDIO && f.pts_ns > 130e9) break;
    }
    fprintf(stderr, "match at start, jump: found %d (automatic %d, %d ms), failures %d\n", found, autom, off, fails);
    assert(jumped && found && autom && !fails && off > -60 && off < 60); }
  dts_engine_destroy(e);
  /* Series memory, the track (owner 09/11): two English tracks; without a
   * hint the first, with the remembered one ("Commentary", stream 2) that one. */
  for (int hint = 0; hint < 2; hint++) {
    e = dts_engine_create();
    dts_engine_external_audio(e, two_en, HEADERS, "en", 0);
    if (hint) dts_engine_external_track_hint(e, 2, "en", "ac3", 2, "Commentary");
    assert(dts_engine_open(e, main_file, HEADERS, -1, 0, 0) == 0);
    const DtsMediaInfo *in = dts_engine_info(e);
    const char *chosen = "";
    for (int i = 0; i < in->n_tracks; i++)
      if (in->tracks[i].stream_index == DTS_ENGINE_EXTERNAL_STREAM) chosen = in->tracks[i].title;
    fprintf(stderr, "two English tracks, hint %d: %s\n", hint, chosen);
    assert(!strcmp(chosen, hint ? "Commentary" : "Main"));
    dts_engine_destroy(e);
  }
  /* A gap 0.6 s longer in the other version at 70 s: the follow moves the
   * sync there by itself (event 4), no switch to this file's audio. */
  e = dts_engine_create();
  dts_engine_external_follow(e, 1);
  dts_engine_external_audio(e, shift_dub, HEADERS, "en", 0);
  assert(dts_engine_open(e, shift_main, HEADERS, -1, 0, 0) == 0);
  { DtsFrame f; int got, fills = 0, shifts = 0, shift_ms = 0, state, ms; int64_t last = INT64_MIN, shift_at = 0;
    while ((got = dts_engine_next(e, &f)) == 1) {
      if (f.kind == DTS_AUDIO) last = f.pts_ns;
      int k = dts_engine_external_follow_poll(e, &state, &ms);
      if (k == 1) fills++;
      if (k == 4 && !shifts++) { shift_ms = ms; shift_at = last; }
    }
    fprintf(stderr, "follow, small shift: %d shift(s), first %d ms at %.1f s, own audio %d\n", shifts, shift_ms, shift_at / 1e9, fills);
    assert(got == 0 && fills == 0 && shifts >= 1 && shift_ms < -550 && shift_ms > -650 && shift_at > 70e9 && shift_at < 90e9); }
  dts_engine_destroy(e);
  /* The second look, same frame rate, a gap 0.5 s longer at 200 s: an
   * offset move (how 1, no speed), never a speed change (how 5). */
  e = dts_engine_create();
  dts_engine_external_audio(e, gap_dub, HEADERS, "en", 0);
  assert(dts_engine_open(e, gap_main, HEADERS, -1, 0, 0) == 0);
  { DtsFrame f; int got, results = 0, speed = 0, moved = 0, moved_ms = 0; DtsMatchInfo mi;
    assert(dts_engine_external_match_arm(e) == 0);
    while ((got = dts_engine_next(e, &f)) == 1) {
      if (dts_engine_external_match_poll(e, &mi) == 1) {
        results++;
        if (mi.how == 5 || mi.ratio_ppm) speed = 1;
        if (results >= 2) { moved = 1; moved_ms = mi.offset_ms; }
      }
    }
    fprintf(stderr, "second look, same frame rate: %d result(s), speed %d, moved to %d ms\n", results, speed, moved_ms);
    assert(got == 0 && !speed && moved && moved_ms < -450 && moved_ms > -550); }
  dts_engine_destroy(e);
  /* Versions whose gaps differ at each break (owner's log 16/11, Chicago
   * Fire): the lengths 1 s apart, so the small-shift follow starts with the
   * first find and the sync is also checked every few minutes. Each move
   * (0.5 s at 200 s and at 470 s) is found within seconds of its break, made
   * at pauses in steps, and the next one measured from it (-1000 ms, not the
   * first one again). */
  e = dts_engine_create();
  dts_engine_external_audio(e, step_dub, HEADERS, "en", 0);
  assert(dts_engine_open(e, step_main, HEADERS, -1, 0, 0) == 0);
  { DtsFrame f; int got, first = 0, nm = 0, moves[8] = { 0 }, state, ms; int64_t at[8] = { 0 }, last = INT64_MIN; DtsMatchInfo mi;
    assert(dts_engine_external_match_arm(e) == 0);
    while ((got = dts_engine_next(e, &f)) == 1) {
      if (f.kind == DTS_VIDEO) last = f.pts_ns;
      int r = dts_engine_external_match_poll(e, &mi), k = dts_engine_external_follow_poll(e, &state, &ms);
      assert(k != 1);   /* never this file's own audio */
      if (r == 1 && !mi.later) { first++; assert(dts_engine_external_shift(e, mi.offset_ms) == 0); }
      int found = r == 1 && mi.later ? mi.offset_ms : k == 4 ? ms : 0;
      if (r == 1 && mi.later) {
        assert(dts_engine_external_shift_quiet(e, mi.offset_ms) == 0);
        assert(dts_engine_external_shift_quiet(e, mi.offset_ms) == 0);   /* asked again: goes on */
      }
      if (found && (nm == 0 || abs(found - moves[nm - 1]) >= 40) && nm < 8) { at[nm] = last; moves[nm++] = found; }
    }
    fprintf(stderr, "versions whose gaps differ: first find %d, moves %d (%d ms at %.0f s, %d ms at %.0f s)\n",
            first, nm, moves[0], at[0] / 1e9, moves[1], at[1] / 1e9);
    assert(got == 0 && first == 1 && nm == 2);
    assert(moves[0] < -450 && moves[0] > -550 && at[0] > 200e9 && at[0] < 230e9);
    assert(moves[1] < -950 && moves[1] > -1050 && at[1] > 470e9 && at[1] < 500e9); }
  dts_engine_destroy(e);
  /* A real missing scene between two languages: found later (the slower
   * check), and the other version still resumes 25 s on. */
  e = dts_engine_create();
  dts_engine_external_follow(e, 1);
  dts_engine_external_audio(e, follow_th_pl, HEADERS, "pl", 0);
  assert(dts_engine_open(e, follow_ext_ru, HEADERS, -1, 0, 0) == 0);
  { DtsFrame f; int got, fill = 0, resumed = 0, resume_ms = 0, state, ms; int64_t last = INT64_MIN, fill_at = 0;
    while ((got = dts_engine_next(e, &f)) == 1) {
      if (f.kind == DTS_AUDIO) last = f.pts_ns;
      int k = dts_engine_external_follow_poll(e, &state, &ms);
      if (k == 1 && !fill) { fill = 1; fill_at = last; }
      if (k == 2 && !resumed) { resumed = 1; resume_ms = ms; }
    }
    fprintf(stderr, "follow, cut in two languages: own audio at %.1f s, resumed %d with %d ms\n", fill_at / 1e9, resumed, resume_ms);
    assert(got == 0 && fill && fill_at > 60e9 && fill_at < 80e9 && resumed && resume_ms > 24900 && resume_ms < 25100); }
  dts_engine_destroy(e);
  /* Prefetch ahead of a jump (an intro skip) on the one-connection stream
   * reader: the jump lands on the fetched bytes and plays on from there. */
  dts_engine_set_conversion_stream(1);
  dts_engine_set_ahead_mib(8);   /* a small ring: the jump lands past it, as on the TV */
  e = dts_engine_create();
  dts_engine_external_audio(e, main60, HEADERS, "", 0);
  assert(dts_engine_open(e, long_idx, HEADERS, -1, 0, 0) == 0);   /* 52 MB, heavy picture */
  r = play(e, 0, 50);
  assert(dts_engine_prefetch(e, 40.0) == 0);
  usleep(300000);
  dts_engine_prefetch_tidy(e);
  assert(dts_engine_seek(e, 40.0) == 0);
  /* Owner's log 11/11: right after a jump served from the prefetched bytes
   * the buffer reported 0 KiB ready and the rebuffer waited on it. */
  { DtsBufferInfo bi; dts_engine_buffer_info(e, &bi);
    fprintf(stderr, "prefetch: ready after the jump %lld KiB of %lld\n", (long long)(bi.contiguous_bytes / 1024), (long long)(bi.capacity_bytes / 1024));
    assert(bi.capacity_bytes > 0 && bi.contiguous_bytes >= 1024 * 1024); }
  r = play(e, 0, 200);
  fprintf(stderr, "prefetch: after the jump, first audio %.2f s, last video %.2f s\n", r.first_audio / 1e9, r.max_video / 1e9);
  assert(r.first_audio > 39500000000LL && r.first_audio < 40600000000LL && r.max_video > 40000000000LL);
  dts_engine_reconnect(e);                  /* a fresh connection keeps playing */
  r = play(e, 0, 100);
  assert(r.video > 10);
  dts_engine_destroy(e);
  dts_engine_set_ahead_mib(0);
  dts_engine_set_conversion_stream(0);
  /* Interrupted: the session closes while the reader works. */
  e = dts_engine_create();
  dts_engine_external_audio(e, cut_dub, HEADERS, "pl", 0);
  assert(dts_engine_open(e, cut_main, HEADERS, -1, 0, 0) == 0);
  assert(dts_engine_external_match_arm(e) == 0);
  r = play(e, 0, 0);
  dts_engine_destroy(e);
  /* A PAL version (25 fps, sped up 25/23.976): the frame rates and the
   * lengths agree, so it plays at that speed from the first open (owner's
   * log 14/11) and the match finds the same speed; played again at the
   * speed the match gave (decoded, stretched, re-encoded). */
  e = dts_engine_create();
  dts_engine_external_audio(e, pal_dub, HEADERS, "pl", 0);
  assert(dts_engine_open(e, cut_main, HEADERS, -1, 0, 0) == 0);
  assert(!strcmp(dts_engine_info(e)->audio_codec, "aac"));
  { int cut, sp, ppm; dts_engine_external_mismatch(e, &cut, &sp, &ppm);
    fprintf(stderr, "PAL version opened: cut %d s, speed warning %d, %d ppm\n", cut, sp, ppm);
    assert(abs(cut) <= 2 && !sp && abs(ppm - 42708) < 30); }
  { DtsMatchInfo mi; int res = 0;
    assert(dts_engine_external_match_arm(e) == 0);
    r = play(e, 0, 0);
    for (int i = 0; i < 100 && !(res = dts_engine_external_match_poll(e, &mi)); i++) usleep(50000);
    fprintf(stderr, "PAL version: result %d offset %d ms ratio %d ppm how %d score %d\n", res, mi.offset_ms, mi.ratio_ppm, mi.how, mi.score);
    assert(res == 1 && mi.how == 1 && abs(mi.ratio_ppm - 42708) < 30 && abs(mi.offset_ms) < 60);
    dts_engine_destroy(e);
    e = dts_engine_create();
    dts_engine_external_audio(e, pal_dub, HEADERS, "pl", mi.offset_ms);
    dts_engine_external_speed(e, mi.ratio_ppm);
    assert(dts_engine_open(e, cut_main, HEADERS, -1, 0, 0) == 0);
    assert(!strcmp(dts_engine_info(e)->audio_codec, "aac"));
    r = play(e, 0, 0);
    fprintf(stderr, "PAL at speed: first audio %.3f s last %.3f s\n", r.first_audio / 1e9, r.last_audio / 1e9);
    assert(r.first_audio < 150000000LL && r.last_audio > 119000000000LL && r.last_audio < 121000000000LL);
    assert(dts_engine_seek(e, 60.0) == 0);
    r = play(e, 0, 200);
    assert(r.first_audio > 59800000000LL && r.first_audio < 60300000000LL);
    /* and matched again at that speed: the same audio, offset ~0 */
    assert(dts_engine_seek(e, 20.0) == 0);
    assert(dts_engine_external_match_arm(e) == 0);
    r = play(e, 0, 0);
    res = 0;
    for (int i = 0; i < 100 && !(res = dts_engine_external_match_poll(e, &mi)); i++) usleep(50000);
    fprintf(stderr, "PAL at speed, matched again: result %d offset %d ms ratio %d\n", res, mi.offset_ms, mi.ratio_ppm);
    assert(res == 1 && abs(mi.offset_ms) < 60 && abs(mi.ratio_ppm - 42708) < 30); }
  dts_engine_destroy(e);
  checkBytes = 1;
  printf("External audio engine checks passed.\n");
  return 0;
}
