// #524 TEST-ONLY: characterize aplicarProgressoEm when two confirmed slots share a
// base content id but differ in tipo (movie vs series). Synthetic ids only; no account
// data. Expected values are source-derived from catalogo.c @ BASE; compile/run NOTRUN.
#include "../src/catalogo.h"
#include "../src/progresso.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int         ajustes_idioma_ingles(void) { return 0; }
int         ajustes_idioma(void) { return 0; }
const char *i18n(const char *s)         { return s; }
const char *idioma_mes_data(int mes, const char *nomePt) { (void)mes; return nomePt; }
const char *dados_dir(void)             { return ""; }
const char *sessao_usuario(void)        { return ""; }
int         perfis_ativo(void)          { return 1; }
const char *desc_genero_pt(const char *g) { return g; }
int prog_gravar_local(const char *imdb, int t, int e, double p, double d, int tocou) {
  (void)imdb; (void)t; (void)e; (void)p; (void)d; (void)tocou; return 1;
}

static const char *BASE = "tt9054364";
static const char *OTHER = "tt0111161";
static ProgRegistro progRegs[4];
static int nProg;

int prog_ler(ProgRegistro *saida, int max) {
  int k = nProg < max ? nProg : max;
  if (k > 0) memcpy(saida, progRegs, sizeof(ProgRegistro) * (size_t)k);
  return k;
}

static void prog_series_default(void) {
  memset(progRegs, 0, sizeof progRegs);
  nProg = 1;
  snprintf(progRegs[0].contentId, sizeof progRegs[0].contentId, "%s", BASE);
  snprintf(progRegs[0].tipo, sizeof progRegs[0].tipo, "series");
  progRegs[0].temporada = 1;
  progRegs[0].episodio = 2;
  progRegs[0].posSeg = 120;
  progRegs[0].durSeg = 300;
  progRegs[0].lastWatchedMs = 1000;
  progRegs[0].tocou = 1;
}

static void seed(CatItem *v, int m, const char *id, const char *tipo, long long ms, int pct) {
  assert(m >= 2);
  memset(v, 0, sizeof(CatItem) * 2);
  snprintf(v[0].imdb, sizeof v[0].imdb, "%s", id);
  snprintf(v[0].tipo, sizeof v[0].tipo, "%s", "series");
  snprintf(v[0].titulo, sizeof v[0].titulo, "Fixture series slot");
  snprintf(v[1].imdb, sizeof v[1].imdb, "%s", id);
  snprintf(v[1].tipo, sizeof v[1].tipo, "%s", "movie");
  snprintf(v[1].titulo, sizeof v[1].titulo, "Fixture movie slot");
  if (pct > 0) {
    v[0].progresso = pct;
    v[0].retomadoMs = ms;
    v[0].temporada = 1;
    v[0].episodio = 2;
  }
  if (ms > 0 && tipo && !strcmp(tipo, "movie")) {
    v[1].retomadoMs = ms;
    if (pct > 0) v[1].progresso = pct;
  }
}

static void expect_opposite_apply_both(void) {
  CatItem v[2];
  prog_series_default();
  seed(v, 2, BASE, "series", 0, 0);
  cat_definir_tudo(v, 2, NULL, 0);
  /* Source: aplicarProgressoEm matches mesmoTitulo only; mesmaCopiaEm type check is NOT used here. */
  assert(cat_item(0)->progresso == 40);
  assert(cat_item(0)->temporada == 1 && cat_item(0)->episodio == 2);
  assert(cat_item(1)->progresso == 40);
  assert(cat_item(1)->temporada == 1 && cat_item(1)->episodio == 2);
  printf("ok  series ProgRegistro applies episode coordinates to both movie+series slots (source-derived)\n");
}

static void expect_newer_movie_vetoes_disk(void) {
  CatItem v[2];
  prog_series_default();
  seed(v, 2, BASE, "movie", 5000, 0);
  v[1].retomadoMs = 5000;
  cat_definir_tudo(v, 2, NULL, 0);
  /* Source: maisNova when any same-title slot has retomadoMs > reg.lastWatchedMs skips aplicarUmEm */
  assert(cat_item(0)->progresso == 0);
  assert(cat_item(1)->progresso == 0);
  /* espalharEm uses mesmaCopiaEm (requires matching tipo); no cross-type spread from zero slots. */
  printf("ok  newer opposite-type retomadoMs veto leaves both slots without disk progress\n");
}

static void expect_different_id_ignored(void) {
  CatItem v[2];
  prog_series_default();
  snprintf(v[0].imdb, sizeof v[0].imdb, "%s", OTHER);
  snprintf(v[0].tipo, sizeof v[0].tipo, "series");
  snprintf(v[1].imdb, sizeof v[1].imdb, "%s", BASE);
  snprintf(v[1].tipo, sizeof v[1].tipo, "movie");
  cat_definir_tudo(v, 2, NULL, 0);
  assert(cat_item(0)->progresso == 0);
  assert(cat_item(1)->progresso == 40);
  printf("ok  reg contentId matches only same-base-title slots\n");
}

static void expect_short_duration_skipped(void) {
  CatItem v[2];
  prog_series_default();
  progRegs[0].durSeg = 0.5;
  seed(v, 2, BASE, "series", 0, 0);
  cat_definir_tudo(v, 2, NULL, 0);
  /* Source: aplicarProgressoEm applies only when regs[i].durSeg > 1.0 */
  assert(cat_item(0)->progresso == 0 && cat_item(1)->progresso == 0);
  printf("ok  durSeg<=1 reg does not apply\n");
}

static void expect_same_type_espalhar_only(void) {
  CatItem v[3];
  nProg = 0;
  snprintf(v[0].imdb, sizeof v[0].imdb, "%s", BASE);
  snprintf(v[0].tipo, sizeof v[0].tipo, "series");
  snprintf(v[0].titulo, sizeof v[0].titulo, "CW series");
  v[0].progresso = 55;
  v[0].retomadoMs = 8000;
  v[0].temporada = 1;
  v[0].episodio = 2;
  snprintf(v[1].imdb, sizeof v[1].imdb, "%s", BASE);
  snprintf(v[1].tipo, sizeof v[1].tipo, "series");
  snprintf(v[1].titulo, sizeof v[1].titulo, "Catalog series");
  snprintf(v[2].imdb, sizeof v[2].imdb, "%s", BASE);
  snprintf(v[2].tipo, sizeof v[2].tipo, "movie");
  snprintf(v[2].titulo, sizeof v[2].titulo, "Movie slot");
  cat_definir_tudo(v, 3, NULL, 0);
  assert(cat_item(1)->progresso == 55);
  assert(cat_item(2)->progresso == 0);
  printf("ok  espalharEm spreads across same tipo only (mesmaCopiaEm), not to movie slot\n");
}

int main(void) {
  expect_opposite_apply_both();
  expect_newer_movie_vetoes_disk();
  expect_different_id_ignored();
  expect_short_duration_skipped();
  expect_same_type_espalhar_only();
  puts("fv524_progress_type: aplicarProgressoEm boundary characterization PASS");
  return 0;
}
