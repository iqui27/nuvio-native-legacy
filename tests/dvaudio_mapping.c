#include <assert.h>
#include <stdio.h>
#include "../src/video_dvaudio.h"
#include "../src/audioinfo.h"
#include "../src/linguas.h"

static int locate(int nt, int nm, int row, NvDvAudioTrack *tv, NvDvAudioTrack *mkv) {
  return nv_dvaudio_lista(nt, nm, row, tv, mkv, ling_casa, audioinfo_codec);
}
int main(void) {
  // Real log: 21 audio tracks, LG exposes all 15 AC3 tracks and omits DTS/TrueHD.
  // Repeated Russian AC3 tracks cannot be located by selected metadata alone.
  NvDvAudioTrack mkv[64], tv[64];
  const int dts[] = {1, 8, 9, 10, 11};
  int nt = 0, expected[64];
  for (int i = 0; i < 21; i++) {
    const char *codec = i == 17 ? "A_TRUEHD" : "A_AC3";
    for (unsigned k = 0; k < sizeof dts / sizeof *dts; k++) if (i == dts[k]) codec = "A_DTS/LOSSLESS";
    mkv[i] = (NvDvAudioTrack){codec, i < 15 ? "rus" : i < 17 ? "ukr" : "eng", 6};
    if (!strcmp(codec, "A_AC3")) {
      tv[nt] = (NvDvAudioTrack){"ac3", i < 15 ? "ru" : i < 17 ? "uk" : "en", 6};
      expected[nt++] = i;
    }
  }
  assert(nt == 15);
  for (int i = 0; i < nt; i++) assert(locate(nt, 21, i, tv, mkv) == expected[i]);
  assert(locate(nt, 21, 14, tv, mkv) == 20); // Beyond the old 16-track limit.
  // A genuinely partial duplicate list is still ambiguous, including its edges.
  assert(locate(1, 21, 0, tv, mkv) == -1);
  assert(locate(2, 21, 1, tv, mkv) == -1);
  // Channel metadata is part of the match; missing fields remain wildcards.
  NvDvAudioTrack file[] = {{"A_EAC3", "eng", 8}, {"A_AC3", "eng", 6}, {"A_DTS/LOSSLESS", "jpn", 6}};
  NvDvAudioTrack native[] = {{"dtshd_ma", "ja", 6}, {"ac3", "en", 6}};
  assert(locate(2, 3, 0, native, file) == 2); // Reordered list, unique metadata.
  assert(locate(2, 3, 1, native, file) == 1);
  native[0].canais = 2; assert(locate(2, 3, 0, native, file) == -1);
  native[0].canais = 0; assert(locate(2, 3, 0, native, file) == 2);
  NvDvAudioTrack unknown[] = {{"", "en", 0}};
  assert(locate(1, 3, 0, unknown, file) == -1);
  assert(locate(0, 3, 0, native, file) == 0);
  assert(locate(2, 3, -1, native, file) == -1);
  assert(locate(2, 3, 2, native, file) == -1);
  puts("PASS DV full-list audio mapping: 21/15 real-log layout, duplicate ambiguity, filtered/reordered lists and DTS families");
}
