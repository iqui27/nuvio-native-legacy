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

int trakt_enfeitar_lote(CatItem *saida, int n) { (void)saida; return n; }
int trakt_enfeitar_lote_sessao(CatItem *saida, int n, uint64_t sessao) {
  (void)saida; (void)sessao; return n;
}

#include "metaprov.h"
const char *metaprov_idioma(void) { return "en"; }
static const char *providerMeta;
static int providerFetches, metadataWrites, detailWrites, season4DetailWrites;
static const char *onePieceProviderMeta, *onePieceCachedMeta;
static int onePieceProviderFetches, onePieceMetadataWrites;
static int onePieceDetailWrites;
char *metaprov_meta(const char *tipo, const char *id, int seg, int *prov) {
  assert(!strcmp(tipo, "series"));
  (void)prov;
  assert(seg == 6);
  if (!strcmp(id, "tt2560140")) {
    providerFetches++;
    return providerMeta ? strdup(providerMeta) : NULL;
  }
  if (!strcmp(id, "tt0388629")) {
    onePieceProviderFetches++;
    return onePieceProviderMeta ? strdup(onePieceProviderMeta) : NULL;
  }
  return NULL;
}
static const char *onePieceCinemeta;
static int onePieceCinemetaFetches;
char *metaprov_meta_cinemeta_com(const char *tipo, const char *id, int seg,
                                 MetaprovGet get, void *ctx,
                                 MetaprovCineResult *result) {
  (void)seg; (void)get; (void)ctx;
  assert(!strcmp(tipo, "series"));
  if (result) *result = METAPROV_CINE_RETRY;
  if (strcmp(id, "tt0388629") || !onePieceCinemeta) return NULL;
  onePieceCinemetaFetches++;
  return strdup(onePieceCinemeta);
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
         !strcmp(key, "series/tt11034066/en") ||
         !strncmp(key, "series/tmdb:", 12) || !strncmp(key, "series/simkl:", 13) ||
         !strncmp(key, "simkl-anime/", 12) || !strncmp(key, "simkl-anime-eps/", 16));
  if (!strcmp(key, "series/tt2560140/en"))
    return episodeMeta ? strdup(episodeMeta) : NULL;
  if (!strcmp(key, "series/tt0388629/en"))
    return onePieceCachedMeta ? strdup(onePieceCachedMeta) : NULL;
  return NULL;
}
void fichameta_gravar(const char *key, const char *body) {
  assert(body && *body);
  if (!strcmp(key, "series/tt2560140/en")) metadataWrites++;
  else if (!strcmp(key, "series/tt0388629/en")) onePieceMetadataWrites++;
  else if (!strncmp(key, "simkl-anime/", 12)) {
    if (!strcmp(key, "simkl-anime/263883")) detailWrites++;
    else if (!strcmp(key, "simkl-anime/2638832")) season4DetailWrites++;
    else if (!strcmp(key, "simkl-anime/38636")) onePieceDetailWrites++;
  } else if (strncmp(key, "simkl-anime-eps/", 16)) assert(0);
}

