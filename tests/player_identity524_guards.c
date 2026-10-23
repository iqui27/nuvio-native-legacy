// #524: requested episodes survive a late confirmed type without borrowing
// another title/profile or replacing the frozen title's visible metadata.
#define main player_retido_existing_main
#include "player_retido.c"
#undef main
#include "dados.h"
#include "progresso.h"
#include "traktscrobble.h"

static CatItem observed;
static int observations;
static int trackerCalls, parentalRequests, refreshRequests;
static int metadataFallbacks;
static char parentalId[64];
static char trackerId[64];
void nv_identity_desc_garantir(int ix) { (void)ix; }
void nv_identity_desc_episodios(int ix, int season) {
  (void)ix; (void)season; metadataFallbacks++;
  assert(!"guard fixture unexpectedly requested metadata");
}
void nv_identity_intro_pedir(const char *id, long tmdb, int t, int e, double a, double p) {
  (void)id; (void)tmdb; (void)t; (void)e; (void)a; (void)p;
}
void nv_identity_intro_mapeado(const char *id, long tmdb, int t, int e, int it, int ie, double a, double p) {
  (void)it; (void)ie; nv_identity_intro_pedir(id, tmdb, t, e, a, p);
}
void nv_identity_trakt_marcar(const char *id, double p, double d, double a, int fim) {
  (void)p; (void)d; (void)a; (void)fim; trackerCalls++;
  snprintf(trackerId, sizeof trackerId, "%s", id ? id : "");
}
void nv_identity_parental_pedir(const char *id) {
  parentalRequests++; snprintf(parentalId, sizeof parentalId, "%s", id ? id : "");
}
void nv_identity_refazer_continuar(void) { refreshRequests++; }
void nv_identity_recomenda_passo(const CatItem *ci, int playing) {
  if (ci && playing) { observed = *ci; observations++; }
}

static void seedEpisode(const char *id) {
  for (int i = 0; i < cat_n(); i++) {
    const CatItem *current = cat_item(i);
    if (current && !strcmp(current->imdb, id)) {
      CatEp episode = {0}; episode.temporada = 1; episode.episodio = 2;
      snprintf(episode.vid, sizeof episode.vid, "%s:1:2", id);
      snprintf(episode.duracao, sizeof episode.duracao, "5 min");
      cat_definir_episodios(i, &episode, 1);
    }
  }
}

