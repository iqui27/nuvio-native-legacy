// Segunda busca real com extras: preserva lista, indices e identidade escolhida.
#define main addons_legendas_main
#include "addons_legendas.c"
#undef main
void teste_merge_escolher(const Legenda *l);
int teste_merge_selecionada(const Legenda *l, int indice);

static void extras(const char *resposta) {
  responses[0] = resposta;
  addons_legendas_fonte("tt123", "movie", "Movie.mkv", 1000000, "0123456789abcdef", "", 0);
  pthread_join(fioLeg, NULL);
  fioLegCriado = 0;
  legExt.arquivo[0] = 0; // proxima chamada usa o mesmo filme com outro arquivo
}
int main(void) {
  char boas[3000] = "{\"subtitles\":[";
  for (int i = 0; i < 8; i++) {
    char item[240];
    snprintf(item, sizeof item, "%s{\"id\":\"sub%d\",\"lang\":\"eng\",\"url\":\"https://fixture.invalid/%d.srt\"}", i ? "," : "", i, i);
    strcat(boas, item);
  }
  strcat(boas, "]}");
  ling_local_legenda("*"); responses[0] = boas;
  memset(&legExt, 0, sizeof legExt); run(1, "tt123", "movie");
  assert(nLegs == 8);
  int escolhida = 4;
  Legenda selecionada = legs[escolhida];
  teste_merge_escolher(&selecionada);
  assert(teste_merge_selecionada(&selecionada, escolhida));
  extras("{\"subtitles\":[{\"id\":\"error.[❌] OpenSubtitles V3+\",\"lang\":\"[❌] OpenSubtitles V3+ - 500 - Internal Server Error\",\"url\":\"https://fixture.invalid/error.srt\"}]}");
  assert(nLegs == 8 && !memcmp(&legs[escolhida], &selecionada, sizeof selecionada));
  extras("{\"subtitles\":[{\"id\":\"sub0\",\"lang\":\"eng\",\"url\":\"https://fixture.invalid/0.srt\"},{\"id\":\"new1\",\"lang\":\"eng\",\"url\":\"https://fixture.invalid/8.srt\"},{\"id\":\"new2\",\"lang\":\"eng\",\"url\":\"https://fixture.invalid/9.srt\"}]}");
  assert(nLegs == 10 && !memcmp(&legs[escolhida], &selecionada, sizeof selecionada));
  // Mesmo id+addon com URL diferente e mesma URL com id diferente nao duplicam.
  extras("{\"subtitles\":[{\"id\":\"sub0\",\"lang\":\"eng\",\"url\":\"https://fixture.invalid/changed.srt\"},{\"id\":\"other\",\"lang\":\"eng\",\"url\":\"https://fixture.invalid/9.srt\"}]}");
  assert(nLegs == 10 && !memcmp(&legs[escolhida], &selecionada, sizeof selecionada));
  extras("{\"subtitles\":[]}"); assert(nLegs == 10);
  extras(NULL); assert(nLegs == 10);
  assert(!memcmp(&legs[escolhida], &selecionada, sizeof selecionada));
  // Erro sem URL tambem e avisado, nunca contado como legenda.
  extras("{\"subtitles\":[{\"id\":\"error.upstream\",\"lang\":\"upstream failed\"}]}");
  assert(nLegs == 10);
  assert(teste_merge_selecionada(&selecionada, escolhida));
  // Busca inicial tambem deduplica.
  legManter = 0;
  responses[0] = "{\"subtitles\":[{\"id\":\"a\",\"lang\":\"eng\",\"url\":\"https://fixture.invalid/a.srt\"},{\"id\":\"a\",\"lang\":\"eng\",\"url\":\"https://fixture.invalid/b.srt\"}]}";
  run(1, "tt123", "movie"); assert(nLegs == 1);
  puts("PASS: 8 preserved after error; 10 after merge; duplicates, empty, failure and selected index preserved");
  return 0;
}
