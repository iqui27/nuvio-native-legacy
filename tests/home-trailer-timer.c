// Drives the production home trailer state machine with deterministic platform
// boundaries. The state machine itself comes from src/home.c; only catalog,
// settings, network-result, and trailer-player edges are stubbed here.
#include "../src/home.h"
#include "../src/ajustes.h"
#include "../src/trailer.h"
#include "../src/trailerapple.h"
#include "../src/extras.h"
#include "../src/anim.h"
#include <stdio.h>
#include <string.h>

#define static
#include "../src/home.c"
#undef static

static CatItem item;
static int trailerSetting = 1;
static int appleReady;
static int appleOpenFails;
static int youtubeReady;
static int opened;
static int playing;
static int openedCount;
static int appleFailure;
static char lastSource[128];

int cat_n(void) { return 1; }
const CatItem *cat_item(int i) { return i == 0 ? &item : NULL; }

int ajustes_hero_ligado(void) { return 1; }
int ajustes_trailer_hero(void) { return trailerSetting; }
int ajustes_tmdb_trailers(void) { return 1; }

int trailer_suportado(void) { return 1; }
int trailer_aberto(void) { return opened; }
int trailer_cheia(void) { return 0; }
int trailer_tocando(void) { return opened && playing; }
int trailer_falhou(void) {
  int r = appleFailure;
  appleFailure = 0;
  return r;
}
void trailer_fechar(void) { opened = 0; playing = 0; }
void trailer_abrir(const char *source, GfxRect r, int som, int cheia) {
  (void)r; (void)som; (void)cheia;
  snprintf(lastSource, sizeof lastSource, "%s", source);
  openedCount++;
  if (appleOpenFails && strstr(source, "apple")) {
    appleFailure = 1;
    opened = 0;
    playing = 0;
    return;
  }
  opened = 1;
  playing = 0;
}

void trailerapple_pedir(const char *imdb, const char *titulo, const char *meta, int serie) {
  (void)imdb; (void)titulo; (void)meta; (void)serie;
}
const char *trailerapple_url(const char *imdb) {
  (void)imdb;
  return appleReady ? "https://media.test/apple.m3u8" : NULL;
}
int trailerapple_respondeu(const char *imdb) { (void)imdb; return appleReady; }

void extras_hero_trailer_pedir(const char *imdb, int serie, long tmdbId) {
  (void)imdb; (void)serie; (void)tmdbId;
}
int extras_hero_trailer_obter(const char *imdb, char *dst, unsigned cap) {
  (void)imdb;
  if (!youtubeReady || !dst || cap < 12) return 0;
  snprintf(dst, cap, "%s", "dQw4w9WgXcQ");
  return 1;
}

static int check(const char *name, int ok) {
  if (!ok) { fprintf(stderr, "FALHOU: %s\n", name); return 1; }
  printf("ok %s\n", name);
  return 0;
}

static void resetState(const char *id) {
  memset(&item, 0, sizeof item);
  snprintf(item.imdb, sizeof item.imdb, "%s", id);
  snprintf(item.titulo, sizeof item.titulo, "Fixture");
  snprintf(item.meta, sizeof item.meta, "2026");
  snprintf(item.tipo, sizeof item.tipo, "movie");
  heroAtual = 0;
  heroDesejado = -1;
  heroEntra = 1.0f;
  heroSai = 0.0f;
  focoHero = 1;
  foco.fileira = -1;
  heroTrailerItem = -1;
  heroTrailerImdb[0] = 0;
  heroTrailerDesde = 0;
  heroTrailerTentado = 0;
  heroTrailerPreparandoAte = 0;
  heroTrailerFonte = 0;
  heroTrailerAppleFalhou = 0;
  heroTrailerFade = 0.0f;
  trailerSetting = 1;
  appleReady = 0;
  appleOpenFails = 0;
  youtubeReady = 0;
  opened = 0;
  playing = 0;
  openedCount = 0;
  appleFailure = 0;
  lastSource[0] = 0;
}

