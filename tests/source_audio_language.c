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
  // Uncommon abbreviations in release names must not become audio tabs.
  {
    Stream *rows = NULL;
    const char *payload =
      "{\"streams\":["
      "{\"url\":\"https://example.test/sg\",\"title\":\"Stargate.SG-1.S01E01.ST.1080p.BluRay.x264\"},"
      "{\"url\":\"https://example.test/sgpl\",\"title\":\"Stargate.SG-1.S01E01.PL.1080p.BluRay\"}"
      "]}";
    assert(stream_extrair(payload, "fixture", &rows) == 2);
    AudioExtEntrada candidate;
    stream_audioext_entrada(&rows[0], &candidate);
    assert(candidate.nIdiomas == 0);
    stream_audioext_entrada(&rows[1], &candidate);
    assert(candidate.nIdiomas == 1 && ling_casa(candidate.idiomas[0], "pl"));
    free(rows);
  }
  // The release group BenTheMen is not Bengali audio.
  snprintf(s.arquivo, sizeof s.arquivo, "Dexter.1080p.BEN.THE.MEN");
  snprintf(s.audioIdiomas, sizeof s.audioIdiomas, "ben,eng");
  AudioExtEntrada candidate;
  stream_audioext_entrada(&s, &candidate);
  assert(candidate.nIdiomas == 1 && ling_casa(candidate.idiomas[0], "en"));
  puts("source audio language: ok");
  return 0;
}
