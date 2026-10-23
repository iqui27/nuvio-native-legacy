// #524: keep a known series identity when opening a title outside the catalog.
// The two reduced Cinemeta responses reproduce the actual service collision:
// series/tt9054364 is Slime; movie/tt9054364 is Chasing Mavericks. Both echo
// the requested IMDb id and have moviedb_id 82684, so that number alone does
// not prove the media type. No live service, account, or test-only app hook.
#define main detalheanime_existing_main
#include "detalheanime.c"
#undef main

static const char *SLIME_SERIES =
  "{\"meta\":{\"id\":\"tt9054364\",\"imdb_id\":\"tt9054364\",\"type\":\"series\","
  "\"name\":\"That Time I Got Reincarnated as a Slime\",\"moviedb_id\":82684,"
  "\"poster\":\"https://fixture.invalid/slime.jpg\",\"description\":\"Series synopsis\","
  "\"videos\":[{\"id\":\"tt9054364:1:1\",\"season\":1,\"episode\":1,\"name\":\"Storm Dragon\"},"
  "{\"id\":\"tt9054364:1:2\",\"season\":1,\"episode\":2,\"name\":\"Meeting the Goblins\"}]}}";
static const char *CHASING_WRONG_ID =
  "{\"meta\":{\"id\":\"tt9054364\",\"imdb_id\":\"tt9054364\",\"type\":\"movie\","
  "\"name\":\"Chasing Mavericks\",\"moviedb_id\":82684,\"releaseInfo\":\"2012\","
  "\"poster\":\"https://fixture.invalid/chasing.jpg\",\"description\":\"Movie synopsis\",\"videos\":[]}}";
static const char *CHASING_MOVIE =
  "{\"meta\":{\"id\":\"tt1629757\",\"imdb_id\":\"tt1629757\",\"type\":\"movie\","
  "\"name\":\"Chasing Mavericks\",\"moviedb_id\":82684,\"releaseInfo\":\"2012\","
  "\"poster\":\"https://fixture.invalid/chasing.jpg\",\"description\":\"Movie synopsis\",\"videos\":[]}}";

static void setup(void) {
  CatItem seed = {0};
  fakePerfil = 1; fakePerfilSessao = 1;
  limparCacheMeta();
  metaprov_zerar_pausa();
  // The normal app already has a catalog. cat_acrescentar intentionally
  // rejects an empty/uninitialized catalog, so keep an unrelated title here.
  snprintf(seed.imdb, sizeof seed.imdb, "tt0111161");
  snprintf(seed.tipo, sizeof seed.tipo, "movie");
  snprintf(seed.titulo, sizeof seed.titulo, "Unrelated catalog title");
  cat_definir_tudo(&seed, 1, NULL, 0);
  addonMeta = nFake = nPedidos = nRotas = 0;
  fakeMetaExterno = fakeTmdbBasico = fakeTmdbArte = fakeSoCinemeta = 0;
  fakeIdioma = "en-US";
  desc_tmdb_definir("00000000000000000000000000000524");
  sobIndice = -1; sobTmdb = 0; sobFioVivo = sobTemSemente = 0;
  // A first synchronous request has no earlier worker/session to borrow.
  sobSessao = 0;
  sobId[0] = sobTipo[0] = 0;
  rota("/tv/82684/external_ids?", "{\"imdb_id\":\"tt9054364\"}");
  rota("/movie/82684/external_ids?", "{\"imdb_id\":\"tt1629757\"}");
  rota("catalog.nuvio.tv", "{\"meta\":{}}");
  rota("/meta/series/tt9054364.json", SLIME_SERIES);
  rota("/meta/movie/tt9054364.json", CHASING_WRONG_ID);
  rota("/meta/movie/tt1629757.json", CHASING_MOVIE);
}