int main(void) {
  const Uint32 start = 100;
  int rc = 0;

  // Apple is ready and opens, but fails before playback. The next source is
  // YouTube and it is attempted exactly once.
  resetState("tt0000001");
  youtubeReady = 1;
  appleReady = 1;
  appleOpenFails = 1;
  home_trailer_passo(1, 0.016f, start);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS);
  rc |= check("Apple abre somente depois da janela", openedCount == 1 && strstr(lastSource, "apple"));
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS + 1);
  rc |= check("falha Apple libera YouTube", opened && !strcmp(lastSource, "dQw4w9WgXcQ"));
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS + 2);
  rc |= check("falha Apple nao repete YouTube", openedCount == 2);

  // A source which never reaches playing gets its own preparation window.
  // The fallback receives a fresh window too; it must stay open until that
  // second deadline instead of inheriting Apple's already-spent time.
  resetState("tt0000002");
  youtubeReady = 1;
  appleReady = 1;
  home_trailer_passo(1, 0.016f, start);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS);
  const Uint32 appleDeadline = start + NV_TRAILER_HERO_ESPERA_MS + NV_TRAILER_HERO_PREPARA_MS;
  const Uint32 youtubeDeadline = appleDeadline + NV_TRAILER_HERO_PREPARA_MS;
  home_trailer_passo(1, 0.016f, appleDeadline - 1);
  rc |= check("Apple ainda prepara antes do prazo proprio", opened && strstr(lastSource, "apple"));
  home_trailer_passo(1, 0.016f, appleDeadline);
  rc |= check("timeout Apple abre YouTube com prazo novo", opened && !strcmp(lastSource, "dQw4w9WgXcQ"));
  home_trailer_passo(1, 0.016f, appleDeadline + 1);
  rc |= check("YouTube permanece aberto durante a janela nova", opened && !strcmp(lastSource, "dQw4w9WgXcQ"));
  home_trailer_passo(1, 0.016f, youtubeDeadline);
  rc |= check("timeout YouTube encerra fonte e libera hero", !opened && heroTrailerFonte == 3 &&
              !heroTrailerSegurando(youtubeDeadline));

  // A source resolved at 3199 ms still gets a complete preparation window;
  // the resolution budget is not reused as its playback deadline.
  resetState("tt0000002b");
  appleReady = 1;
  home_trailer_passo(1, 0.016f, start);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_MAX_ESPERA_MS - 1);
  rc |= check("Apple no ultimo instante ganha janela util", opened && strstr(lastSource, "apple") &&
              heroTrailerPreparandoAte == start + NV_TRAILER_HERO_MAX_ESPERA_MS - 1 + NV_TRAILER_HERO_PREPARA_MS);

  // Turning the preference off during playback closes the source, clears the
  // fade immediately, and leaves the rotation helper free.
  playing = 1;
  heroTrailerFade = 0.8f;
  trailerSetting = 0;
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_MAX_ESPERA_MS + 1);
  rc |= check("OFF durante playback reseta trailer e fade", !opened && heroTrailerItem < 0 && heroTrailerFade == 0.0f);
  rc |= check("OFF libera rotacao", !heroTrailerSegurando(start + NV_TRAILER_HERO_MAX_ESPERA_MS + 1));

  // Without Apple or YouTube, the source wait is finite and the helper stops
  // holding the hero at the exact configured budget.
  resetState("tt0000003");
  home_trailer_passo(1, 0.016f, start);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_MAX_ESPERA_MS);
  rc |= check("sem fontes encerra no limite", !opened && heroTrailerTentado &&
              !heroTrailerSegurando(start + NV_TRAILER_HERO_MAX_ESPERA_MS));

  // Reusing a hero slot for another identity cannot retain the old fade or
  // preparation state.
  heroTrailerFade = 0.9f;
  snprintf(item.imdb, sizeof item.imdb, "%s", "tt0000004");
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_MAX_ESPERA_MS + 1);
  rc |= check("troca de identidade reseta fade", heroTrailerFade == 0.0f);

  puts(rc ? "home-trailer-timer: FALHOU" : "home-trailer-timer: tudo ok");
  return rc ? 1 : 0;
}
