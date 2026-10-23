// THE REMEMBERED SOURCE KEEPS THE RELEASE NAME (the Scandal bug).
//
// Two works can return sources with the SAME provider, the SAME bingeGroup,
// the SAME label and the SAME height: watching "The Scandal" got "Scandal"
// sources, and every manual re-pick was dropped as a duplicate because every
// field the memory compared was equal. Only the release file name tells the
// two apart, so the memory has to keep it and weigh it.
//
// What this locks:
//   - the file name survives the S1E1 -> S1E2 change (the marker is cut away);
//   - the pick follows the file name regardless of list order;
//   - a re-pick that differs ONLY in the file name is written, not dropped;
//   - names without a year ("The.Scandal.S01E06...") still separate the works;
//   - no name, or a name with no episode marker, behaves exactly as before;
//   - the key itself: marker cut, dot collapsing, "1280x720" never a marker,
//     "s01e065" never a marker, two markers never cut.
//
// Same harness as tests/fontepref_manual.c (NUVIO_DADOS temporary, refuses to
// run without it).
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../src/fontepref.c"

#include "dados.h"

static int failures;
#define CONFERE(cond, ...) do { \
    if (cond) { printf("ok    "); } else { printf("FAIL  "); failures++; } \
    printf(__VA_ARGS__); printf("\n"); } while (0)

static void mksource(Stream *s, const char *provider, const char *label, const char *file,
                   const char *url, int height, const char *binge) {
  memset(s, 0, sizeof *s);
  snprintf(s->provedor, sizeof s->provedor, "%s", provider);
  snprintf(s->rotulo, sizeof s->rotulo, "%s", label);
  snprintf(s->arquivo, sizeof s->arquivo, "%s", file);
  snprintf(s->url, sizeof s->url, "%s", url);
  s->altura = height;
  snprintf(s->bingeGroup, sizeof s->bingeGroup, "%s", binge);
}

// ONE EPISODE'S ANSWER from AIOStreams: [0] "Scandal" (the other series),
// [1] "The Scandal". Everything the memory used to compare is equal: same
// provider, same bingeGroup, same label, same height. `inverted` swaps them,
// because the addon's list order is not stable between episodes.
static int episodeList(Stream *v, int ep, int inverted, int noYear) {
  char a0[120], a1[120], u0[120], u1[120];
  if (noYear) {
    snprintf(a0, sizeof a0, "Scandal.S01E%02d.1080p.mkv", ep);
    snprintf(a1, sizeof a1, "The.Scandal.S01E%02d.1080p.mkv", ep);
  } else {
    snprintf(a0, sizeof a0, "Scandal.2026.S01E%02d.1080p.WEB-DL.x264.mkv", ep);
    snprintf(a1, sizeof a1, "The.Scandal.2026.S01E%02d.1080p.WEB-DL.x264.mkv", ep);
  }
  snprintf(u0, sizeof u0, "https://example.invalid/scandal/%d.mkv", ep);
  snprintf(u1, sizeof u1, "https://example.invalid/the-scandal/%d.mkv", ep);
  mksource(&v[0], "AIOStreams", "\xe2\x9a\xa1" "Full HD 1080p", a0, u0, 1080,
         "host.midnightignite.me|1080p|WEB-DL");
  mksource(&v[1], "AIOStreams", "\xe2\x9a\xa1" "Full HD 1080p", a1, u1, 1080,
         "host.midnightignite.me|1080p|WEB-DL");
  if (inverted) { Stream t = v[0]; v[0] = v[1]; v[1] = t; }
  return 2;
}

// Names with no episode marker at all: the key cannot work, the old rule must.
static int noMarkerList(Stream *v, int ep) {
  char a0[120], a1[120], u0[120], u1[120];
  snprintf(a0, sizeof a0, "scandal.1080p.part%d.mkv", ep);
  snprintf(a1, sizeof a1, "the.scandal.1080p.part%d.mkv", ep);
  snprintf(u0, sizeof u0, "https://example.invalid/scandal/%d.mkv", ep);
  snprintf(u1, sizeof u1, "https://example.invalid/the-scandal/%d.mkv", ep);
  mksource(&v[0], "AIOStreams", "\xe2\x9a\xa1" "Full HD 1080p", a0, u0, 1080,
         "host.midnightignite.me|1080p|WEB-DL");
  mksource(&v[1], "AIOStreams", "\xe2\x9a\xa1" "Full HD 1080p", a1, u1, 1080,
         "host.midnightignite.me|1080p|WEB-DL");
  return 2;
}

