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
  {
    Stream *v = NULL; char out[4096];
    assert(stream_extrair("{\"streams\":[{\"url\":\"https://example.test/v\",\"title\":\"Movie.1080p.Turkish.mkv\"}]}", "fixture", &v) == 1);
    assert(linhasFaixas(v) == 1);
    assert(audioInferido(v, out, sizeof out) && out[0]);
    assert(!stream_faixas_texto(v, 0, out, sizeof out));
    snprintf(v->audioIdiomas, sizeof v->audioIdiomas, "English");
    assert(!audioInferido(v, out, sizeof out));
    assert(linhasFaixas(v) == 1);
    free(v); v = NULL;
    assert(stream_extrair("{\"streams\":[{\"url\":\"https://example.test/v\",\"title\":\"Movie.1080p.Castilian.mkv\"}]}", "fixture", &v) == 1);
    assert(audioInferido(v, out, sizeof out));
    assert(strstr(out, i18n("Espanhol")));
    free(v); v = NULL;
    assert(stream_extrair("{\"streams\":[{\"url\":\"https://example.test/v\",\"title\":\"Subtitles: Spanish\"}]}", "fixture", &v) == 1);
    assert(!audioInferido(v, out, sizeof out) && linhasFaixas(v) == 1); /* subtitles only */
    free(v);
  }
  // Release labels without declared tracks: languages outside the fixed
  // filter options, subtitle scopes, generic dubs, transport text, codecs.
  static const struct { const char *pref, *campos; int esperado; const char *prov; } c[] = {
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
    {"es","\"title\":\"Movie.2010.1080p.BluRay.Castilian.x264.mkv\"",1},
    {"es","\"description\":\"Subtitles:\\n\xF0\x9F\x87\xAA\xF0\x9F\x87\xB8\"",0},
    {"tr","\"language\":\"en\",\"title\":\"Frozen.2013.1080p.WEB-DL.Turkish.mkv\"",0},
    {"*","\"title\":\"Frozen.2013.1080p.WEB-DL.mkv\"",1},
    // Generic languages: explicit audio fields win; unknown values never match.
    {"tr","\"languages\":[\"tr\"],\"title\":\"Frozen.2013.1080p.WEB-DL.mkv\"",1},
    {"tr","\"parsed\":{\"languages\":[\"Turkish\"]},\"title\":\"Frozen 2013\"",1},
    {"tr","\"languages\":[\"tr\"],\"audioLanguages\":[\"en\"]",0},
    {"en","\"languages\":[\"tr\"],\"audioLanguages\":[\"en\"]",1},
    {"tr","\"streamData\":{\"parsedFile\":{\"languages\":[\"Turkish\"]}},\"audioTracks\":[{\"language\":\"en\"}]",0},
    {"tr","\"languages\":[\"tr\"],\"description\":\"Audio: English\"",1},
    {"en","\"languages\":[\"tr\"],\"description\":\"Audio: English\"",0},
    {"tr","\"languages\":[\"und\"],\"title\":\"Frozen.2013.1080p.WEB-DL.Turkish.x264.mkv\"",0},
    {"tr","\"languages\":[\"Dual Audio\"]",0},
    {"tr","\"languages\":[]",0},
    {"en","\"languages\":[\"tr\"],\"subtitles\":[{\"lang\":\"en\",\"url\":\"https://example.test/s.srt\"}]",0},
    {"tr","\"subtitles\":[{\"lang\":\"tr\",\"url\":\"https://example.test/s.srt\"}],\"title\":\"Frozen 2013\"",0},
    // A bracketed language name before the technical tags.
    {"tr","\"title\":\"Frozen (2013) [Turkish] 1080p WEB-DL x264\"",1},
    {"tr","\"title\":\"Frozen.2013.[Turkish].1080p.WEB-DL.x264.mkv\"",1},
    {"tr","\"behaviorHints\":{\"filename\":\"Frozen.2013.(Turkish).1080p.BluRay.x264.mkv\"}",1},
    {"en","\"title\":\"The.English.Patient.1996.1080p.BluRay.x264.mkv\"",0},
    {"en","\"title\":\"[English Patient] 1996 1080p BluRay x264\"",0},
    {"it","\"title\":\"It.2017.1080p.BluRay.x264.mkv\"",0},
    {"it","\"title\":\"[It] 2017 1080p BluRay x264\"",0},
    {"tr","\"title\":\"[Tron] 2010 1080p BluRay x264\"",0},
    {"tr","\"title\":\"Frozen 2013 [Dubbed] 1080p WEB-DL x264\"",0},
    {"tr","\"title\":\"Frozen 2013 [Dual] 1080p WEB-DL x264\"",0},
    {"tr","\"title\":\"Frozen 2013 [Multi] 1080p WEB-DL x264\"",0},
    // Subtitle sections sharing a line with audio or the release name.
    {"tr","\"description\":\"Audio: Turkish | Subs: English\"",1},
    {"en","\"description\":\"Audio: Turkish | Subs: English\"",0},
    {"tr","\"title\":\"Frozen.2013.1080p.WEB-DL.Turkish.x264 | Subs: English\"",1},
    {"en","\"title\":\"Frozen.2013.1080p.WEB-DL.Turkish.x264 | Subs: English\"",0},
    {"tr","\"title\":\"Frozen.2013.1080p.WEB-DL.x264 | Subs: Turkish\"",0},
    {"tr","\"title\":\"Frozen.2013.1080p.WEB-DL.x264\\nSubtitles:\\nTurkish\"",0},
    // Provider name never counts; title survives next to a description.
    {"tr","\"title\":\"Frozen 2013\"",0,"Turkish.1080p.WEB-DL.x264"},
    {"tr","\"description\":\"Frozen.2013.1080p.WEB-DL.mkv\"",0,"\xF0\x9F\x87\xB9\xF0\x9F\x87\xB7 IPTV"},
    {"es","\"description\":\"Frozen.2013.1080p.WEB-DL.mkv\"",0,"\xF0\x9F\x87\xAA\xF0\x9F\x87\xB8 IPTV"},
    {"tr","\"name\":\"\xF0\x9F\x87\xB9\xF0\x9F\x87\xB7 1080p\",\"description\":\"Frozen.2013.1080p.WEB-DL.mkv\"",1,"\xF0\x9F\x87\xAA\xF0\x9F\x87\xB8 IPTV"},
    {"tr","\"description\":\"Frozen.2013.1080p.WEB-DL.Turkish.mkv\"",1,"\xF0\x9F\x87\xAA\xF0\x9F\x87\xB8 IPTV"},
    {"tr","\"audioLanguages\":[\"en\"],\"description\":\"Frozen.2013.1080p.WEB-DL.Turkish.mkv\"",0,"\xF0\x9F\x87\xB9\xF0\x9F\x87\xB7 IPTV"},
    {"en","\"audioLanguages\":[\"en\"],\"description\":\"Frozen.2013.1080p.WEB-DL.Turkish.mkv\"",1,"\xF0\x9F\x87\xB9\xF0\x9F\x87\xB7 IPTV"},
    {"tr","\"title\":\"Frozen.2013.1080p.WEB-DL.Turkish.x264.mkv\",\"description\":\"2.1 GB\"",1},
    {"tr","\"name\":\"Addon 1080p\",\"title\":\"Frozen.2013.1080p.WEB-DL.Turkish.x264.mkv\",\"description\":\"Frozen 2013\\n2.1 GB\"",1},
  };
  int falhas = 0;
  for (unsigned i = 0; i < sizeof c / sizeof *c; i++) {
    char json[1024]; Stream *v = NULL;
    snprintf(json, sizeof json, "{\"streams\":[{\"url\":\"https://example.test/v\",%s}]}", c[i].campos);
    assert(stream_extrair(json, c[i].prov ? c[i].prov : "fixture", &v) == 1);
    ling_local_audio(c[i].pref); lista = v; n = 1; soDub = 1;
    int r = passaChips(0);
    printf("case %02u %s\n", i, r == c[i].esperado ? "ok" : "FAIL");
    falhas += r != c[i].esperado;
    free(v);
  }
  assert(!falhas);
  // Original audio and the saved-filter mask follow the same evidence.
  {
    Stream *v = NULL;
    assert(stream_extrair("{\"streams\":[{\"url\":\"https://example.test/v\",\"languages\":[\"en\"],\"audioLanguages\":[\"tr\"]}]}", "fixture", &v) == 1);
    assert(!(v->filtro.valores[FONTEFILTRO_LINGUAS] & fontefiltro_bit(FONTEFILTRO_LINGUAS, 0)) && "explicit audio wins in the filter mask");
    ling_local_audio("~"); ling_definir_original("tr"); lista = v; n = 1; soDub = 1;
    assert(passaChips(0) && "Original follows declared audio");
    ling_definir_original("en");
    assert(!passaChips(0));
    free(v);
    puts("case mask/original ok");
  }
  puts("source audio language: ok");
  return 0;
}
