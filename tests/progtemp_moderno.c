// Progress uses one season chart; the colored ratings and retention stay together
// in the existing season numbers block. Unknown history is not known zero.
#define extras_n_estudios studioCountFixture
#define extras_estudio_tmdb studioIdFixture
#define extras_estudio_nome studioNameFixture
#define extras_estudio_rede studioNetworkFixture
#define vertudo_colecao studioOpenFixture
#define vertudo_aberta studioVisibleFixture
#define vertudo_evento studioEventFixture
#define extras_nota ratingFixture
#define extras_n_temporadas seasonCountFixture
#define extras_temporada_numero seasonNumberFixture
#define extras_n_eps episodeCountFixture
#define seriefrases_n quoteCountFixture
#define seriefrases_n_fatos factCountFixture
#define extras_comentario_spoiler comSpoilerFixture
#define extras_n_comentarios comCountFixture
#define extras_relacionado_imdb relFixture
#define juntos_disponivel togetherAvailableFixture
#define travaui_disponivel lockAvailableFixture
#define juntosui_abrir_folha togetherOpenFixture
#define travaui_abrir lockOpenFixture
#include "../src/detail.c"
static int studios;
int studioCountFixture(void) { return studios; }
long studioIdFixture(int i) { (void)i; return 0; }
const char *studioNameFixture(int i) { (void)i; return ""; }
int studioNetworkFixture(int i) { (void)i; return 0; }
void studioOpenFixture(const ColFolder *f) { (void)f; }
int studioVisibleFixture(void) { return 0; }
void studioEventFixture(const SDL_Event *e) { (void)e; }
int ratingFixture(int f) { (void)f; return 0; }
int seasonCountFixture(void) { return 1; }
int seasonNumberFixture(int t) { (void)t; return 1; }
int episodeCountFixture(int t) { (void)t; return 6; }
int togetherAvailableFixture(void) { return 0; }
int lockAvailableFixture(void) { return 0; }
void togetherOpenFixture(int i,int t,int e) { (void)i; (void)t; (void)e; }
int lockOpenFixture(const CatItem *ci,int i) { (void)ci; (void)i; return 0; }
#undef extras_relacionado_imdb
int quoteCountFixture(void) { return 0; }
int factCountFixture(void) { return 0; }
int comSpoilerFixture(int ep,int i) { (void)ep; (void)i; return 0; }
int comCountFixture(void) { return 0; }
const char *relFixture(int i) { (void)i; return ""; }

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "dados.h"
#include "temporadas_grafico.h"
#include "vistoep.h"

static CatItem serieItem;
static CatEp eps[12];

static void episodiosComNota(int n) {
  int i;
  for (i = 0; i < n; i++) {
    memset(&eps[i], 0, sizeof eps[i]);
    eps[i].temporada = 1;
    eps[i].episodio = i + 1;
    eps[i].nota = 75 + i;
    snprintf(eps[i].nome, sizeof eps[i].nome, "Ep %d", i + 1);
  }
  cat_definir_episodios(0, eps, n);
}

static void layoutHome(int modo) {
  char path[1024];
  FILE *f;
  snprintf(path, sizeof path, "%s/ajustes.txt", dados_dir());
  f = fopen(path, "w"); assert(f);
  fprintf(f, "idioma 0\nhomeLayoutLocal %d\n", modo);
  fclose(f);
  ajustes_dir(dados_dir());
}

int main(void) {
  char path[1024];
  const TgDados *d;
  float h, hTg;
  int falhas = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %d: %s\n", __LINE__, #c); falhas++; } } while (0)

  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  ajustes_iniciar();
  dados_iniciar("deploy/app/art");
  snprintf(path, sizeof path, "%s/ajustes.txt", dados_dir());
  { FILE *f = fopen(path, "w"); assert(f); fputs("idioma 0\n", f); fclose(f); }
  ajustes_dir(dados_dir());

  memset(&serieItem, 0, sizeof serieItem);
  snprintf(serieItem.tipo, sizeof serieItem.tipo, "series");
  snprintf(serieItem.imdb, sizeof serieItem.imdb, "tt7700001");
  serieItem.temporadas[0] = 1;
  serieItem.nTemporadas = 1;
  cat_definir_tudo(&serieItem, 1, NULL, 0);
  idx = 0;
  vistoep_esquecer();
  episodiosComNota(6);

  layoutHome(HOME_LAYOUT_MODERNA);
  recalcularLayout();
  d = tgraf_dados(idx);
  CHECK(tgraf_existe(d));
  hTg = tgraf_altura_dados(d);
  h = alturaSecao(SEC_PROGTEMP);
  CHECK(h > 0.0f);
  CHECK(fabsf(h - hTg) < 0.5f);
  CHECK(secaoN(SEC_PROGTEMP) == 1);
  CHECK(secaoN(SEC_NUMEROS) == 6);
  CHECK(notasDados()->nTemp == 1);
  CHECK(!vistoep_conhecido(serieItem.imdb));
  CHECK(d->vistos == 0 && !d->sabe && d->exibidos == 6);

  vistoep_definir(serieItem.imdb, 1, 1, 0);
  d = tgraf_dados(idx);
  CHECK(vistoep_conhecido(serieItem.imdb));
  CHECK(d->sabe && d->vistos == 0 && tgraf_existe(d));

  layoutHome(HOME_LAYOUT_DINAMICA);
  recalcularLayout();
  h = alturaSecao(SEC_PROGTEMP);
  hTg = tgraf_altura_dados(tgraf_dados(idx));
  CHECK(fabsf(h - hTg) < 0.5f);

  printf(falhas ? "FALHAS: %d\n" : "PASS: progtemp_moderno\n", falhas);
  SDL_Quit();
  return falhas != 0;
}
