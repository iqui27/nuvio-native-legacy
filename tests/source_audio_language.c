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
  soDub = 0; lista = NULL; n = 0;
  // Real list publication, append and teardown exercise the open-sheet cache.
  Stream cacheRows[2] = {{0}};
  strcpy(cacheRows[0].url, "https://example.test/first");
  strcpy(cacheRows[1].url, "https://example.test/second");
  strcpy(cacheRows[0].audioIdiomas, "pl"); strcpy(cacheRows[1].audioIdiomas, "en");
  stream_definir_lista(cacheRows, 1); aberta = 1; ling_local_audio("pl");
  assert(temAudioPreferido(&lista[0]) && audioFiltroCache.match[0] == 2);
  stream_lista_acrescentar(cacheRows + 1, 1, 0);
  assert(temAudioPreferido(&lista[0]) && !temAudioPreferido(&lista[1]));
  ling_local_audio("en"); assert(!temAudioPreferido(&lista[0]) && temAudioPreferido(&lista[1]));
  stream_definir_lista(cacheRows + 1, 1); assert(temAudioPreferido(&lista[0]));
  stream_definir_lista(NULL, 0); assert(!audioFiltroCache.match && !audioFiltroCache.cap); aberta = 0;
  puts("source audio language: ok");
  return 0;
}
