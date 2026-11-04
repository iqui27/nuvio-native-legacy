#include "../src/streams.c"
#include <assert.h>

int main(void) {
  Stream s = {0};
  ling_local_audio("pl");
  snprintf(s.audioIdiomas, sizeof s.audioIdiomas, "Polish, eng");
  lista = &s; n = 1; soDub = 1;
  assert(passaChips(0) && "Polish preference must accept Polish audio");
  snprintf(s.audioIdiomas, sizeof s.audioIdiomas, "Portuguese");
  snprintf(s.legendaIdiomas, sizeof s.legendaIdiomas, "Polish");
  assert(!passaChips(0) && "Polish subtitles must not count as Polish audio");
  snprintf(s.audioIdiomas, sizeof s.audioIdiomas, "pol");
  assert(passaChips(0));
  snprintf(s.audioIdiomas, sizeof s.audioIdiomas, "pl-PL");
  assert(passaChips(0));
  ling_local_audio("en");
  assert(!passaChips(0));
  snprintf(s.audioIdiomas, sizeof s.audioIdiomas, "English");
  assert(passaChips(0));
  ling_local_audio("pt");
  assert(!passaChips(0));
  snprintf(s.audioIdiomas, sizeof s.audioIdiomas, "pt-BR");
  assert(passaChips(0));
  s.audioIdiomas[0] = s.legendaIdiomas[0] = 0;
  snprintf(s.rotulo, sizeof s.rotulo, "Dublado");
  assert(!passaChips(0) && "Unspecified dubbing is not declared audio");
  ling_local_audio("*");
  snprintf(s.audioIdiomas, sizeof s.audioIdiomas, "Polish");
  assert(passaChips(0) && "All audio languages leaves the filter open");
  ling_local_audio("~"); ling_definir_original("pl");
  snprintf(s.audioIdiomas, sizeof s.audioIdiomas, "Polish");
  assert(passaChips(0));
  const char *cases[] = {
    "\"title\":\"Movie.1080p.es.mkv\"",
    "\"title\":\"Movie.1080p.spa.mkv\"",
    "\"title\":\"Movie.1080p.es-ES.mkv\"",
    "\"title\":\"Movie.1080p.es.mkv\",\"audioLanguages\":[\"en\"]",
    "\"title\":\"Subtitles: Spanish\"",
    "\"subtitleLanguages\":[\"es\"]"
  };
  for (int i = 0; i < 6; i++) {
    char json[512]; Stream *v = NULL;
    snprintf(json, sizeof json, "{\"streams\":[{\"url\":\"https://example.test/v\",%s}]}", cases[i]);
    assert(stream_extrair(json, "fixture", &v) == 1);
    ling_local_audio("es"); lista = v; n = 1; soDub = 1;
    assert(passaChips(0) == (i < 3));
    free(v);
  }
  // Release labels without declared tracks: languages outside the fixed
  // filter options, subtitle scopes, generic dubs, transport text, codecs.
  static const struct { const char *pref, *campos; int esperado; } c[] = {
    {"tr","\"title\":\"Frozen.2013.1080p.WEB-DL.Turkish.x264.mkv\"",1},
    {"tr","\"title\":\"Frozen.2013.1080p.WEB-DL.TR.x264.mkv\"",1},
    {"tr","\"title\":\"Frozen.2013.1080p.WEB-DL.tur.x264.mkv\"",1},
    {"tr","\"title\":\"Frozen 2013\\n\xF0\x9F\x87\xB9\xF0\x9F\x87\xB7 1080p\"",1},
    {"tr","\"title\":\"Frozen 2013\\n1080p\xF0\x9F\x87\xB9\xF0\x9F\x87\xB7\"",1},
    {"tr","\"description\":\"Frozen.2013.1080p.WEB-DL.Turkish.x264.mkv\"",1},
    {"tr","\"behaviorHints\":{\"filename\":\"Frozen.2013.1080p.WEB-DL.Turkish.x264.mkv\"}",1},
    {"tr","\"title\":\"Frozen.2013.1080p.WEB-DL.Turkish.mkv\",\"audioLanguages\":[\"en\"]",0},
    {"en","\"title\":\"Frozen.2013.1080p.WEB-DL.Turkish.mkv\",\"audioLanguages\":[\"en\"]",1},
    {"tr","\"description\":\"Audio: English | Subs: \xF0\x9F\x87\xB9\xF0\x9F\x87\xB7\"",0},
    {"tr","\"description\":\"1080p WEB-DL | Subs: 1080p\xF0\x9F\x87\xB9\xF0\x9F\x87\xB7\"",0},
    {"tr","\"description\":\"Subtitles:\\n\xF0\x9F\x87\xB9\xF0\x9F\x87\xB7\"",0},
    {"tr","\"description\":\"Subtitles:\\n1080p\xF0\x9F\x87\xB9\xF0\x9F\x87\xB7\"",0},
    {"tr","\"description\":\"Subtitles: Turkish\\nFrozen.2013.1080p.WEB-DL.x264.mkv\"",0},
    {"tr","\"subtitleLanguages\":[\"tr\"],\"title\":\"Frozen.2013.1080p.WEB-DL.mkv\"",0},
    {"tr","\"title\":\"Frozen.2013.1080p.WEB-DL.Dual.Audio.mkv\"",0},
    {"tr","\"title\":\"Frozen.2013.1080p.WEB-DL.MULTI.mkv\"",0},
    {"tr","\"title\":\"Frozen.2013.1080p.WEB-DL.Dubbed.mkv\"",0},
    {"tr","\"name\":\"Turkish Addon\",\"title\":\"Frozen.2013.1080p.WEB-DL.mkv\"",0},
    {"tr","\"behaviorHints\":{\"proxyHeaders\":{\"request\":{\"Accept-Language\":\"tr\"}}},\"title\":\"Frozen.2013.1080p.WEB-DL.mkv\"",0},
    {"tr","\"title\":\"https://example.test/tr/Frozen.Turkish.1080p.WEB-DL.mkv\"",0},
    {"tr","\"title\":\"Turks.and.Caicos.2014.1080p.WEB-DL.x264.mkv\"",0},
    {"tr","\"title\":\"Tron.Legacy.2010.1080p.BluRay.x264.mkv\"",0},
    {"es","\"title\":\"Movie.2010.1080p.BluRay.DTS-ES.x264.mkv\"",0},
    {"es","\"title\":\"Movie.2010.1080p.BluRay.es.x264.mkv\"",1},
    {"es","\"description\":\"Subtitles:\\n\xF0\x9F\x87\xAA\xF0\x9F\x87\xB8\"",0},
    {"tr","\"language\":\"en\",\"title\":\"Frozen.2013.1080p.WEB-DL.Turkish.mkv\"",0},
    {"*","\"title\":\"Frozen.2013.1080p.WEB-DL.mkv\"",1},
  };
  for (unsigned i = 0; i < sizeof c / sizeof *c; i++) {
    char json[1024]; Stream *v = NULL;
    snprintf(json, sizeof json, "{\"streams\":[{\"url\":\"https://example.test/v\",%s}]}", c[i].campos);
    assert(stream_extrair(json, "fixture", &v) == 1);
    ling_local_audio(c[i].pref); lista = v; n = 1; soDub = 1;
    int r = passaChips(0);
    printf("case %02u %s\n", i, r == c[i].esperado ? "ok" : "FAIL");
    assert(r == c[i].esperado);
    free(v);
  }
  puts("source audio language: ok");
  return 0;
}
