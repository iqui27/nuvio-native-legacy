// Drives the production home trailer state machine with deterministic platform
// boundaries. The state machine itself comes from src/home.c; only catalog,
// settings, network-result, and trailer-player edges are stubbed here.
#include "../src/home.h"
#include "../src/ajustes.h"
#include "../src/trailer.h"
#include "../src/trailerapple.h"
#include "../src/extras.h"
#include "../src/anim.h"
#include "../src/trailerfonte.h"
#include <stdio.h>
#include <string.h>

#define static
#include "../src/home.c"
#undef static

static CatItem item;
static int trailerSetting = 1;
// "Fonte do trailer" (trailerfonte.h). A regra e a de producao
// (src/trailerfonte.c entra na linha do emcc); so o valor gravado e do teste.
static int fonteSetting = TRF_AUTO;
static int lastSom = -1;
static int appleReady;
static int appleOpenFails;
static int youtubeReady;
static int imdbReady, imdbAnswered;
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
int ajustes_trailer_fonte(void) { return fonteSetting; }

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
int trailer_estado(void) { return opened ? (playing ? 1 : -1) : -2; }
void trailer_abrir(const char *source, GfxRect r, int som, int cheia) {
  (void)r; (void)cheia;
  lastSom = som;
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

// IMDb (#136): na Samsung ele existe quando a build tem o servico de
// recomendacoes; aqui trailerfonte_definir_imdb_tizen decide.
void trailerimdb_pedir(const char *imdb) { (void)imdb; }
const char *trailerimdb_url(const char *imdb, const char **nome) {
  (void)imdb; if (nome) *nome = "Trailer";
  return imdbReady ? "https://media.test/imdb.mp4" : NULL;
}
int trailerimdb_respondeu(const char *imdb) { (void)imdb; return imdbReady || imdbAnswered; }

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
  fonteSetting = TRF_AUTO;
  lastSom = -1;
  appleReady = 0;
  appleOpenFails = 0;
  youtubeReady = 0;
  imdbReady = 0; imdbAnswered = 1;
  trailerfonte_definir_imdb_tizen(1);
  opened = 0;
  playing = 0;
  openedCount = 0;
  appleFailure = 0;
  lastSource[0] = 0;
}

