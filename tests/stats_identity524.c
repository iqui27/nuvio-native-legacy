// Real Trakt stats and CW enrichment: authoritative movie history must not
// trigger speculative series GETs or borrow another title from the catalog.
#define main ep_fixture_main
#define rede_baixar_st_retry unused_retry
#include "trakt_ep_cinemeta.c"
#undef main
#undef rede_baixar_st_retry
#include "../src/trakt.c"

int simkl_e_a_seguir(const char *id) { (void)id; return 0; }
int rede_pedir(const RedePedido *p, RedeResposta *r) {
  (void)p; r->erro = REDE_INDISPONIVEL; return 0;
}
void rede_resposta_limpar(RedeResposta *r) { (void)r; }
static int requests;
char *rede_baixar_st_retry(const char *u, int s, const char *const *h, int *st, int *retry) {
  (void)s; (void)h; *st = 200; *retry = 0; requests++;
  if (strstr(u, "/users/settings")) return strdup("{}");
  assert(strstr(u, "/users/me/history?"));
  return strdup("[{\"watched_at\":\"2026-10-01T12:00:00Z\",\"movie\":{\"title\":\"Chasing Mavericks\",\"runtime\":116,\"ids\":{\"imdb\":\"tt1629757\"}}},{\"watched_at\":\"2026-10-01T12:00:00Z\",\"show\":{\"title\":\"Slime\",\"ids\":{\"imdb\":\"tt9054364\"}},\"episode\":{\"season\":1,\"number\":2,\"runtime\":24}}]");
}
int main(void) {
  static PerfilDados stats;
  static CatItem movie;
  ligado = 1;
  snprintf(token, sizeof token, "fixture-token");
  snprintf(cliente, sizeof cliente, "fixture-client");
  assert(trakt_perfil(&stats));
  assert(requests == 2 && stats.plays == 2 && stats.filmes == 1 && stats.episodios == 1);
  assert(stats.minutos == 140 && stats.nDestaques == 2);
  assert(!strcmp(stats.destaques[0].titulo, "Chasing Mavericks"));
  assert(!strcmp(stats.destaques[1].id, "tt9054364"));
  assert(!strcmp(stats.destaques[1].titulo, "Slime"));
  assert(!strcmp(stats.destaques[1].detalhe, "T1E2"));
  snprintf(movie.imdb, sizeof movie.imdb, "tt1629757");
  snprintf(movie.tipo, sizeof movie.tipo, "movie");
  snprintf(movie.titulo, sizeof movie.titulo, "Chasing Mavericks");
  movie.poster[0] = movie.backdrop[0] = movie.sinopse[0] = 'x'; movie.nota = 70;
  assert(trakt_enfeitar_lote(&movie, 1) == 1);
  assert(requests == 2 && !strcmp(movie.titulo, "Chasing Mavericks"));
  puts("stats #524: movie/episode counts, ranking identity, zero speculative GETs PASS");
  return 0;
}
