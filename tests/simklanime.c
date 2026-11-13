// The Simkl anime cache (src/simklanime.c): one file, every answer its own
// entry, expiry by age, the size cap and a damaged file.
#include "simklanime.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// The data directory: just this cache's file, in memory.
static char *arquivo;
static int gravacoes;
char *dados_ler(const char *nome) {
  assert(!strcmp(nome, SIMKLANIME_ARQ));
  return arquivo ? strdup(arquivo) : NULL;
}
int dados_gravar_leve(const char *nome, const char *conteudo) {
  assert(!strcmp(nome, SIMKLANIME_ARQ));
  free(arquivo);
  arquivo = strdup(conteudo);
  gravacoes++;
  return 1;
}

static int tem(const char *chave, long ttl, const char *esperado) {
  char *c = simklanime_ler(chave, ttl);
  int ok = c && (!esperado || !strcmp(c, esperado));
  free(c);
  return ok;
}

int main(void) {
  const long dia = 24 * 3600;
  long long agora = (long long)time(NULL);

  // Nothing saved yet.
  assert(!simklanime_ler("simkl-anime/1", dia));

  // The two episode lists that shared ficha-62 in the old cache: both stay.
  simklanime_gravar("simkl-anime-eps/2830474", "[{\"episode\":1}]");
  simklanime_gravar("simkl-anime-eps/620039", "[{\"episode\":2}]");
  assert(tem("simkl-anime-eps/2830474", dia, "[{\"episode\":1}]"));
  assert(tem("simkl-anime-eps/620039", dia, "[{\"episode\":2}]"));
  assert(gravacoes == 2);

  // Saving again replaces, never duplicates.
  simklanime_gravar("simkl-anime-eps/620039", "[{\"episode\":3}]");
  assert(tem("simkl-anime-eps/620039", dia, "[{\"episode\":3}]"));

  // Age: two days old is gone for a one-day limit, there for thirty days.
  simklanime_gravar_em("simkl-anime/3320816", "{}", agora - 2 * dia);
  assert(!tem("simkl-anime/3320816", dia, NULL));
  assert(tem("simkl-anime/3320816", 30 * dia, "{}"));
  // A save "from the future" (the clock moved back) is not trusted.
  simklanime_gravar_em("simkl-anime/9", "{}", agora + 3600);
  assert(!tem("simkl-anime/9", 30 * dia, NULL));

  // Survives a relaunch: the file holds everything, ages included.
  simklanime_descarregar();
  assert(tem("simkl-anime-eps/2830474", dia, "[{\"episode\":1}]"));
  assert(tem("simkl-anime-eps/620039", dia, "[{\"episode\":3}]"));
  assert(!tem("simkl-anime/3320816", dia, NULL));
  assert(tem("simkl-anime/3320816", 30 * dia, "{}"));

  // A body with newlines and spaces round-trips (lengths, not lines).
  simklanime_gravar("simkl-anime/7", "{\"title\":\"a\nb c\"}");
  simklanime_descarregar();
  assert(tem("simkl-anime/7", dia, "{\"title\":\"a\nb c\"}"));

  // Keys that cannot be stored are refused, not written wrong.
  gravacoes = 0;
  simklanime_gravar("bad key", "{}");
  simklanime_gravar("", "{}");
  assert(gravacoes == 0);

  // The cap: past 1 MB the oldest go first; the one just saved stays.
  {
    size_t n = 300 * 1024;
    char *grande = malloc(n + 1);
    char chave[32];
    memset(grande, 'x', n);
    grande[n] = 0;
    for (int i = 0; i < 4; i++) {
      snprintf(chave, sizeof chave, "simkl-anime/big%d", i);
      simklanime_gravar_em(chave, grande, agora - (10 - i));
    }
    assert(!tem("simkl-anime/big0", 30 * dia, NULL));   // oldest, dropped
    assert(tem("simkl-anime/big3", 30 * dia, NULL));    // newest, kept
    assert(strlen(arquivo) <= SIMKLANIME_MAX_BYTES + 4096);
    // Larger than the cap on its own: not kept, nothing else lost.
    {
      char *enorme = malloc(SIMKLANIME_MAX_BYTES + 2);
      memset(enorme, 'y', SIMKLANIME_MAX_BYTES + 1);
      enorme[SIMKLANIME_MAX_BYTES + 1] = 0;
      simklanime_gravar("simkl-anime/enorme", enorme);
      assert(!tem("simkl-anime/enorme", dia, NULL));
      assert(tem("simkl-anime/big3", 30 * dia, NULL));
      free(enorme);
    }
    free(grande);
  }

  // A damaged file (cut short) or another version: ignored, starts empty,
  // and the next save writes a good file again.
  {
    char *cortado = strdup(arquivo);
    cortado[strlen(cortado) / 2] = 0;
    free(arquivo);
    arquivo = cortado;
    simklanime_descarregar();
    assert(!tem("simkl-anime/big3", 30 * dia, NULL));
    simklanime_gravar("simkl-anime/1", "{\"title\":\"ok\"}");
    simklanime_descarregar();
    assert(tem("simkl-anime/1", dia, "{\"title\":\"ok\"}"));
  }
  free(arquivo);
  arquivo = strdup("nuvio-simkl-anime 2\n0 2 simkl-anime/1\n{}\n");
  simklanime_descarregar();
  assert(!tem("simkl-anime/1", 30 * dia, NULL));

  puts("PASS: simkl anime cache");
  return 0;
}
