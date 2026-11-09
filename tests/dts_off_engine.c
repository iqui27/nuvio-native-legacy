/* Settings > DTS "Off" on the real engine and the shared rules: no DTS track
 * is selected or switched to; AC-3 keeps the Dolby Vision path; with DTS on
 * nothing changes. */
#define main existing_engine_fixture_main
#include "dts_engine.c"
#undef main
#include "../src/video_dvaudio.h"
static int same(const char *a, const char *b) { return !strcmp(a, b); }
static void rules(void) {
  assert(nv_dts_audio_feeds_off("dts", 0, 0) && nv_dts_audio_feeds_off("dts", 1, 0));
  assert(!nv_dts_audio_feeds_off("dts", 0, 1) && !nv_dts_audio_feeds_off("dts", 1, 1));
  assert(!nv_dts_audio_feeds_off("A_DTS/LOSSLESS", 1, 1));
  assert(nv_dts_audio_feeds_off("ac3", 1, 1) && nv_dts_audio_feeds_off("eac3", 1, 1) && !nv_dts_audio_feeds_off("ac3", 0, 1));
  const char codec[3][16] = { "A_TRUEHD", "A_DTS", "A_AC3" }, idioma[3][8] = { "en", "en", "en" }, rotulo[3][48] = { "", "", "" };
  const char dtsOnly[2][16] = { "A_TRUEHD", "A_DTS" };
  nv_dvaudio_sem_dts = 0;
  assert(nv_dvaudio_nota_codec("A_DTS") == 1 && nv_dvaudio_alimentavel("A_DTS"));
  assert(nv_dvaudio_escolher(3, 0, codec, idioma, rotulo, same) == 2);       /* AC-3 beats DTS anyway */
  assert(nv_dvaudio_escolher(2, 0, dtsOnly, idioma, rotulo, same) == 1);     /* DTS keeps DV when on */
  nv_dvaudio_sem_dts = 1;
  assert(!nv_dvaudio_nota_codec("A_DTS") && !nv_dvaudio_alimentavel("A_DTS"));
  assert(nv_dvaudio_escolher(3, 0, codec, idioma, rotulo, same) == 2);       /* AC-3 keeps DV when off */
  assert(nv_dvaudio_escolher(2, 0, dtsOnly, idioma, rotulo, same) == -1);    /* only DTS: TV player */
  nv_dvaudio_sem_dts = 0;
#ifdef NV_DV_TRUEHD_TEST
  /* Owner 09/11: Off stops the TrueHD conversion of the Dolby Vision path too;
   * the same-language E-AC-3 keeps Dolby Vision, TrueHD alone goes to the TV. */
  assert(nv_dts_audio_feeds_off("truehd", 1, 0) && nv_dts_audio_feeds_off("A_TRUEHD", 1, 0));
  assert(!nv_dts_audio_feeds_off("truehd", 1, 1) && !nv_dts_audio_feeds_off("A_TRUEHD", 1, 1));
  assert(nv_dts_audio_feeds_off("eac3", 1, 1));
  { const char th[2][16] = { "A_TRUEHD", "A_EAC3" }, thOnly[1][16] = { "A_TRUEHD" };
    assert(nv_dvaudio_alimentavel("A_TRUEHD"));
    nv_dvaudio_sem_dts = 1;
    assert(!nv_dvaudio_alimentavel("A_TRUEHD") && nv_dvaudio_alimentavel("A_EAC3"));
    assert(nv_dvaudio_escolher(2, 0, th, idioma, rotulo, same) == 1);         /* E-AC-3 keeps DV */
    assert(nv_dvaudio_escolher(1, 0, thOnly, idioma, rotulo, same) == -1);    /* TrueHD alone: TV player */
    nv_dvaudio_sem_dts = 0; }
  puts("dts off: TrueHD rules passed");
#endif
}
static int frames(DtsEngine *e, int n) {
  DtsFrame f; int r, got = 0;
  for (int i = 0; i < n && (r = dts_engine_next(e, &f)) > 0; i++) got++;
  return got;
}
int main(int argc, char **argv) {
  assert(argc == 2);
  rules();
  DtsEngine *e = dts_engine_create(); assert(e && dts_engine_available());
  /* On (default): plain conversion takes the DTS track (stream 1). */
  dts_engine_set_dts_off(0);
  check(e, dts_engine_open(e, argv[1], HEADERS, -1, 0, 0));
  assert(dts_engine_info(e)->audio_stream == 1 && frames(e, 50) == 50);
  /* Off: plain conversion has no track to convert. */
  dts_engine_set_dts_off(1);
  assert(dts_engine_open(e, argv[1], HEADERS, -1, 0, 0) == -1 && strstr(dts_engine_error(e), "Selected DTS track"));
  /* Off with the Dolby Vision path: AC-3 (stream 2) feeds; DTS is refused. */
  dts_engine_dv(e, 1);
  check(e, dts_engine_open(e, argv[1], HEADERS, -1, 0, 0));
  assert(dts_engine_info(e)->audio_stream == 2 && frames(e, 50) == 50);
  assert(dts_engine_audio(e, 1, 0, 1.0) == -1 && dts_engine_info(e)->audio_stream == 2);
  check(e, dts_engine_seek(e, 2.0)); assert(frames(e, 50) == 50);
  /* Back on: the DV path may take DTS again (first feedable track). */
  dts_engine_set_dts_off(0);
  check(e, dts_engine_open(e, argv[1], HEADERS, -1, 0, 0));
  assert(dts_engine_info(e)->audio_stream == 1);
  check(e, dts_engine_audio(e, 2, 0, 1.0)); assert(dts_engine_info(e)->audio_stream == 2);
  dts_engine_destroy(e);
  puts("PASS DTS Off: rules, no DTS selected or switched to, AC-3 keeps the DV path, DTS on unchanged");
  return 0;
}
