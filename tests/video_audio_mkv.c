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
  puts("PASS video_audio_mkv: omitted DTS (same language too) recovered once, ambiguous rows left alone");
  return 0;
}
