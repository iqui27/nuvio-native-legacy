// Offline Simkl API-shaped fixtures exercise the actual CW merge.
#define SIMKL_ANIME_FIXTURE
#define main simkl_existing_main
#define trakt_enfeitar_lote fixture_enfeitar_lote
#define trakt_enfeitar_lote_sessao fixture_enfeitar_lote_sessao
#define rede_baixar_st_retry previous_retry
#define rede_pedir previous_request
#define rede_resposta_limpar previous_response_clear
#include "simkl_cw.c"
#undef rede_baixar_st_retry
#undef rede_pedir
#undef rede_resposta_limpar
#undef main
#undef trakt_enfeitar_lote
#undef trakt_enfeitar_lote_sessao

#include "metaprov.h"
const char *metaprov_idioma(void) { return "en"; }
char *metaprov_meta_cinemeta_com(const char *t, const char *id, int s, MetaprovGet g, void *ctx) {
  (void)t; (void)id; (void)s; (void)g; (void)ctx; return NULL;
}
unsigned int SDL_GetTicks(void) { return 0; }
void marco(const char *m) { (void)m; }
char *rede_baixar(const char *u, int s) { (void)u; (void)s; return NULL; }
int cwo_conta_a_seguir(const char *id) { (void)id; return 0; }
void cwo_conta_trocar(const char *a, const char *b) { (void)a; (void)b; }
void cwo_marcar_estreia(const char *id, long long ms) { (void)id; (void)ms; }
int cwo_virada_aceita(long long e, long long a) { (void)e; (void)a; return 1; }
int ajustes_tmdb_cw(void) { return 0; }
const char *desc_chave_tmdb(void) { return ""; }
const char *desc_tmdb_idioma(void) { return "en"; }
char *metaprov_meta(const char *tipo, const char *id, int seg, int *prov) {
  (void)tipo; (void)id; (void)seg; (void)prov; return NULL;
}
void metaprov_duracao(char *s, size_t n) { (void)s; (void)n; }

char *rede_baixar_st_retry(const char *u, int s, const char *const *c, int *st, int *retry) {
  *retry = 0;
  return rede_baixar_st(u, s, c, st);
}
int rede_pedir(const RedePedido *p, RedeResposta *r) {
  (void)p; r->erro = REDE_INDISPONIVEL; return 0;
}
void rede_resposta_limpar(RedeResposta *r) { (void)r; }

static const char *episodeMeta;
char *fichameta_ler(const char *key, long ttl) {
  (void)ttl;
  assert(!strcmp(key, "series/tt2560140/en") ||
         !strcmp(key, "series/tt0388629/en") ||
         !strncmp(key, "series/tmdb:", 12) || !strncmp(key, "series/simkl:", 13));
  return episodeMeta && !strcmp(key, "series/tt2560140/en") ? strdup(episodeMeta) : NULL;
}
void fichameta_gravar(const char *key, const char *body) { (void)key; (void)body; }