int main(int argc, char **argv) {
  const char *mode = argc > 1 ? argv[1] : "";
  int movie = !strcmp(mode, "known-movie-conflict");
  int noPair = !strcmp(mode, "series-without-request");
  int bare = !strcmp(mode, "bare-source-series");
  int fresh = !strcmp(mode, "fresh-episode-source");
  int promote = !strcmp(mode, "pending-before-meta") || !strcmp(mode, "reordered-series") || fresh;
  // An uncertain title keeps a requested episode. A confirmed movie and a
  // source opened on the bare id still drop it.
  int keepEpisode = !movie && !noPair && !bare;
  int expectedT = (promote || keepEpisode) ? 1 : 0, expectedE = (promote || keepEpisode) ? 2 : 0;
  CatItem c = {0}, published[3] = {{0}};
  int t, e, failed = 0;
  setvbuf(stdout, NULL, _IONBF, 0);
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  dados_iniciar(""); perfis_definir_ativo(1);
  snprintf(c.imdb, sizeof c.imdb, "%s", movie ? "tt1629757" : "tt9054364");
  snprintf(c.tipo, sizeof c.tipo, "%s", movie ? "movie" : "anime");
  snprintf(c.titulo, sizeof c.titulo, "Frozen original title");
  snprintf(c.poster, sizeof c.poster, "https://fixture.invalid/frozen.jpg");
  snprintf(c.meta, sizeof c.meta, "5 min");
  cat_definir(&c, 1);
  seedEpisode(c.imdb);
  assert(!strcmp(cat_item(0)->tipo, c.tipo)); // Episodes do not invent a media type.
  if (bare || fresh) {
    Stream source = {0};
    snprintf(source.url, sizeof source.url, "https://fixture.invalid/requested-episode.mp4");
    stream_definir_alvo(c.imdb); stream_definir_lista(&source, 1); stream_definir_atual(0);
  }
  duracao = 300; posicao = 120;
  player_abrir(0, "https://fixture.invalid/requested-episode.mp4");
  // The request exists before metadata. Do not call the setter again after
  // publication: a real frame must consume the newly confirmed classification.
  if (!noPair) player_definir_episodio(1, 2);
  published[0] = c;
  snprintf(published[0].tipo, sizeof published[0].tipo, "series");
  snprintf(published[0].titulo, sizeof published[0].titulo, "Late metadata title");
  snprintf(published[0].poster, sizeof published[0].poster, "https://fixture.invalid/late.jpg");
  if (!strcmp(mode, "still-unknown")) { /* no classification was published */ }
  else if (!strcmp(mode, "other-title")) {
    snprintf(published[0].imdb, sizeof published[0].imdb, "tt0111161");
    cat_definir(published, 1);
  } else if (!strcmp(mode, "profile-change") || !strcmp(mode, "profile-aba")) {
    perfis_definir_ativo(2);
    if (!strcmp(mode, "profile-aba")) perfis_definir_ativo(1);
    cat_definir(published, 1);
  } else if (!strcmp(mode, "conflicting-copies")) {
    published[1] = published[0];
    snprintf(published[1].tipo, sizeof published[1].tipo, "movie");
    cat_definir(published, 2);
  } else if (!strcmp(mode, "reordered-series")) {
    published[1] = published[0];
    snprintf(published[0].imdb, sizeof published[0].imdb, "tt0111161");
    cat_definir(published, 2);
  } else if (!strcmp(mode, "colliding-movie-only")) {
    snprintf(published[0].tipo, sizeof published[0].tipo, "movie");
    snprintf(published[0].titulo, sizeof published[0].titulo, "Chasing Mavericks");
    cat_definir(published, 1);
  } else if (promote || movie || noPair || bare) cat_definir(published, 1);
  else { fprintf(stderr, "unknown scenario %s\n", mode); return 2; }
  // A confirmed source has its own episode metadata. Without it the real
  // player's normal missing-list fallback would fetch live catalog metadata
  // and replace this five-minute fixture's runtime with an unrelated value.
  seedEpisode(c.imdb);
  SDL_Delay(10); posicao += .01;
  player_atualizar(.01f, SDL_GetTicks());
  if (fresh) {
    Stream source = {0};
    player_episodio_atual(&t, &e); assert(t == 0 && e == 0);
    snprintf(source.url, sizeof source.url, "https://fixture.invalid/new-episode.mp4");
    stream_definir_alvo("tt9054364:1:2"); stream_definir_lista(&source, 1); stream_definir_atual(0);
    player_definir_fonte(source.url);
  }
  // Use the actual watched-proof clock before checking close persistence.
  for (int i = 0; i < 24; i++) {
    SDL_Delay(100); posicao += .1; player_atualizar(.1f, SDL_GetTicks());
  }
  player_episodio_atual(&t, &e);
  printf("guard %s actual=%d:%d expected=%d:%d observations=%d type=%s title=%s poster=%s\n",
         mode, t, e, expectedT, expectedE, observations, observed.tipo, observed.titulo, observed.poster);
  if (t != expectedT || e != expectedE) failed = 1;
  // Unknown sessions may legitimately suppress outbound tracking altogether.
  // If tracking observes a session, it must still belong to the opening title.
  if (observations && (strcmp(observed.imdb, c.imdb) || strcmp(observed.titulo, c.titulo) ||
                       strcmp(observed.poster, c.poster))) failed = 1;
  if (promote && (!observations || strcmp(observed.tipo, "series"))) failed = 1;
  if (movie && observations && strcmp(observed.tipo, "movie")) failed = 1;
  if (!promote && !movie && observations) failed = 1;
  player_encerrar();
  ProgRegistro progress;
  int bareSaved = prog_por_chave(c.imdb, &progress);
  if (!movie && bareSaved) failed = 1;
  if (movie && bareSaved && strcmp(progress.tipo, "movie")) failed = 1;
  int episodeSaved = prog_por_chave("tt9054364_s1e2", &progress);
  if (promote ? (!episodeSaved || strcmp(progress.tipo, "series")) : episodeSaved) failed = 1;
  printf("guard boundaries parental=%d id=%s refresh=%d tracker=%d bare-progress=%d episode-progress=%d\n",
         parentalRequests, parentalId, refreshRequests, trackerCalls, bareSaved, episodeSaved);
  if (parentalRequests != 1 || strcmp(parentalId, c.imdb)) failed = 1;
  if (!promote && !movie && (trackerCalls || refreshRequests)) failed = 1;
  if (promote && (trackerCalls != 1 || refreshRequests != 1)) failed = 1;
  if (movie && (bareSaved || cat_item(0)->progresso || trackerCalls != 1 ||
                refreshRequests != 1 || strcmp(trackerId, "tt1629757"))) failed = 1;
  printf("guard metadata-fallbacks=%d\n", metadataFallbacks);
  if (metadataFallbacks) failed = 1;
  SDL_Quit();
  return failed;
}