static int awaitTitle(const char *id, const char *type, const char *title) {
  for (int i = 0; i < 5000 && desc_titulo_buscando(); i++) SDL_Delay(1);
  int ix = desc_titulo_pronto();
  const CatItem *ci = ix >= 0 ? cat_item(ix) : NULL;
  printf("result id=%s type=%s title=%s index=%d requests=%d\n",
         ci ? ci->imdb : "(missing)", ci ? ci->tipo : "(missing)",
         ci ? ci->titulo : "(missing)", ix, nPedidos);
  for (int i = 0; i < nPedidos && i < 64; i++) printf("request %s\n", pedidos[i]);
  return ci && !strcmp(ci->imdb, id) && !strcmp(ci->tipo, type) && !strcmp(ci->titulo, title);
}

static void cached(const char *id, const char *type, const char *title) {
  CatItem c = {0};
  snprintf(c.imdb, sizeof c.imdb, "%s", id);
  snprintf(c.tipo, sizeof c.tipo, "%s", type);
  snprintf(c.titulo, sizeof c.titulo, "%s", title);
  cat_definir_tudo(&c, 1, NULL, 0);
}

static void seriesResponse(const char *body) {
  for (int i = 0; i < nRotas; i++)
    if (!strcmp(rotas[i].trecho, "/meta/series/tt9054364.json")) { rotas[i].resp = body; return; }
  assert(0 && "series fixture route missing");
}

static int awaitMissing(void) {
  for (int i = 0; i < 5000 && desc_titulo_buscando(); i++) SDL_Delay(1);
  int ix = desc_titulo_pronto();
  printf("rejected index=%d catalog-count=%d movie-requested=%d\n",
         ix, cat_n(), pediu("/meta/movie/tt9054364.json"));
  return ix < 0 && cat_n() == 1 && !pediu("/meta/movie/tt9054364.json");
}