static int isThe(const Stream *s) {
  return strstr(s->arquivo, "The") == s->arquivo;
}

int main(void) {
  const char *dir = getenv("NUVIO_DADOS");
  Stream v[4];
  char key[FONTEPREF_ARQUIVO];
  int n, k;

  if (!dir || !dir[0]) { printf("fontepref_file: NUVIO_DADOS missing; refusing\n"); return 2; }
  dados_iniciar(dir);
  if (strcmp(dados_dir(), dir)) { printf("fontepref_file: dados_dir mismatch; refusing\n"); return 2; }

  // --- the key itself ------------------------------------------------------
  fileKey("The.Scandal.2026.S01E06.1080p.WEB-DL.x264.mkv", key, sizeof key);
  CONFERE(!strcmp(key, "the.scandal.2026"), "key: marker cut (%s)", key);
  fileKey("the scandal s2x03 720p", key, sizeof key);
  CONFERE(!strcmp(key, "the.scandal"), "key: 2x03 is a marker too (%s)", key);
  fileKey("1280x720 clip.mkv", key, sizeof key);
  CONFERE(!strcmp(key, "1280x720.clip.mkv"), "key: 1280x720 is not a marker (%s)", key);
  fileKey("show s01e065 mkv", key, sizeof key);
  CONFERE(!strcmp(key, "show.s01e065.mkv"), "key: s01e065 does not cut (%s)", key);
  fileKey("show.s01e06.s01e06", key, sizeof key);
  CONFERE(!strcmp(key, "show.s01e06.s01e06"), "key: two markers do not cut (%s)", key);
  // A path in `filename` repeats the name in the directory. Only the basename
  // is the name of the release (measured: 90 such rows on a 1656-name corpus).
  fileKey("Show.Name.S01E02.1080p/Show.Name.S01E02.1080p.mkv", key, sizeof key);
  CONFERE(!strcmp(key, "show.name"), "key: a path uses the basename only (%s)", key);
  fileKey("Better.Late.Than.Single.S02E02.WEB/Better.Late.Than.Single.S02E02.WEB.mkv", key, sizeof key);
  CONFERE(!strcmp(key, "better.late.than.single"), "key: path, repeated name, one cut (%s)", key);
  // A NAME THAT WOULD NOT FIT IS DROPPED, NOT CUT: a truncated key would let
  // two different releases of two different works agree on the first bytes.
  { char tiny[12];
    fileKey("Scandal.2026.S01E06.1080p.WEB-DL.x264.mkv", tiny, sizeof tiny);
    CONFERE(tiny[0] == 0, "key: too long for the buffer -> empty, never truncated (%s)", tiny);
    fileKey("a.mkv", tiny, sizeof tiny);
    CONFERE(!strcmp(tiny, "a.mkv"), "key: a short name still fits the small buffer (%s)", tiny); }
  // NON-LATIN NAMES ARE WORDS, NOT PUNCTUATION. UTF-8 bytes are all >= 0x80 and
  // isalnum() is false for them, so dropping them made a Korean-only name key
  // to empty (two different series agreeing) and a spec-only name reduce to
  // "1080p.mkv" (a collision). Both are the bug this key exists to prevent.
  fileKey("\xeb\xa9\x8b\xec\xa7\x84 " "\xec\x8b\xa0\xec\x84\xb8\xea\xb3\x84.My.Royal.Nemesis.S01E01.1080p.mkv", key, sizeof key);
  CONFERE(!strcmp(key, "\xeb\xa9\x8b\xec\xa7\x84.\xec\x8b\xa0\xec\x84\xb8\xea\xb3\x84.my.royal.nemesis"),
          "key: both titles kept, in either order (%s)", key);
  fileKey("My.Royal.Nemesis.\xeb\xa9\x8b\xec\xa7\x84 " "\xec\x8b\xa0\xec\x84\xb8\xea\xb3\x84.S01E01.1080p.mkv", key, sizeof key);
  CONFERE(!strcmp(key, "my.royal.nemesis.\xeb\xa9\x8b\xec\xa7\x84.\xec\x8b\xa0\xec\x84\xb8\xea\xb3\x84"),
          "key: the same two titles in the other order give a different string, as they must (%s)", key);
  fileKey("\xeb\xa9\x8b\xec\xa7\x84 " "\xec\x8b\xa0\xec\x84\xb8\xea\xb3\x84.S01E01.1080p.mkv", key, sizeof key);
  CONFERE(key[0] != 0, "key: a Korean-only name is not empty (%s)", key);
  { char other[FONTEPREF_ARQUIVO];
    fileKey("\xeb\x8b\xa4\xeb\xa5\xb8 " "\xec\x84\xb8\xea\xb3\x84.S01E01.1080p.mkv", other, sizeof other);
    CONFERE(strcmp(key, other) != 0, "key: two Korean-only names do not collide (%s vs %s)", key, other); }
  fileKey("\xeb\xa9\x8b\xec\xa7\x84 " "\xec\x8b\xa0\xec\x84\xb8\xea\xb3\x84.1080p.mkv", key, sizeof key);
  CONFERE(strcmp(key, "1080p.mkv") != 0, "key: no marker, the name is not reduced to its spec (%s)", key);

  // --- the defect: collision between The Scandal and Scandal ----------------
  fontepref_definir_perfil(1);
  n = episodeList(v, 1, 0, 0);
  stream_definir_lista(v, n);
  CONFERE(fontepref_guardar("tt36347588:1:1", stream_item(1)) == 1,
          "S1E1: the person's pick (The Scandal) is stored");

  // S1E2, Scandal first in the list (the default: the wrong one comes first).
  n = episodeList(v, 2, 0, 0);
  stream_definir_lista(v, n);
  k = fontepref_escolher("tt36347588:1:2");
  CONFERE(k == 1 && isThe(stream_item(k)),
          "S1E2: follows The Scandal with Scandal first (%d)", k);

  // Plain base ID, inverted order: answer does not depend on list position.
  n = episodeList(v, 8, 1, 0);
  stream_definir_lista(v, n);
  k = fontepref_escolher("tt36347588");
  CONFERE(k == 0 && isThe(stream_item(k)),
          "base id, list inverted: same answer (%d)", k);

  // Another series inherits nothing.
  n = episodeList(v, 1, 0, 0);
  stream_definir_lista(v, n);
  CONFERE(fontepref_escolher("tt9999999:1:1") == -1, "other series: nothing remembered");

  // --- a re-pick differing only in the file name is written -----------------
  n = episodeList(v, 3, 0, 0);
  stream_definir_lista(v, n);
  CONFERE(fontepref_guardar("tt36347588:1:3", stream_item(0)) == 1,
          "a re-pick differing only in the file name is written (before: silence)");
  n = episodeList(v, 4, 0, 0);
  stream_definir_lista(v, n);
  k = fontepref_escolher("tt36347588:1:4");
  CONFERE(k == 0 && !isThe(stream_item(k)),
          "and the memory follows the LAST pick (%d)", k);

  // --- no year in name -----------------------------------------------------
  fontepref_definir_perfil(2);
  n = episodeList(v, 1, 0, 1);
  stream_definir_lista(v, n);
  CONFERE(fontepref_guardar("tt2000001:1:1", stream_item(1)) == 1, "no year: stored");
  n = episodeList(v, 2, 1, 1);
  stream_definir_lista(v, n);
  k = fontepref_escolher("tt2000001:1:2");
  CONFERE(k == 0 && isThe(stream_item(k)),
          "no year: the name still separates the works (%d)", k);

  // --- no marker in name: falls back to old behavior without degrading ------
  n = noMarkerList(v, 6);
  stream_definir_lista(v, n);
  CONFERE(fontepref_guardar("tt2000002:1:1", stream_item(1)) == 1, "no marker: stored");
  n = noMarkerList(v, 7);
  stream_definir_lista(v, n);
  k = fontepref_escolher("tt2000002:1:2");
  CONFERE(k == 0, "no marker: tied as before, first in the list wins (%d)", k);

  // --- no source has a file name: exactly as before -------------------------
  fontepref_definir_perfil(3);
  n = noMarkerList(v, 6);
  v[0].arquivo[0] = v[1].arquivo[0] = 0;
  stream_definir_lista(v, n);
  CONFERE(fontepref_guardar("tt2000003:1:1", stream_item(1)) == 1, "no name: stored");
  n = noMarkerList(v, 7);
  v[0].arquivo[0] = v[1].arquivo[0] = 0;
  stream_definir_lista(v, n);
  k = fontepref_escolher("tt2000003:1:2");
  CONFERE(k == 0, "no name: old behaviour (%d)", k);

  printf(failures ? "fontepref_file: %d failure(s)\n" : "fontepref_file: all ok\n", failures);
  return failures ? 1 : 0;
}
