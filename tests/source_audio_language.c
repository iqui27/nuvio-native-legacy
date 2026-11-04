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
  puts("source audio language: ok");
  return 0;
}
