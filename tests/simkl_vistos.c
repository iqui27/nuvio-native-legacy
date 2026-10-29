#include "simkl.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
  const char *j = "{\"movies\":[{\"last_watched_at\":\"2026-10-01T10:00:00Z\",\"movie\":{\"title\":\"Old\",\"ids\":{\"imdb\":\"tt0000001\"}}}],"
    "\"shows\":[{\"show\":{\"title\":\"Show\",\"ids\":{\"imdb\":\"tt0000002\"}},\"seasons\":[{\"number\":1,\"episodes\":["
    "{\"number\":1,\"watched_at\":\"2026-10-05T10:00:00Z\"},{\"number\":2,\"watched_at\":\"2026-10-20T10:00:00Z\"}]}]}],\"anime\":[]}";
  SimklVisto v[8];
  int n = simkl_ler_vistos(j, v, 8);
  assert(n == 3);
  assert(v[0].episodio == 2 && v[1].episodio == 1 && v[2].temporada == 0 && v[2].quando < v[1].quando);
  assert(!strcmp(v[2].imdb, "tt0000001") && !strcmp(v[2].tipo, "movie"));
  assert(simkl_ler_vistos(j, v, 2) == 2 && v[0].episodio == 2);
  assert(simkl_ler_vistos("<html>", v, 8) == 0);
  puts("simkl_vistos ok");
  return 0;
}