int main(void) {
  // Unsorted provider episodes, including a special: absolute 26 is S02E01.
  char meta[4096] = "{\"meta\":{\"videos\":[{\"season\":0,\"episode\":1},{\"season\":2,\"episode\":1}";
  for (int e = 25; e > 0; e--) {
    char video[64];
    snprintf(video, sizeof video, ",{\"season\":1,\"episode\":%d}", e);
    strcat(meta, video);
  }
  strcat(meta, "]}}");
  static const char titan[] = "{\"anime\":[{\"next_to_watch\":\"E26\","
    "\"anime\":{\"title\":\"Attack on Titan\",\"ids\":{\"imdb\":\"tt2560140\"}}}]}";
  CatItem mapped[1];
  episodeMeta = meta;
  assert(simkl_ler_assistindo(titan, mapped, 1) == 1);
  assert(!strcmp(mapped[0].imdb, "tt2560140:2:1"));
  assert(mapped[0].temporada == 2 && mapped[0].episodio == 1);
  episodeMeta = NULL;
  assert(simkl_ler_assistindo(titan, mapped, 1) == 1);
  assert(!strcmp(mapped[0].imdb, "tt2560140"));
  assert(mapped[0].temporada == 0 && mapped[0].episodio == 0);
  episodeMeta = "{\"meta\":{\"videos\":[{\"season\":1,\"episode\":25},{\"season\":2,\"episode\":1}]}}";
  assert(simkl_ler_assistindo(titan, mapped, 1) == 1);
  assert(!strcmp(mapped[0].imdb, "tt2560140") && mapped[0].episodio == 0);
  episodeMeta = NULL;
  static const char fixture[] =
    "{\"anime\":["
    "{\"last_watched\":\"E1122\",\"next_to_watch\":\"E1123\","
    "\"last_watched_at\":\"2026-01-22T21:00:00Z\",\"status\":\"watching\","
    "\"anime\":{\"title\":\"One Piece\",\"ids\":{\"simkl\":38636,\"mal\":21,\"anidb\":69,\"tmdb\":37854,\"imdb\":\"tt0388629\"}}},"
    "{\"last_watched\":\"S01E1122\",\"next_to_watch\":\"S01E1123\","
    "\"anime\":{\"title\":\"One Piece duplicate\",\"ids\":{\"imdb\":\"tt0388629\"}}},"
    "{\"last_watched\":\"E5\",\"next_to_watch\":\"E6\","
    "\"anime\":{\"title\":\"TMDB only\",\"ids\":{\"simkl\":8,\"tmdb\":123}}},"
    "{\"last_watched\":\"E9\",\"next_to_watch\":\"E10\","
    "\"anime\":{\"title\":\"Other TMDB\",\"ids\":{\"simkl\":9,\"tmdb\":456}}},"
    "{\"last_watched\":\"E1\",\"next_to_watch\":\"E2\","
    "\"anime\":{\"title\":\"Simkl only\",\"ids\":{\"simkl\":10,\"mal\":11,\"anidb\":12}}},"
    "{\"last_watched\":\"E1122\",\"next_to_watch\":null,"
    "\"anime\":{\"title\":\"Caught up\",\"ids\":{\"simkl\":11}}}]}";
  CatItem v[16];
  tokenAtual = TOKEN;
  PLAYBACK = "[]";
  WATCHING = "{\"shows\":[]}";
  WATCHING_ANIME = fixture;
  int n = simkl_continuar(v, 16);
  assert(n == 4);
  const char *expected[] = {"tt0388629", "tmdb:123", "tmdb:456", "simkl:10"};
  for (int i = 0; i < n; i++) {
    assert(!strcmp(v[i].imdb, expected[i]));
    assert(!strcmp(v[i].tipo, "series") && v[i].progresso == 0);
    assert(simkl_e_a_seguir(v[i].imdb));
  }
  for (int i = 0; i < n; i++) assert(v[i].temporada == 0 && v[i].episodio == 0);
  assert(v[0].tmdb == 37854);
  assert(v[1].tmdb == 123 && v[2].tmdb == 456);
  // Simkl anime uses bare absolute numbers too. Prime bodies, then reuse them
  // with unchanged activities; changed stub bodies must never be requested.
  simkl_esquecer();
  WATCHING_ANIME = "{\"anime\":[{\"last_watched\":1122,\"next_to_watch\":1123,"
    "\"status\":\"watching\",\"anime\":{\"title\":\"One Piece\","
    "\"ids\":{\"imdb\":\"tt0388629\",\"tmdb\":37854}}}]}";
  (void)simkl_continuar(v, 16);
  int requests = totalBatidas();
  WATCHING_ANIME = "{\"anime\":[]}";
  n = simkl_continuar(v, 16);
  assert(totalBatidas() == requests + 1); // activities only, cached anime body
  assert(n == 1 && !strcmp(v[0].imdb, "tt0388629"));
  assert(v[0].tmdb == 37854 && simkl_e_a_seguir(v[0].imdb));
  assert(v[0].temporada == 0 && v[0].episodio == 0); // unknown provider mapping
  assert(simkl_ler_assistindo("{\"anime\":[{\"next_to_watch\":\"1123\","
    "\"anime\":{\"ids\":{\"imdb\":\"tt0388629\"}}}]}", mapped, 1) == 1);
  assert(simkl_ler_assistindo("{\"anime\":[{\"next_to_watch\":null,"
    "\"anime\":{\"ids\":{\"imdb\":\"tt0388629\"}}}]}", mapped, 1) == 0);
  proximoTeste = 0;
  assert(simkl_continuar(v, 16) == 0);
  puts("PASS: absolute anime provider mapping, title fallback, dedup and next-up setting");
  return 0;
}
