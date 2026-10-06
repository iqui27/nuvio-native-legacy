// O Trakt como fonte do Diario: leitura de /sync/history e /sync/watched/*
// com os formatos da documentacao do Trakt. Sem rede (dubles).
#include "diariotrakt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int perfis_ativo(void) { return 1; }
char *dados_ler(const char *n) { (void)n; return NULL; }
int dados_gravar(const char *n, const char *c) { (void)n; (void)c; return 1; }
int trakt_ativo(void) { return 0; }
int trakt_cabecalhos(const char **c, char *a, size_t na, char *k, size_t nk) { (void)c; (void)a; (void)na; (void)k; (void)nk; return 0; }
char *rede_baixar_com(const char *u, int s, const char *const *c) { (void)u; (void)s; (void)c; return NULL; }

static const char *HIST =
"[{\"id\":1,\"watched_at\":\"2026-10-05T23:10:00.000Z\",\"action\":\"watch\",\"type\":\"episode\","
 "\"episode\":{\"season\":2,\"number\":5,\"title\":\"X\",\"ids\":{\"trakt\":9,\"tvdb\":1,\"imdb\":\"tt7777777\",\"tmdb\":555}},"
 "\"show\":{\"title\":\"Ruptura\",\"year\":2022,\"ids\":{\"trakt\":2,\"slug\":\"severance\",\"imdb\":\"tt11280740\",\"tmdb\":95396}}},"
 "{\"id\":2,\"watched_at\":\"2026-10-01T20:00:00.000Z\",\"action\":\"scrobble\",\"type\":\"movie\","
 "\"movie\":{\"title\":\"Duna\",\"year\":2021,\"ids\":{\"trakt\":3,\"slug\":\"dune\",\"imdb\":\"tt1160419\",\"tmdb\":438631}}},"
 "{\"id\":3,\"watched_at\":\"2026-09-01T20:00:00.000Z\",\"type\":\"movie\",\"movie\":{\"title\":\"Sem imdb\",\"ids\":{\"trakt\":4}}}]";

static const char *FILMES =
"[{\"plays\":2,\"last_watched_at\":\"2026-03-01T21:00:00.000Z\",\"last_updated_at\":\"x\","
 "\"movie\":{\"title\":\"Duna\",\"year\":2021,\"ids\":{\"imdb\":\"tt1160419\",\"tmdb\":438631}}}]";
static const char *SERIES =
"[{\"plays\":3,\"last_watched_at\":\"2026-10-05T23:10:00.000Z\",\"show\":{\"title\":\"Ruptura\",\"year\":2022,"
 "\"ids\":{\"imdb\":\"tt11280740\",\"tmdb\":95396}},\"seasons\":[{\"number\":1,\"episodes\":["
 "{\"number\":1,\"plays\":1,\"last_watched_at\":\"2025-01-01T10:00:00.000Z\"},"
 "{\"number\":2,\"plays\":1,\"last_watched_at\":\"2025-01-02T10:00:00.000Z\"}]},"
 "{\"number\":2,\"episodes\":[{\"number\":5,\"plays\":1,\"last_watched_at\":\"2026-10-05T23:10:00.000Z\"}]}]}]";

int main(void) {
  DiarioEntrada v[8];
  long long novo;
  char t[64]; int ano;
  int n = diariotrakt_ler_historico(HIST, v, 8, &novo);
  assert(n == 2);
  // O imdb e o da SERIE (nao o do episodio, que vem antes no JSON).
  assert(!strcmp(v[0].id, "tt11280740") && v[0].temporada == 2 && v[0].episodio == 5 && v[0].tipo == 's');
  assert(v[0].tmdb == 0 && v[0].origem == 't' && v[0].hora == -1);
  assert(!strcmp(v[1].id, "tt1160419") && v[1].tipo == 'm' && v[1].tmdb == 438631);
  assert(novo == v[0].quando && v[0].quando > v[1].quando);
  assert(diario_nome("tt11280740", t, sizeof t, &ano, NULL, 0) && !strcmp(t, "Ruptura") && ano == 2022);
  assert(diariotrakt_ler_vistos(FILMES, 0, v, 8) == 1 && !strcmp(v[0].id, "tt1160419") && v[0].tmdb == 438631);
  n = diariotrakt_ler_vistos(SERIES, 1, v, 8);
  assert(n == 3 && v[0].temporada == 1 && v[0].episodio == 1 && v[2].temporada == 2 && v[2].episodio == 5);
  assert(diariotrakt_ler_vistos(SERIES, 1, v, 2) == 2);     // teto
  assert(diariotrakt_ler_historico("{\"error\":1}", v, 8, &novo) == 0);
  // Historico e completude da MESMA vez se fundem no Diario (mesmo instante).
  n = diariotrakt_ler_historico(HIST, v, 8, &novo);
  diario_adicionar_lote(1, v, n);
  n = diariotrakt_ler_vistos(SERIES, 1, v, 8);
  diario_adicionar_lote(1, v, n);
  assert(diario_episodio(1, "tt11280740", 2, 5, NULL) == 1 && diario_n(1) == 4);
  printf("diariotrakt: ok\n");
  return 0;
}
