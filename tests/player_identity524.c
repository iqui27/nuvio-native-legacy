// #524: a late, same-title metadata response must not leave playback using
// its earlier uncertain media type. Reuse only the real player's fake video
// boundary; catalog, player, progress persistence and scrobble JSON are real.
#define main player_retido_existing_main
#include "player_retido.c"
#undef main
#include "dados.h"
#include "progresso.h"
#include "traktscrobble.h"

static char statsId[64];
static char parentalId[64];
static int parentalRequests, refreshRequests;
void nv_identity_desc_garantir(int ix) { (void)ix; }
void nv_identity_intro_pedir(const char *id, long tmdb, int t, int e, double a, double p) {
  (void)id; (void)tmdb; (void)t; (void)e; (void)a; (void)p;
}
void nv_identity_intro_mapeado(const char *id, long tmdb, int t, int e, int it, int ie, double a, double p) {
  (void)it; (void)ie; nv_identity_intro_pedir(id, tmdb, t, e, a, p);
}
void nv_identity_trakt_marcar(const char *id, double p, double d, double a, int fim) {
  (void)p; (void)d; (void)a; (void)fim;
  snprintf(statsId, sizeof statsId, "%s", id);
}
// Neither parental HTTP nor the detached Home refresh participates in the
// identity being tested. Capture their real requests without starting workers
// that could outlive the fixture's normal SDL_Quit teardown.
void nv_identity_parental_pedir(const char *id) {
  parentalRequests++;
  snprintf(parentalId, sizeof parentalId, "%s", id ? id : "");
}
void nv_identity_refazer_continuar(void) { refreshRequests++; }