int main(void) {
  const Uint32 start = 100;
  int rc = 0;

  // Apple is ready and opens, but fails before playback. The next source on
  // Samsung is IMDb (#136) and it is attempted exactly once. YouTube, even
  // with an id in hand, never opens in the hero: its iframe costs ~1 s of main
  // thread on the AU7000 and then fails with error 153.
  resetState("tt0000001");
  youtubeReady = 1;
  imdbReady = 1;
  appleReady = 1;
  appleOpenFails = 1;
  home_trailer_passo(1, 0.016f, start);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS);
  rc |= check("Apple abre somente depois da janela", openedCount == 1 && strstr(lastSource, "apple"));
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS + 1);
  rc |= check("falha Apple libera IMDb", opened && strstr(lastSource, "imdb"));
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS + 2);
  rc |= check("falha Apple nao repete IMDb", openedCount == 2);

  // A source which never reaches playing gets its own preparation window.
  // The fallback receives a fresh window too.
  resetState("tt0000002");
  youtubeReady = 1;
  imdbReady = 1;
  appleReady = 1;
  home_trailer_passo(1, 0.016f, start);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS);
  const Uint32 appleDeadline = start + NV_TRAILER_HERO_ESPERA_MS + NV_TRAILER_HERO_PREPARA_MS;
  const Uint32 nextDeadline = appleDeadline + NV_TRAILER_HERO_PREPARA_MS;
  home_trailer_passo(1, 0.016f, appleDeadline - 1);
  rc |= check("Apple ainda prepara antes do prazo proprio", opened && strstr(lastSource, "apple"));
  home_trailer_passo(1, 0.016f, appleDeadline);
  rc |= check("timeout Apple abre IMDb com prazo novo", opened && strstr(lastSource, "imdb"));
  home_trailer_passo(1, 0.016f, appleDeadline + 1);
  rc |= check("IMDb permanece aberto durante a janela nova", opened && strstr(lastSource, "imdb"));
  home_trailer_passo(1, 0.016f, nextDeadline);
  rc |= check("timeout IMDb encerra fonte e libera hero (sem YouTube)", !opened && heroTrailerFonte == 3 &&
              openedCount == 2 && !heroTrailerSegurando(nextDeadline));

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

  // Apple ainda sem resposta e o IMDb ja em maos: o hero espera a Apple a
  // janela inteira; so no fim dela, sem Apple, o IMDb entra.
  resetState("tt0000002c");
  imdbReady = 1;
  youtubeReady = 1;
  home_trailer_passo(1, 0.016f, start);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS);
  rc |= check("IMDb nao atropela a Apple sem resposta", !opened && openedCount == 0);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_MAX_ESPERA_MS);
  rc |= check("sem Apple no fim da janela, IMDb entra", opened && strstr(lastSource, "imdb"));

  // So o YouTube tem trailer: no hero da Samsung, nada abre (fica a arte).
  resetState("tt0000002d");
  youtubeReady = 1;
  appleReady = 0;
  home_trailer_passo(1, 0.016f, start);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_MAX_ESPERA_MS);
  rc |= check("hero da Samsung nunca abre o iframe do YouTube", openedCount == 0 && heroTrailerTentado &&
              !heroTrailerSegurando(start + NV_TRAILER_HERO_MAX_ESPERA_MS));

  // Without any source, the source wait is finite and the helper stops
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

  // --- "Fonte do trailer" no hero da Samsung (este binario e -D__EMSCRIPTEN__).
  // Apple fixa: erro da Apple NAO cai em outra fonte.
  resetState("tt0000010");
  fonteSetting = TRF_APPLE;
  youtubeReady = 1;
  imdbReady = 1;
  appleReady = 1;
  appleOpenFails = 1;
  home_trailer_passo(1, 0.016f, start);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS + 1);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS + 2);
  rc |= check("Apple fixa: erro fica a arte, sem outra fonte", openedCount == 1 && !opened &&
              heroTrailerFonte == 3 && strstr(lastSource, "apple"));

  // YouTube fixo: o hero da Samsung nao abre o iframe; fica a arte, com
  // prazo finito (a pagina do titulo ainda toca o YouTube).
  resetState("tt0000011");
  fonteSetting = TRF_YOUTUBE;
  youtubeReady = 1;
  appleReady = 1;
  home_trailer_passo(1, 0.016f, start);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS);
  rc |= check("YouTube fixo: hero nao abre nada", openedCount == 0 && heroTrailerTentado &&
              !heroTrailerSegurando(start + NV_TRAILER_HERO_ESPERA_MS));

  // IMDb fixo com o servico: so IMDb, mudo, sem esperar a Apple.
  resetState("tt0000012");
  fonteSetting = TRF_IMDB;
  imdbReady = 1;
  appleReady = 1;
  home_trailer_passo(1, 0.016f, start);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS);
  rc |= check("IMDb fixo com servico: so IMDb", openedCount == 1 && opened && strstr(lastSource, "imdb"));
  rc |= check("hero da Samsung abre mudo", lastSom == 0);

  // IMDb fixo SEM o servico na build: nada, com prazo finito.
  resetState("tt0000012b");
  trailerfonte_definir_imdb_tizen(0);
  fonteSetting = TRF_IMDB;
  imdbReady = 1;
  appleReady = 1;
  home_trailer_passo(1, 0.016f, start);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS);
  rc |= check("IMDb fixo sem servico: nada abre", openedCount == 0 && heroTrailerTentado &&
              !heroTrailerSegurando(start + NV_TRAILER_HERO_ESPERA_MS));

  // Automatico mantem a ordem: com todas prontas, a Apple.
  resetState("tt0000013");
  youtubeReady = 1;
  imdbReady = 1;
  appleReady = 1;
  home_trailer_passo(1, 0.016f, start);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS);
  rc |= check("Automatico: Apple primeiro", openedCount == 1 && strstr(lastSource, "apple"));

  puts(rc ? "home-trailer-timer: FALHOU" : "home-trailer-timer: tudo ok");
  return rc ? 1 : 0;
}