int main(int argc, char **argv) {
  const char *mode = argc > 1 ? argv[1] : "";
  setup();
  if (!strcmp(mode, "tmdb-tv")) {
    // A TV credit already tells us the media type. Resolving its external IMDb
    // id must not switch to the numerically colliding TMDB movie.
    desc_pedir_titulo_tmdb(82684, "tv");
    return !awaitTitle("tt9054364", "series", "That Time I Got Reincarnated as a Slime");
  }
  if (!strcmp(mode, "episode-id")) {
    // Activity/Continue Watching keys explicitly carry episode coordinates.
    // Looking up the title must retain that evidence after removing the suffix.
    desc_pedir_titulo("tt9054364:1:2");
    return !awaitTitle("tt9054364", "series", "That Time I Got Reincarnated as a Slime");
  }
  if (!strcmp(mode, "movie-control")) {
    // Movie 82684 and TV 82684 are different titles. A correct fix must keep
    // the movie route, not globally coerce anime-shaped/numeric ids to series.
    desc_pedir_titulo_tmdb(82684, "movie");
    return !awaitTitle("tt1629757", "movie", "Chasing Mavericks");
  }
  if (!strcmp(mode, "known-series-control")) {
    catalogoDe("tt9054364", "series", "That Time I Got Reincarnated as a Slime", "");
    abrir();
    const CatItem *ci = cat_item(0);
    printf("series detail title=%s type=%s episodes=%d movie-requested=%d\n",
           ci->titulo, ci->tipo, cat_n_episodios(0), pediu("/meta/movie/tt9054364.json"));
    return strcmp(ci->tipo, "series") || strcmp(ci->titulo, "That Time I Got Reincarnated as a Slime") ||
           cat_n_episodios(0) != 2 || pediu("/meta/movie/tt9054364.json");
  }
  if (!strcmp(mode, "uncertain-detail-control")) {
    catalogoDe("tt9054364", "anime", "That Time I Got Reincarnated as a Slime", "");
    abrir();
    const CatItem *ci = cat_item(0);
    printf("uncertain detail title=%s type=%s episodes=%d movie-requested=%d\n",
           ci->titulo, ci->tipo, cat_n_episodios(0), pediu("/meta/movie/tt9054364.json"));
    return strcmp(ci->tipo, "series") || strcmp(ci->titulo, "That Time I Got Reincarnated as a Slime") ||
           cat_n_episodios(0) != 2 || pediu("/meta/movie/tt9054364.json");
  }
  if (!strcmp(mode, "tmdb-series-alias") || !strcmp(mode, "tmdb-show-alias")) {
    desc_pedir_titulo_tmdb(82684, !strcmp(mode, "tmdb-show-alias") ? "show" : "series");
    return !awaitTitle("tt9054364", "series", "That Time I Got Reincarnated as a Slime") ||
           pediu("/movie/82684/external_ids?") || pediu("/meta/movie/tt9054364.json");
  }
  if (!strcmp(mode, "cached-opposite-movie") || !strcmp(mode, "cached-uncertain") ||
      !strcmp(mode, "cached-channel")) {
    const char *oldType = !strcmp(mode, "cached-uncertain") ? "anime" :
                          !strcmp(mode, "cached-channel") ? "tv" : "movie";
    cached("tt9054364", oldType, "Cached other type");
    desc_pedir_titulo_tmdb(82684, "tv");
    int ok = awaitTitle("tt9054364", "series", "That Time I Got Reincarnated as a Slime");
    return !ok || strcmp(cat_item(0)->tipo, oldType) || strcmp(cat_item(0)->titulo, "Cached other type");
  }
  if (!strcmp(mode, "cached-opposite-series")) {
    cached("tt1629757", "series", "Cached wrong series");
    desc_pedir_titulo_tmdb(82684, "movie");
    return !awaitTitle("tt1629757", "movie", "Chasing Mavericks") ||
           strcmp(cat_item(0)->tipo, "series") || strcmp(cat_item(0)->titulo, "Cached wrong series");
  }
  if (!strcmp(mode, "cached-show-control")) {
    cached("tt9054364", "show", "Cached show alias");
    desc_pedir_titulo_tmdb(82684, "tv");
    return !awaitTitle("tt9054364", "show", "Cached show alias") || pediu("/meta/");
  }
  if (!strcmp(mode, "seed-opposite-movie") || !strcmp(mode, "seed-opposite-series")) {
    int movie = !strcmp(mode, "seed-opposite-series");
    const char *id = movie ? "tt1629757" : "tt9054364";
    const char *oldType = movie ? "series" : "movie";
    const char *type = movie ? "movie" : "series";
    cached(id, oldType, "Cached opposite type");
    desc_pedir_titulo_semente(id, 0, type, "Seed title", "2024", "https://fixture.invalid/seed.jpg");
    assert(sobSessao == perfis_sessao_geracao());
    return !awaitTitle(id, type, "Seed title") || nPedidos || cat_n() != 2 ||
           strcmp(cat_item(0)->tipo, oldType) || strcmp(cat_item(0)->titulo, "Cached opposite type");
  }
  if (!strcmp(mode, "seed-matching-show-control")) {
    cached("tt9054364", "show", "Cached show alias");
    desc_pedir_titulo_semente("tt9054364", 0, "series", "Unused seed", "2024", "https://fixture.invalid/seed.jpg");
    assert(sobSessao == perfis_sessao_geracao());
    return !awaitTitle("tt9054364", "show", "Cached show alias") || nPedidos || cat_n() != 1;
  }
  if (!strcmp(mode, "seed-current-session") || !strcmp(mode, "seed-stale-session") ||
      !strcmp(mode, "seed-cached-stale-session")) {
    int cachedSeed = !strcmp(mode, "seed-cached-stale-session");
    if (cachedSeed) cached("tt9054364", "show", "Cached show alias");
    // The first seeded result belongs to this visit even without a worker.
    // Returning to the same profile after switching does not revive it.
    desc_pedir_titulo_semente("tt9054364", 0, "series", "Seed title", "2024",
                               "https://fixture.invalid/seed.jpg");
    assert(sobSessao == perfis_sessao_geracao() && sobSessao == 1);
    assert(!desc_titulo_buscando() && cat_n() == (cachedSeed ? 1 : 2) && nPedidos == 0);
    if (!strcmp(mode, "seed-stale-session") || cachedSeed) {
      fakePerfil = 2; fakePerfilSessao++;
      fakePerfil = 1; fakePerfilSessao++;
      int ix = desc_titulo_pronto();
      printf("stale seed index=%d request-session=%llu current-session=%llu profile=%d\n",
             ix, (unsigned long long)sobSessao, (unsigned long long)fakePerfilSessao, fakePerfil);
      return ix >= 0 || nPedidos != 0;
    }
    return !awaitTitle("tt9054364", "series", "Seed title") || nPedidos != 0;
  }
  if (!strcmp(mode, "native-tmdb-canonical-alias") ||
      !strcmp(mode, "native-tmdb-wrong-id") || !strcmp(mode, "native-tmdb-wrong-type")) {
    // Reduced Bigg Boss response: Nuvio accepts the native TMDB request but
    // publishes its canonical IMDb id. No moviedb_id/tmdb_id proof is present.
    const char *canonical =
      "{\"meta\":{\"id\":\"tt1281973\",\"type\":\"series\",\"name\":\"Bigg Boss\","
      "\"poster\":\"https://fixture.invalid/bigg-boss.jpg\",\"videos\":[{"
      "\"id\":\"tt1281973:1:1\",\"season\":1,\"episode\":1,\"name\":\"Episode 1\"}]}}";
    const char *wrongId =
      "{\"meta\":{\"id\":\"tmdb:99999\",\"type\":\"series\",\"name\":\"Other title\","
      "\"poster\":\"https://fixture.invalid/other.jpg\"}}";
    const char *wrongType =
      "{\"meta\":{\"id\":\"tt1281973\",\"type\":\"movie\",\"name\":\"Other type\","
      "\"poster\":\"https://fixture.invalid/other.jpg\"}}";
    nRotas = nPedidos = 0;
    rota("/tv/11436/external_ids?", "{\"imdb_id\":null}");
    rota("/meta/series/tmdb:11436.json", !strcmp(mode, "native-tmdb-wrong-id") ? wrongId :
         !strcmp(mode, "native-tmdb-wrong-type") ? wrongType : canonical);
    desc_pedir_titulo_tmdb(11436, "tv");
    int ok = !strcmp(mode, "native-tmdb-canonical-alias") ?
      awaitTitle("tt1281973", "series", "Bigg Boss") : awaitMissing();
    return !ok || !pediu("/tv/11436/external_ids?") ||
           !pediu("/meta/series/tmdb:11436.json") || pediu("/meta/movie/");
  }
  if (!strcmp(mode, "opposite-response-type")) {
    seriesResponse(CHASING_WRONG_ID);
    desc_pedir_titulo_tmdb(82684, "tv");
    return !awaitMissing();
  }
  if (!strcmp(mode, "different-response-id")) {
    seriesResponse("{\"meta\":{\"id\":\"tt1629757\",\"type\":\"series\",\"name\":\"Different title\","
                   "\"poster\":\"https://fixture.invalid/other.jpg\"}}");
    desc_pedir_titulo_tmdb(82684, "tv");
    return !awaitMissing();
  }
  if (!strcmp(mode, "missing-series-no-movie")) {
    seriesResponse(NULL);
    desc_pedir_titulo_tmdb(82684, "tv");
    return !awaitMissing();
  }
  if (!strcmp(mode, "context-reset")) {
    desc_pedir_titulo_tmdb(82684, "tv");
    if (!awaitTitle("tt9054364", "series", "That Time I Got Reincarnated as a Slime")) return 1;
    cached("tt0111161", "movie", "Unrelated title"); nPedidos = 0;
    desc_pedir_titulo("tt1629757");
    return !awaitTitle("tt1629757", "movie", "Chasing Mavericks");
  }
  if (!strcmp(mode, "malformed-episode-control")) {
    const char *ids[] = {"tt1629757:1:0", "tt1629757:1:2junk", "tt1629757:-1:2", "tt1629757::2"};
    for (unsigned i = 0; i < sizeof ids / sizeof *ids; i++) {
      setup();
      desc_pedir_titulo(ids[i]);
      if (!awaitTitle("tt1629757", "movie", "Chasing Mavericks") || pediu("/meta/series/tt1629757.json")) return 1;
    }
    return 0;
  }
  fprintf(stderr, "unknown scenario: %s\n", mode);
  return 2;
}