int main(int argc, char **argv) {
  CatItem c = {0};
  CatEp ep = {0};
  ProgRegistro r;
  int t, e, failed = 0;
  const char *mode = argc > 1 ? argv[1] : "";
  int lateMovie = !strcmp(mode, "late-movie-control");
  int lateClose = !strcmp(mode, "late-close-control");
  int late = !strcmp(mode, "late-series") || lateMovie || lateClose;
  int movie = !strcmp(mode, "movie-control") || lateMovie;
  int special = !strcmp(mode, "known-series-special");
  int defaultSeries = !strcmp(mode, "known-series-default");
  int next = !strcmp(mode, "next-opening-control");
  int retained = !strcmp(mode, "retained-series-control");
  int expectedT = special ? 0 : 1;
  int expectedE = defaultSeries ? 1 : next ? 3 : 2;
  setvbuf(stdout, NULL, _IONBF, 0);
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  dados_iniciar(""); perfis_definir_ativo(1);
  if (retained) {
    char path[600]; const char *dir = getenv("NUVIO_DADOS"); FILE *f;
    assert(dir && *dir);
    snprintf(path, sizeof path, "%s/ajustes.txt", dir);
    f = fopen(path, "w"); assert(f); fputs("manterVideoLocal 0\n", f); fclose(f);
    ajustes_dir(dir); assert(ajustes_manter_video());
  }
  snprintf(c.imdb, sizeof c.imdb, "%s", movie ? "tt1629757" : "tt9054364");
  snprintf(c.titulo, sizeof c.titulo, "%s", movie ? "Chasing Mavericks" : "That Time I Got Reincarnated as a Slime");
  snprintf(c.tipo, sizeof c.tipo, "%s", late ? "anime" : movie ? "movie" : "series");
  snprintf(c.meta, sizeof c.meta, "5 min");
  cat_definir(&c, 1);
  duracao = 300; posicao = 120;
  player_abrir(0, "https://fixture.invalid/native-video.mp4");
  if (late && !lateClose) {
    // The metadata worker resolves the original uncertain item, with the same
    // title id, while the player is already open. It does not switch titles.
    snprintf(c.tipo, sizeof c.tipo, "%s", movie ? "movie" : "series");
    cat_atualizar_item(0, &c);
    assert(!strcmp(cat_item(0)->tipo, movie ? "movie" : "series"));
  }
  if (!movie) {
    ep.temporada = special ? 0 : 1; ep.episodio = defaultSeries ? 1 : 2;
    snprintf(ep.vid, sizeof ep.vid, "tt9054364:%d:%d", ep.temporada, ep.episodio);
    cat_definir_episodios(0, &ep, 1);
    if (!defaultSeries) player_definir_episodio(expectedT, 2);
  }
  player_episodio_atual(&t, &e);
  printf("player episode %d:%d; catalog type=%s\n", t, e, cat_item(0)->tipo);
  if (!movie && (t != expectedT || e != (defaultSeries ? 1 : 2))) failed = 1;
  // Advance the fake pipeline against real monotonic time. This satisfies
  // the production watched-proof guard rather than changing its globals.
  for (int i = 0; i < 24; i++) {
    SDL_Delay(100); posicao += .1;
    player_atualizar(.1f, SDL_GetTicks());
  }
  if (lateClose) {
    // No frame or second setter between publication and closing: the close
    // boundary itself must recover the already explicit episode request.
    snprintf(c.tipo, sizeof c.tipo, "series"); cat_atualizar_item(0, &c);
  }
  if (retained) {
    int loads = cargas, seeks = buscas;
    player_preparar_retencao(); confirmado = 1;
    assert(player_suspender() && player_retido());
    assert(player_retomar_retido("tt9054364", 1, 2));
    assert(cargas == loads && buscas == seeks);
    player_episodio_atual(&t, &e); assert(t == 1 && e == 2);
    SDL_Delay(100); posicao += .1; player_atualizar(.1f, SDL_GetTicks());
    puts("retained source and episode unchanged without another load or seek");
  }
  if (next) {
    player_encerrar();
    assert(prog_por_chave("tt9054364_s1e2", &r) && !strcmp(r.tipo, "series"));
    ep.episodio = 3; snprintf(ep.vid, sizeof ep.vid, "tt9054364:1:3");
    cat_definir_episodios(0, &ep, 1);
    player_abrir(0, "https://fixture.invalid/next-episode.mp4");
    player_definir_episodio(1, 3); posicao = 120;
    for (int i = 0; i < 24; i++) {
      SDL_Delay(100); posicao += .1; player_atualizar(.1f, SDL_GetTicks());
    }
    player_episodio_atual(&t, &e); assert(t == 1 && e == 3);
    puts("next opening keeps its new source and explicit episode");
  }
  player_encerrar();
  printf("boundaries parental=%d id=%s refresh=%d\n", parentalRequests, parentalId, refreshRequests);
  if (parentalRequests != (next ? 2 : 1) || refreshRequests != (next || retained ? 2 : 1) ||
      strcmp(parentalId, movie ? "tt1629757" : "tt9054364")) failed = 1;
  const char *key = movie ? "tt1629757" : special ? "tt9054364_s0e2" :
                    defaultSeries ? "tt9054364_s1e1" : next ? "tt9054364_s1e3" : "tt9054364_s1e2";
  int saved = prog_por_chave(key, &r);
  printf("progress expected=%s exists=%d type=%s; stats id=%s\n",
         key, saved, saved ? r.tipo : "(missing)", statsId);
  if (!saved || strcmp(r.tipo, movie ? "movie" : "series")) failed = 1;
  if (!movie) {
    ProgRegistro other;
    int bare = prog_por_chave("tt9054364", &other);
    printf("series bare-movie progress exists=%d type=%s episode=%d:%d\n",
           bare, bare ? other.tipo : "(missing)", bare ? other.temporada : 0,
           bare ? other.episodio : 0);
    if (bare) failed = 1;
  }
  char expectedId[64];
  if (movie) snprintf(expectedId, sizeof expectedId, "tt1629757");
  else snprintf(expectedId, sizeof expectedId, "tt9054364:%d:%d", expectedT, expectedE);
  if (strcmp(statsId, expectedId)) failed = 1;
  char body[400];
  scrobble_corpo(body, sizeof body, statsId, 40);
  printf("stats payload %s\n", body);
  char expectedNumber[24], expectedSeason[24];
  snprintf(expectedNumber, sizeof expectedNumber, "\"number\":%d", expectedE);
  snprintf(expectedSeason, sizeof expectedSeason, "\"season\":%d", expectedT);
  if (movie ? !strstr(body, "\"movie\"") :
      (!strstr(body, "\"show\"") || !strstr(body, expectedSeason) || !strstr(body, expectedNumber)))
    failed = 1;
  SDL_Quit();
  return failed;
}
