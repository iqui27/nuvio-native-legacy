/* Omitted MKV DTS tracks recovered into the TV list (src/video_audio_mkv.h). */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "audioinfo.h"
#include "linguas.h"
#include "video_audio_mkv.h"
static VideoFaixa tv(int numero, const char *codec, const char *idioma, int canais) {
  VideoFaixa f; memset(&f, 0, sizeof f);
  f.numero = numero; f.stream_index = f.stream_id = -1; f.canais = canais;
  snprintf(f.codec, sizeof f.codec, "%s", codec); snprintf(f.idioma, sizeof f.idioma, "%s", idioma);
  return f;
}
int main(void) {
  const char codec[5][16] = { "A_AC3", "A_DTS/LOSSLESS", "A_DTS", "A_DTS", "A_TRUEHD" };
  const char idioma[5][8] = { "eng", "eng", "eng", "pol", "eng" };
  const char nome[5][48] = { "", "DTS-HD MA 7.1", "Commentary", "Lektor", "" };
  const int canais[5] = { 6, 8, 6, 6, 8 };
  VideoFaixa l[NV_FAIXA_MAX];
  /* TV without DTS: lists AC-3 and TrueHD only. All three DTS come back,
   * two of them in the same language, each with its own file ordinal. */
  l[0] = tv(0, "ac3", "en", 6); l[1] = tv(1, "truehd", "en", 8);
  int n = nv_audio_mkv_omitidas(l, 2, NV_FAIXA_MAX, 5, codec, idioma, canais, nome, ling_casa, audioinfo_codec);
  assert(n == 5 && l[0].numero == 0 && l[1].numero == 1);
  assert(l[2].numero == -1 && l[2].stream_index == -1 && l[2].ordinalMkv == 1 && l[2].canais == 8 && !strcmp(l[2].codec, "dts"));
  assert(l[3].ordinalMkv == 2 && !strcmp(l[3].nome, "Commentary") && !strcmp(l[3].idioma, "eng"));
  assert(l[4].ordinalMkv == 3 && !strcmp(l[4].idioma, "pol"));
  /* Idempotent: a second pass rebuilds, never duplicates. */
  assert(nv_audio_mkv_omitidas(l, n, NV_FAIXA_MAX, 5, codec, idioma, canais, nome, ling_casa, audioinfo_codec) == 5);
  /* TV lists one English DTS: it could be either English DTS, so neither is
   * added (no guessing); Polish DTS still is. */
  l[2] = tv(2, "dts", "en", 0);
  n = nv_audio_mkv_omitidas(l, 3, NV_FAIXA_MAX, 5, codec, idioma, canais, nome, ling_casa, audioinfo_codec);
  assert(n == 4 && l[2].numero == 2 && l[3].ordinalMkv == 3);
  /* Capacity is respected. */
  l[0] = tv(0, "ac3", "en", 6);
  assert(nv_audio_mkv_omitidas(l, 1, 2, 5, codec, idioma, canais, nome, ling_casa, audioinfo_codec) == 2);
  /* DTS-HD told apart by the file's frames (tester 15/11): the codec name
   * for the menu line comes from dts_kind, not from A_DTS. */
  { NvAudioMkvTrack m = {2, 6, "A_DTS", "en", "x", 3};
    assert(!strcmp(nv_audio_mkv_rotulo_codec(&m), "dts-hd ma"));
    m.dts_kind = 2; assert(!strcmp(nv_audio_mkv_rotulo_codec(&m), "dts-hd"));
    m.dts_kind = 1; assert(!strcmp(nv_audio_mkv_rotulo_codec(&m), "A_DTS"));
    assert(nv_audio_mkv_dts("A_DTS") && nv_audio_mkv_dts("A_DTS/LOSSLESS") &&
           !nv_audio_mkv_dts("A_AC3") && !nv_audio_mkv_dts("ac3")); }
  /* Two A_DTS of the same language and channels, one MA and one core: the TV
   * listed both rows and each line gets the kind of the file track it
   * embeds to. Labels only: codec and numero stay. */
  { NvAudioMkvTrack two[2] = { {2, 6, "A_DTS", "en", "MA", 3}, {3, 6, "A_DTS", "en", "core", 1} };
    VideoFaixa w[NV_FAIXA_MAX];
    for (int i = 0; i < 2; i++) {
      w[i] = tv(i, "dts", "en", 6);
      snprintf(w[i].rotulo, sizeof w[i].rotulo, "English  \xc2\xb7  DTS 5.1");
    }
    assert(nv_audio_mkv_rotular(w, 2, two, 2) == 1);
    assert(!strcmp(w[0].rotulo, "English  \xc2\xb7  DTS-HD MA 5.1"));
    assert(!strcmp(w[1].rotulo, "English  \xc2\xb7  DTS 5.1"));
    assert(!strcmp(w[0].codec, "dts") && w[0].numero == 0);
    assert(nv_audio_mkv_rotular(w, 2, two, 2) == 0);   /* idempotent */
    /* The same pair without explicit names is still told apart by frames. */
    { NvAudioMkvTrack nn[2] = { {2, 6, "A_DTS", "", "x", 2}, {3, 6, "A_DTS", "", "x", 1} };
      for (int i = 0; i < 2; i++) {
        w[i] = tv(i, "dts", "en", 6);
        snprintf(w[i].rotulo, sizeof w[i].rotulo, "English  \xc2\xb7  DTS 5.1");
      }
      assert(nv_audio_mkv_rotular(w, 2, nn, 2) == 1);
      assert(!strcmp(w[0].rotulo, "English  \xc2\xb7  DTS-HD 5.1"));
      assert(!strcmp(w[1].rotulo, "English  \xc2\xb7  DTS 5.1")); } }
  /* The TV left one DTS row out: which of two identical file tracks it is
   * can't be told, so its label stays plain (never guess). */
  { NvAudioMkvTrack amb[2] = { {2, 6, "A_DTS", "en", "x", 3}, {3, 6, "A_DTS", "en", "x", 1} };
    l[0] = tv(0, "dts", "en", 6);
    snprintf(l[0].rotulo, sizeof l[0].rotulo, "English  \xc2\xb7  DTS 5.1");
    assert(nv_audio_mkv_rotular(l, 1, amb, 2) == 0);
    assert(!strcmp(l[0].rotulo, "English  \xc2\xb7  DTS 5.1")); }
  /* A recovered row (numero<0) is not relabelled by rotular: the caller
   * labels it itself with the kind-aware codec name. */
  { const char hc[1][16] = {"A_DTS"}, hi[1][8] = {"en"}, hn[1][48] = {"Original"};
    const int hch[1] = {6};
    NvAudioMkvTrack hd[1] = { {2, 6, "A_DTS", "en", "Original", 2} };
    l[0] = tv(0, "ac3", "en", 2);
    int n2 = nv_audio_mkv_omitidas(l, 1, NV_FAIXA_MAX, 1, hc, hi, hch, hn, ling_casa, audioinfo_codec);
    assert(n2 == 2 && l[1].numero == -1);
    snprintf(l[1].rotulo, sizeof l[1].rotulo, "English  \xc2\xb7  DTS 5.1");
    assert(nv_audio_mkv_rotular(l, 2, hd, 1) == 0);
    assert(!strcmp(l[1].rotulo, "English  \xc2\xb7  DTS 5.1")); }
  puts("PASS video_audio_mkv: omitted DTS (same language too) recovered once, ambiguous rows left alone, DTS-HD/MA labels");
  return 0;
}
