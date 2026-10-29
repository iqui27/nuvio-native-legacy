#ifndef LISTAS_SOURCE
#define LISTAS_SOURCE "../src/listas.c"
#endif
#include LISTAS_SOURCE
#include <assert.h>
static char *fixture(const char *path) {
  FILE *f = fopen(path, "rb"); assert(f);
  fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
  char *s = calloc((size_t)n + 5, 1); assert(s);
  memcpy(s, " \r\n\t", 4);
  assert(fread(s + 4, 1, (size_t)n, f) == (size_t)n); fclose(f); return s;
}
int main(void) {
  char *s = fixture("tests/fixtures/trakt_minhas_listas.json");
  assert(lst_ler_trakt(s, LST_JS_MINHAS) == 2);
  assert(lst_lista(0)->propria && lst_lista(0)->traktId == 1049904);
  free(s); s = fixture("tests/fixtures/trakt_busca_listas.json");
  assert(lst_ler_trakt(s, LST_JS_ENVELOPE) == 2);
  const LstLista *l = lst_lista(0);
  assert(l->traktId == 8842211 && !l->propria && l->itens > 0);
  assert(!strcmp(l->autor, "cinefilo_br"));
  assert(lst_ler_trakt("{\"error\":\"denied\"}", LST_JS_ENVELOPE) == 0);
  free(s);
  char large[5000]; memset(large, 'x', sizeof large);
  const char *prefix = "[{\"list\":{\"name\":\"Long public list\",\"ids\":{\"trakt\":99},\"description\":\"";
  memcpy(large, prefix, strlen(prefix));
  strcpy(large + 4500, "\"}}]");
  assert(lst_ler_trakt(large, LST_JS_ENVELOPE) == 1);
  puts("Personal/public list parser: PASS"); return 0;
}