static char *lerFixture(const char *caminho) {
  FILE *f = fopen(caminho, "rb");
  long n;
  char *b;
  assert(f);
  assert(fseek(f, 0, SEEK_END) == 0);
  n = ftell(f);
  assert(n > 0 && fseek(f, 0, SEEK_SET) == 0);
  b = malloc((size_t)n + 1);
  assert(b && fread(b, 1, (size_t)n, f) == (size_t)n);
  b[n] = 0;
  fclose(f);
  return b;
}

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
  providerMeta = meta;
  assert(simkl_ler_assistindo(titan, mapped, 1) == 1);
  assert(!strcmp(mapped[0].imdb, "tt2560140:2:1"));
  assert(mapped[0].temporada == 2 && mapped[0].episodio == 1);
  assert(providerFetches == 2 && metadataWrites == 1);
  providerMeta = NULL;
  episodeMeta = "{\"meta\":{\"videos\":[{\"season\":1,\"episode\":25},{\"season\":2,\"episode\":1}]}}";
  assert(simkl_ler_assistindo(titan, mapped, 1) == 1);
  assert(!strcmp(mapped[0].imdb, "tt2560140") && mapped[0].episodio == 0);
  static const char titanShow[] = "{\"anime\":[{\"next_to_watch\":\"E26\","
    "\"show\":{\"title\":\"Attack on Titan\",\"ids\":{\"imdb\":\"tt2560140\"}}}]}";
  episodeMeta = meta;
  assert(simkl_ler_assistindo(titanShow, mapped, 1) == 1);
  assert(!strcmp(mapped[0].imdb, "tt2560140:2:1"));
  episodeMeta = NULL;

  providerMeta = meta;
  tokenAtual = TOKEN;
  PLAYBACK = "[]";
  WATCHING = "{\"shows\":[]}";
  WATCHING_ANIME = "{\"anime\":[{\"next_to_watch\":\"E26\","
    "\"anime\":{\"title\":\"Anime sem IDs externos\",\"ids\":{\"simkl\":263883}}}]}";
  snprintf(atividades, sizeof atividades, "{\"all\":\"2026-10-30T21:00:00Z\"}");
  CatItem resolved[1];
  assert(simkl_continuar(resolved, 1) == 1);
  assert(!strcmp(resolved[0].imdb, "tt2560140:2:1"));
  assert(resolved[0].tmdb == 98765);
  assert(!strcmp(resolved[0].poster, "https://simkl.in/posters/poster-key_m.jpg"));
  assert(resolved[0].temporada == 2 && resolved[0].episodio == 1);
  assert(detailWrites == 1 && metadataWrites == 2);
  providerMeta = NULL;

  simkl_esquecer();
  WATCHING_ANIME = "{\"anime\":[{\"next_to_watch\":\"E1\","
    "\"anime\":{\"title\":\"Mairimashita! Iruma-kun\","
    "\"ids\":{\"simkl\":2638832}}}]}";
  assert(simkl_continuar(resolved, 1) == 1);
  assert(!strcmp(resolved[0].imdb, "tt11034066:4:1"));
  assert(!strcmp(resolved[0].titulo, "Welcome to Demon School! Iruma-kun season 4"));
  assert(resolved[0].tmdb == 91801);
  assert(!strcmp(resolved[0].poster, "https://simkl.in/posters/19/19613958579eba8187_m.jpg"));
  assert(resolved[0].temporada == 4 && resolved[0].episodio == 1);

  simkl_esquecer();
  WATCHING_ANIME = "{\"anime\":[{\"next_to_watch\":\"S01E01\","
    "\"anime\":{\"title\":\"Mairimashita! Iruma-kun\","
    "\"ids\":{\"simkl\":2638832,\"imdb\":\"tt11034066\"}}}]}";
  assert(simkl_continuar(resolved, 1) == 1);
  assert(!strcmp(resolved[0].imdb, "tt11034066:4:1"));
  assert(!strcmp(resolved[0].titulo, "Welcome to Demon School! Iruma-kun season 4"));
  assert(resolved[0].temporada == 4 && resolved[0].episodio == 1);

  simkl_esquecer();
  WATCHING_ANIME = "{\"anime\":[{\"next_to_watch\":\"S01E01\","
    "\"anime\":{\"title\":\"Mairimashita! Iruma-kun\","
    "\"ids\":{\"simkl\":2638832}}}]}";
  assert(simkl_continuar(resolved, 1) == 1);
  assert(!strcmp(resolved[0].imdb, "tt11034066:4:1"));
  assert(!strcmp(resolved[0].titulo, "Welcome to Demon School! Iruma-kun season 4"));
  assert(resolved[0].temporada == 4 && resolved[0].episodio == 1);
  assert(season4DetailWrites == 3);

  // One Simkl entry per season, only a Simkl ID (Kaiju No. 8 Season 2): the
  // episode name, still and TVDB placement come from that entry's episode list,
  // not from Cinemeta, which may number the show as one season.
  simkl_esquecer();
  WATCHING_ANIME = "{\"anime\":[{\"next_to_watch\":\"E7\","
    "\"show\":{\"title\":\"Mairimashita! Iruma-kun\",\"ids\":{\"simkl\":2638832}}}]}";
  assert(simkl_continuar(resolved, 1) == 1);
  assert(!strcmp(resolved[0].imdb, "tt11034066:4:7"));
  assert(resolved[0].temporada == 4 && resolved[0].episodio == 7);
  assert(!strcmp(resolved[0].nomeEpisodio, "Seventh"));
  assert(!strcmp(resolved[0].thumbEpisodio, "https://simkl.in/episodes/19/19613958579eba8189_w.jpg"));
  // An episode missing from Simkl's list: nothing invented, numbering as before.
  simkl_esquecer();
  WATCHING_ANIME = "{\"anime\":[{\"next_to_watch\":\"E3\","
    "\"show\":{\"title\":\"Mairimashita! Iruma-kun\",\"ids\":{\"simkl\":2638832}}}]}";
  assert(simkl_continuar(resolved, 1) == 1);
  assert(!strcmp(resolved[0].imdb, "tt11034066:4:3"));
  assert(!resolved[0].nomeEpisodio[0] && !resolved[0].thumbEpisodio[0]);
  puts("PASS: per-season anime takes name and still from its Simkl episode list");

  FILE *onePieceFile = fopen("tests/fixtures/onepiece/cinemeta_tt0388629.json", "rb");
  assert(onePieceFile);
  assert(fseek(onePieceFile, 0, SEEK_END) == 0);
  long onePieceSize = ftell(onePieceFile);
  assert(onePieceSize > 0 && fseek(onePieceFile, 0, SEEK_SET) == 0);
  char *onePieceMeta = malloc((size_t)onePieceSize + 1);
  assert(onePieceMeta);
  assert(fread(onePieceMeta, 1, (size_t)onePieceSize, onePieceFile) == (size_t)onePieceSize);
  onePieceMeta[onePieceSize] = 0;
  fclose(onePieceFile);
  onePieceCachedMeta = "{\"meta\":{\"videos\":[{\"season\":1,\"episode\":1}]}}";
  onePieceProviderMeta = onePieceMeta;
  WATCHING_ANIME = "{\"anime\":[{\"next_to_watch\":\"E1123\","
    "\"anime\":{\"title\":\"One Piece\",\"ids\":{\"imdb\":\"tt0388629\"}}}]}";
  assert(simkl_ler_assistindo(WATCHING_ANIME, mapped, 1) == 1);
  assert(!strcmp(mapped[0].imdb, "tt0388629:22:39"));
  assert(mapped[0].temporada == 22 && mapped[0].episodio == 39);
  assert(onePieceProviderFetches == 1 && onePieceMetadataWrites == 1);

  static const int ends[] = {
    61, 77, 91, 130, 143, 195, 228, 263, 336, 381, 407, 421,
    522, 580, 642, 692, 748, 803, 877, 891, 1088, 1155
  };
  char *nuvioMeta = malloc(65536);
  assert(nuvioMeta);
  size_t nuvioUsed = (size_t)snprintf(nuvioMeta, 65536, "{\"meta\":{\"videos\":[");
  int season = 0;
  for (int absolute = 1; absolute <= 1123; absolute++) {
    while (absolute > ends[season]) season++;
    int written;
    if (absolute == 1123) {
      written = snprintf(nuvioMeta + nuvioUsed, 65536 - nuvioUsed,
        "%s{\"season\":%d,\"episode\":%d,"
        "\"thumbnail\":\"https://image.tmdb.org/t/p/w500/5VFdmFzQdmpre1o9qtAqoLXOzsl.jpg\"}",
        absolute > 1 ? "," : "", season + 1, absolute);
    } else {
      written = snprintf(nuvioMeta + nuvioUsed, 65536 - nuvioUsed,
        "%s{\"season\":%d,\"episode\":%d}", absolute > 1 ? "," : "",
        season + 1, absolute);
    }
    assert(written > 0 && (size_t)written < 65536 - nuvioUsed);
    nuvioUsed += (size_t)written;
  }
  assert(nuvioUsed + sizeof "]}}" <= 65536);
  strcat(nuvioMeta, "]}}");
  onePieceCachedMeta = "{\"meta\":{\"videos\":[{\"season\":1,\"episode\":1}]}}";
  onePieceProviderMeta = nuvioMeta;
  onePieceProviderFetches = 0;
  assert(simkl_ler_assistindo(WATCHING_ANIME, mapped, 1) == 1);
  assert(!strcmp(mapped[0].imdb, "tt0388629:22:1123"));
  assert(mapped[0].temporada == 22 && mapped[0].episodio == 1123);
  assert(!strcmp(mapped[0].thumbEpisodio,
    "https://image.tmdb.org/t/p/w500/5VFdmFzQdmpre1o9qtAqoLXOzsl.jpg"));
  assert(onePieceProviderFetches == 1);

  simkl_esquecer();
  tokenAtual = TOKEN;
  PLAYBACK = "[]";
  WATCHING = "{\"shows\":[]}";
  snprintf(atividades, sizeof atividades, "{\"all\":\"2026-10-30T21:30:00Z\"}");
  assert(simkl_continuar(resolved, 1) == 1);
  assert(!strcmp(resolved[0].imdb, "tt0388629:22:1123"));
  assert(resolved[0].temporada == 22 && resolved[0].episodio == 1123);
  assert(!strcmp(resolved[0].thumbEpisodio,
    "https://image.tmdb.org/t/p/w500/5VFdmFzQdmpre1o9qtAqoLXOzsl.jpg"));
  static const char onePieceHistory[] =
    "{\"movies\":[],\"shows\":[],\"anime\":[{\"anime\":{\"ids\":{\"imdb\":\"tt0388629\"}},"
    "\"seasons\":[{\"number\":1,\"episodes\":["
    "{\"number\":1123,\"tvdb_season\":22,\"tvdb_number\":35,"
    "\"watched_at\":\"2026-10-12T12:00:00Z\"}]}]}]}";
  onePieceCachedMeta = nuvioMeta;
  nHistorico = 0;
  assert(simkl_historico_ler(onePieceHistory, "tt0388629", 1) == 1);
  assert(nHistorico == 1 && !strcmp(historicoIds[0], "tt0388629"));
  assert(historicoPares[0].temporada == 22 && historicoPares[0].episodio == 1123);

  static const char onePieceNestedTvdbHistory[] =
    "{\"movies\":[],\"shows\":[],\"anime\":[{\"anime\":{\"ids\":{\"imdb\":\"tt0388629\"}},"
    "\"seasons\":[{\"number\":1,\"episodes\":[{\"number\":35,"
    "\"tvdb\":{\"season\":22,\"episode\":35},"
    "\"watched_at\":\"2026-10-12T12:00:00Z\"}]}]}]}";
  nHistorico = 0;
  assert(simkl_historico_ler(onePieceNestedTvdbHistory, "tt0388629", 1) == 1);
  assert(nHistorico == 1 && historicoPares[0].temporada == 22 &&
         historicoPares[0].episodio == 1123);

  onePieceCachedMeta = "{\"meta\":{\"videos\":[{\"season\":1,\"episode\":1}]}}";
  onePieceProviderMeta = onePieceCachedMeta;
  onePieceProviderFetches = 0;
  static const char onePieceUnmappedHistory[] =
    "{\"movies\":[],\"shows\":[],\"anime\":[{\"anime\":{\"ids\":{\"imdb\":\"tt0388629\"}},"
    "\"seasons\":[{\"number\":1,\"episodes\":["
    "{\"number\":1500,\"watched_at\":\"2026-10-12T12:00:00Z\"},"
    "{\"number\":1501,\"watched_at\":\"2026-10-12T12:01:00Z\"}]}]}]}";
  nHistorico = 0;
  assert(simkl_historico_ler(onePieceUnmappedHistory, "tt0388629", 1) == 0);
  assert(nHistorico == 0 && onePieceProviderFetches == 1);
  onePieceCachedMeta = onePieceProviderMeta = NULL;
  onePieceProviderFetches = 0;
  assert(simkl_historico_ler(onePieceUnmappedHistory, "tt0388629", 1) == 0);
  assert(nHistorico == 0 && onePieceProviderFetches == 1);
  onePieceCachedMeta = onePieceProviderMeta = nuvioMeta;

  simkl_esquecer();
  WATCHING_ANIME = "{\"anime\":[{\"next_to_watch\":\"E1123\","
    "\"anime\":{\"title\":\"One Piece\",\"ids\":{\"simkl\":38636}}}]}";
  assert(simkl_continuar(resolved, 1) == 1);
  assert(!strcmp(resolved[0].imdb, "tt0388629:22:1123"));
  static const char onePieceSimklIdHistory[] =
    "{\"movies\":[],\"shows\":[],\"anime\":[{\"anime\":{\"ids\":{\"simkl\":38636}},"
    "\"seasons\":[{\"number\":1,\"episodes\":["
    "{\"number\":1,\"watched_at\":\"2026-10-12T12:00:00Z\"}]}]}]}";
  nHistorico = 0;
  assert(simkl_historico_ler(onePieceSimklIdHistory, "tt0388629", 1) == 1);
  assert(nHistorico == 1 && !strcmp(historicoIds[0], "tt0388629"));
  assert(historicoPares[0].temporada == 1 && historicoPares[0].episodio == 1);
  assert(onePieceDetailWrites == 1);

  static const char onePieceTvdbHistory[] =
    "{\"movies\":[],\"shows\":[],\"anime\":[{\"anime\":{\"ids\":{\"imdb\":\"tt0388629\"}},"
    "\"seasons\":["
    "{\"number\":2,\"episodes\":[{\"number\":1,\"tvdb_season\":2,\"tvdb_number\":1,"
    "\"watched_at\":\"2026-10-12T12:00:00Z\"}]},"
    "{\"number\":3,\"episodes\":[{\"number\":14,\"tvdb_season\":3,\"tvdb_number\":14,"
    "\"watched_at\":\"2026-10-12T12:00:00Z\"}]},"
    "{\"number\":22,\"episodes\":[{\"number\":35,\"tvdb_season\":22,\"tvdb_number\":35,"
    "\"watched_at\":\"2026-10-12T12:00:00Z\"}]}]}]}";
  nHistorico = 0;
  assert(simkl_historico_ler(onePieceTvdbHistory, "tt0388629", 1) == 3);
  assert(nHistorico == 3);
  assert(!strcmp(historicoIds[0], "tt0388629"));
  assert(historicoPares[0].temporada == 2 && historicoPares[0].episodio == 62);
  assert(historicoPares[1].temporada == 3 && historicoPares[1].episodio == 91);
  assert(historicoPares[2].temporada == 22 && historicoPares[2].episodio == 1123);

  snprintf(historicoCatalogId, sizeof historicoCatalogId, "%s", "tt0388629");
  historicoCatalogEpsQtd = 64;
  historicoCatalogEpsFinal = 1;
  for (int i = 0; i < 61; i++) {
    historicoCatalogEps[i].temporada = 1;
    historicoCatalogEps[i].episodio = (short)(i + 1);
  }
  for (int i = 61; i < historicoCatalogEpsQtd; i++) {
    historicoCatalogEps[i].temporada = 2;
    historicoCatalogEps[i].episodio = (short)(i - 60);
  }
  static const char onePieceAbsoluteHistory[] =
    "{\"movies\":[],\"shows\":[],\"anime\":[{\"anime\":{\"ids\":{\"imdb\":\"tt0388629\"}},"
    "\"seasons\":[{\"number\":1,\"episodes\":["
    "{\"number\":1,\"watched_at\":\"2026-10-12T12:00:00Z\"},"
    "{\"number\":61,\"watched_at\":\"2026-10-12T12:01:00Z\"},"
    "{\"number\":62,\"watched_at\":\"2026-10-12T12:02:00Z\"},"
    "{\"number\":63,\"watched_at\":\"2026-10-12T12:03:00Z\"},"
    "{\"number\":65,\"watched_at\":\"2026-10-12T12:04:00Z\"}]}]}]}";
  nHistorico = 0;
  assert(simkl_historico_ler(onePieceAbsoluteHistory, "tt0388629", 1) == 5);
  assert(nHistorico == 5);
  assert(historicoPares[0].temporada == 1 && historicoPares[0].episodio == 1);
  assert(historicoPares[1].temporada == 1 && historicoPares[1].episodio == 61);
  assert(historicoPares[2].temporada == 2 && historicoPares[2].episodio == 1);
  assert(historicoPares[3].temporada == 2 && historicoPares[3].episodio == 2);
  // Past the end of the loaded episode list: the provider metadata decides.
  assert(historicoPares[4].temporada == 2 && historicoPares[4].episodio == 65);

  // The catalog appends new pairs at the end, so it is not in watch order.
  // Opening the season list early must not shift every absolute number.
  {
    CatEp primeiro = historicoCatalogEps[0];
    memmove(historicoCatalogEps, historicoCatalogEps + 1,
            sizeof historicoCatalogEps[0] * (size_t)(historicoCatalogEpsQtd - 1));
    historicoCatalogEps[historicoCatalogEpsQtd - 1] = primeiro;
  }
  nHistorico = 0;
  assert(simkl_historico_ler(onePieceAbsoluteHistory, "tt0388629", 1) == 5);
  assert(historicoPares[0].temporada == 1 && historicoPares[0].episodio == 1);
  assert(historicoPares[1].temporada == 1 && historicoPares[1].episodio == 61);
  assert(historicoPares[2].temporada == 2 && historicoPares[2].episodio == 1);
  historicoCatalogId[0] = 0;
  historicoCatalogEpsQtd = historicoCatalogEpsFinal = 0;

  snprintf(historicoCatalogId, sizeof historicoCatalogId, "%s", "tt0388629");
  historicoCatalogTvdb = 123456;
  historicoCatalogTemporada = 22;
  historicoCatalogEpisodio = 1123;
  static const char onePieceExactTvdbHistory[] =
    "{\"movies\":[],\"shows\":[],\"anime\":[{\"anime\":{\"ids\":{\"imdb\":\"tt0388629\"}},"
    "\"seasons\":[{\"number\":1,\"episodes\":[{\"number\":36,"
    "\"tvdb\":{\"season\":7,\"episode\":36},\"ids\":{\"tvdb_id\":123456},"
    "\"watched_at\":\"2026-10-12T12:00:00Z\"}]}]}]}";
  nHistorico = 0;
  assert(simkl_historico_ler(onePieceExactTvdbHistory, "tt0388629", 1) == 1);
  assert(nHistorico == 1 && historicoPares[0].temporada == 22 &&
         historicoPares[0].episodio == 1123);
  historicoCatalogId[0] = 0;
  historicoCatalogTvdb = historicoCatalogTemporada = historicoCatalogEpisodio = 0;

  static const char onePieceShowHistory[] =
    "{\"movies\":[],\"shows\":[],\"anime\":[{\"show\":{\"ids\":{\"imdb\":\"tt0388629\"}},"
    "\"seasons\":[{\"number\":1,\"episodes\":["
    "{\"number\":1123,\"watched_at\":\"2026-10-12T12:00:00Z\"}]}]}]}";
  nHistorico = 0;
  assert(simkl_historico_ler(onePieceShowHistory, "tt0388629", 1) == 1);
  assert(nHistorico == 1 && !strcmp(historicoIds[0], "tt0388629"));
  assert(historicoPares[0].temporada == 22 && historicoPares[0].episodio == 1123);

  // Cinemeta restarts numbering every season (season 1 has only 8 episodes).
  // Simkl's season-1 absolute numbers at or below 100 must map too, without
  // the catalog episode list loaded yet.
  onePieceCachedMeta = onePieceProviderMeta = onePieceMeta;
  static const char onePieceCinemetaHistory[] =
    "{\"movies\":[],\"shows\":[],\"anime\":[{\"anime\":{\"ids\":{\"imdb\":\"tt0388629\"}},"
    "\"seasons\":[{\"number\":1,\"episodes\":["
    "{\"number\":1,\"watched_at\":\"2026-10-12T12:00:00Z\"},"
    "{\"number\":8,\"watched_at\":\"2026-10-12T12:00:00Z\"},"
    "{\"number\":9,\"watched_at\":\"2026-10-12T12:00:00Z\"},"
    "{\"number\":31,\"watched_at\":\"2026-10-12T12:00:00Z\"},"
    "{\"number\":100,\"tvdb_season\":2,\"tvdb_number\":39,"
    "\"watched_at\":\"2026-10-12T12:00:00Z\"},"
    "{\"number\":1122,\"watched_at\":\"2026-10-12T12:00:00Z\"}]}]}]}";
  nHistorico = 0;
  assert(simkl_historico_ler(onePieceCinemetaHistory, "tt0388629", 1) == 6);
  assert(historicoPares[0].temporada == 1 && historicoPares[0].episodio == 1);
  assert(historicoPares[1].temporada == 1 && historicoPares[1].episodio == 8);
  assert(historicoPares[2].temporada == 2 && historicoPares[2].episodio == 1);
  assert(historicoPares[3].temporada == 3 && historicoPares[3].episodio == 1);
  assert(historicoPares[4].temporada == 7 && historicoPares[4].episodio == 9);
  assert(historicoPares[5].temporada == 22 && historicoPares[5].episodio == 38);

  // Real data: the Simkl history of an account that watched 1..1122, against
  // the live Cinemeta and Nuvio catalog lists for One Piece.
  {
    char *hist = lerFixture("tests/fixtures/onepiece/simkl_history_tt0388629.json");
    char *cine = lerFixture("tests/fixtures/onepiece/cinemeta_tvdb_tt0388629.json");
    char *nuvio = lerFixture("tests/fixtures/onepiece/nuvio_tt0388629.json");
    historicoSoGrade = 1;

    // Cinemeta restarts numbering each season, has no 628 and adds an
    // extra S22E38 before 1123: only TVDB IDs line the two up.
    onePieceCachedMeta = onePieceProviderMeta = cine;
    memset(historicoGrade, 0, sizeof historicoGrade);
    nHistorico = 0;
    assert(simkl_historico_ler(hist, "tt0388629", 1) == 1121);
    assert(historicoGrade[1][8] && !historicoGrade[1][9]);
    assert(historicoGrade[22][37]);   // 1122 "The Last Lesson! Impact Inherited"
    assert(!historicoGrade[22][38]);  // the next one, not watched yet

    // Nuvio keeps absolute numbers: S22E1122 is Simkl's 1122.
    onePieceCachedMeta = onePieceProviderMeta = nuvio;
    memset(historicoGrade, 0, sizeof historicoGrade);
    nHistorico = 0;
    assert(simkl_historico_ler(hist, "tt0388629", 1) == 1122);
    assert(historicoGrade[1][61] && historicoGrade[22][1122]);
    assert(!historicoGrade[22][1123]);

    // Xperience with TheTVDB numbering: Cinemeta's episodes, no TVDB IDs.
    // Cinemeta's IDs are borrowed, so 628's absence shifts nothing.
    {
      const char *v;
      int q = 0;
      memset(historicoCatalogEps, 0, sizeof historicoCatalogEps);
      for (v = js_array(cine, NULL, "videos"); v; v = js_prox(js_fim(v))) {
        const char *vf = js_fim(v);
        int t = (int)js_num(v, vf, "season", 0), e = (int)js_num(v, vf, "episode", 0);
        if (t <= 0 || e <= 0) continue;
        assert(q < 1300);
        historicoCatalogEps[q].temporada = (short)t;
        historicoCatalogEps[q].episodio = (short)e;
        q++;
      }
      snprintf(historicoCatalogId, sizeof historicoCatalogId, "%s", "tt0388629");
      historicoCatalogEpsQtd = q;
      historicoCatalogEpsFinal = 1;
      onePieceCinemeta = cine;
      onePieceCinemetaFetches = 0;
      memset(historicoGrade, 0, sizeof historicoGrade);
      nHistorico = 0;
      assert(simkl_historico_ler(hist, "tt0388629", 1) == 1121);
      assert(onePieceCinemetaFetches == 1);
      assert(historicoGrade[1][8] && historicoGrade[2][1]);
      assert(historicoGrade[22][37] && !historicoGrade[22][38]);
      onePieceCinemeta = NULL;
      historicoCatalogId[0] = 0;
      historicoCatalogEpsQtd = historicoCatalogEpsFinal = 0;
      memset(historicoCatalogEps, 0, sizeof historicoCatalogEps);
    }

    historicoSoGrade = 0;
    nHistorico = 0;
    free(hist);
    free(cine);
    free(nuvio);
  }

  // The list on screen switches numbering after the import (Nuvio's absolute
  // list, then an addon's per-season one): the kept Simkl entry is mapped
  // again onto the new list, matched by TVDB ID.
  {
    static const char trocaHistorico[] =
      "{\"movies\":[],\"shows\":[],\"anime\":[{\"show\":{\"ids\":{\"imdb\":\"tt0388629\"}},"
      "\"seasons\":[{\"number\":1,\"episodes\":["
      "{\"number\":1,\"watched_at\":\"2026-10-12T12:00:00Z\",\"ids\":{\"tvdb_id\":101}},"
      "{\"number\":2,\"watched_at\":\"2026-10-12T12:00:00Z\",\"ids\":{\"tvdb_id\":102}},"
      "{\"number\":3,\"watched_at\":\"2026-10-12T12:00:00Z\",\"ids\":{\"tvdb_id\":103}}]}]}]}";
    onePieceCachedMeta = onePieceProviderMeta =
      "{\"meta\":{\"videos\":[{\"season\":1,\"episode\":1},{\"season\":1,\"episode\":2},"
      "{\"season\":2,\"episode\":3}]}}";
    historicoSoGrade = 1;
    memset(historicoGrade, 0, sizeof historicoGrade);
    nHistorico = 0;
    assert(simkl_historico_ler(trocaHistorico, "tt0388629", 1) == 3);
    assert(historicoGrade[1][1] && historicoGrade[1][2] && historicoGrade[2][3]);

    snprintf(historicoCatalogId, sizeof historicoCatalogId, "%s", "tt0388629");
    historicoCatalogEpsQtd = 4;
    historicoCatalogEpsFinal = 1;
    memset(historicoCatalogEps, 0, sizeof historicoCatalogEps);
    historicoCatalogEps[0].temporada = 2; historicoCatalogEps[0].episodio = 1;
    historicoCatalogEps[0].tvdbEp = 103;
    historicoCatalogEps[1].temporada = 1; historicoCatalogEps[1].episodio = 1;
    historicoCatalogEps[1].tvdbEp = 101;
    historicoCatalogEps[2].temporada = 1; historicoCatalogEps[2].episodio = 2;
    historicoCatalogEps[2].tvdbEp = 102;
    historicoCatalogEps[3].temporada = 2; historicoCatalogEps[3].episodio = 2;
    historicoCatalogEps[3].tvdbEp = 104;
    memset(historicoGrade, 0, sizeof historicoGrade);
    nHistorico = 0;
    simkl_anime_lista_mudou("tt0388629:1:1");
    for (int espera = 0; espera < 200 && nHistorico < 3; espera++) usleep(10000);
    assert(nHistorico == 3);
    assert(historicoGrade[1][1] && historicoGrade[1][2] && historicoGrade[2][1]);
    // Continue watching asks for the next-up in this list's numbering.
    {
      int nt = 0, ne = 0;
      for (int espera = 0; espera < 200 && !simkl_anime_proximo("tt0388629", &nt, &ne);
           espera++) usleep(10000);
      assert(nt == 2 && ne == 2);
    }
    // The same list published again does not map it a third time.
    nHistorico = 0;
    simkl_anime_lista_mudou("tt0388629");
    usleep(100000);
    assert(nHistorico == 0);

    // Another list: the stored next-up does not apply to it.
    historicoCatalogEps[3].episodio = 3;
    { int nt, ne; assert(!simkl_anime_proximo("tt0388629", &nt, &ne)); }

    historicoSoGrade = 0;
    historicoCatalogId[0] = 0;
    historicoCatalogEpsQtd = historicoCatalogEpsFinal = 0;
    memset(historicoCatalogEps, 0, sizeof historicoCatalogEps);
  }

  free(nuvioMeta);
  free(onePieceMeta);
  onePieceProviderMeta = onePieceCachedMeta = NULL;

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
  simkl_esquecer();
  tokenAtual = TOKEN;
  PLAYBACK = "[]";
  WATCHING = "{\"shows\":[]}";
  snprintf(atividades, sizeof atividades, "{\"all\":\"2026-10-30T22:00:00Z\"}");
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
  // Next season of a finished anime: Edgerunners completed, Edgerunners II in
  // plan to watch with nothing watched and no IMDb ID in the list.
  simkl_esquecer();
  PLAYBACK = "[]";
  WATCHING = "{\"shows\":[]}";
  WATCHING_ANIME = "{\"anime\":[]}";
  FEITO_ANIME = "{\"anime\":[{\"status\":\"completed\",\"watched_episodes_count\":10,"
    "\"last_watched_at\":\"2022-09-13T07:00:00Z\",\"show\":{\"title\":\"Cyberpunk: Edgerunners\","
    "\"ids\":{\"simkl\":1377443,\"imdb\":\"tt12590266\",\"tvdb\":\"384541\"}}}]}";
  PTW_ANIME = "{\"anime\":["
    "{\"status\":\"plantowatch\",\"watched_episodes_count\":0,\"next_to_watch\":\"E1\","
    "\"show\":{\"title\":\"Cyberpunk: Edgerunners II\",\"ids\":{\"simkl\":2830474}}},"
    "{\"status\":\"plantowatch\",\"watched_episodes_count\":0,\"next_to_watch\":\"E1\","
    "\"show\":{\"title\":\"Not aired yet\",\"ids\":{\"simkl\":2830475}}},"
    "{\"status\":\"plantowatch\",\"watched_episodes_count\":0,\"next_to_watch\":\"E1\","
    "\"show\":{\"title\":\"Never watched\",\"ids\":{\"simkl\":5,\"imdb\":\"tt7654321\"}}}]}";
  snprintf(atividades, sizeof atividades, "{\"all\":\"2026-11-01T10:00:00Z\"}");
  detalhesAnime = 0;
  n = simkl_continuar(v, 16);
  assert(n == 1 && !strcmp(v[0].imdb, "tt12590266:2:1"));
  assert(v[0].temporada == 2 && v[0].episodio == 1 && v[0].progresso == 0);
  assert(!strcmp(v[0].tipo, "series") && simkl_e_a_seguir(v[0].imdb));
  // Details for both, episodes for the aired one; never-watched: nothing.
  assert(detalhesAnime == 3);
  assert(!strcmp(v[0].nomeEpisodio, "Night City Again"));
  assert(!strcmp(v[0].thumbEpisodio,
                 "https://simkl.in/episodes/17/17570079032e93ce31_w.jpg"));
  // Watching season 2 already: the watching entry wins, no duplicate.
  simkl_esquecer();
  WATCHING_ANIME = "{\"anime\":[{\"next_to_watch\":\"S02E04\","
    "\"anime\":{\"title\":\"Cyberpunk: Edgerunners\",\"ids\":{\"imdb\":\"tt12590266\"}}}]}";
  snprintf(atividades, sizeof atividades, "{\"all\":\"2026-11-01T11:00:00Z\"}");
  n = simkl_continuar(v, 16);
  assert(n == 1 && !strncmp(v[0].imdb, "tt12590266:2:4", 14));
  FEITO_ANIME = "{\"anime\":[]}";
  PTW_ANIME = "{\"anime\":[]}";
  WATCHING_ANIME = "{\"anime\":[]}";

  // A show filed as plan to watch after two seasons (Simkl, Monster, 01/11/2026):
  // one entry for every season, next_to_watch S03E01. Untouched plan-to-watch
  // shows stay out.
  {
    const char *ptwAntes = PTW_SERIES;
    simkl_esquecer();
    PTW_SERIES = "{\"shows\":["
      "{\"status\":\"plantowatch\",\"last_watched\":\"S02E09\",\"next_to_watch\":\"S03E01\","
      "\"watched_episodes_count\":19,\"last_watched_at\":\"2026-09-24T13:59:07Z\","
      "\"show\":{\"title\":\"Monster\",\"ids\":{\"simkl\":1446614,\"imdb\":\"tt13207736\"}}},"
      "{\"status\":\"plantowatch\",\"next_to_watch\":\"S01E01\",\"watched_episodes_count\":0,"
      "\"show\":{\"title\":\"Untouched\",\"ids\":{\"imdb\":\"tt1234567\"}}}]}";
    snprintf(atividades, sizeof atividades, "{\"all\":\"2026-11-01T12:00:00Z\"}");
    n = simkl_continuar(v, 16);
    assert(n == 1 && !strcmp(v[0].imdb, "tt13207736:3:1"));
    assert(v[0].temporada == 3 && v[0].episodio == 1 && simkl_e_a_seguir(v[0].imdb));
    PTW_SERIES = ptwAntes;
  }

  proximoTeste = 0;
  assert(simkl_continuar(v, 16) == 0);
  puts("PASS: absolute anime provider mapping, title fallback, dedup and next-up setting");
  return 0;
}
