// #356 / Shield #5: "a seguir" e proximos episodios sumiam quando a ficha do
// catalogo do Nuvio nao listava o episodio futuro (The Rookie: o Nuvio tem so o
// S9E1 placeholder, o Cinemeta tem os 18). Fixtures reais aparadas em
// tests/fixtures/{nuvio,cinemeta}_the_rookie.json.
#include "trakt.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned int SDL_GetTicks(void) { return 0; }
void marco(const char *m) { (void)m; }

static int nuvioGets, cineGets;
static long long estreiaMs;
static char estreiaId[48];

static char *ler(const char *nome) {
  char path[200]; FILE *f; char *b; long n;
  snprintf(path, sizeof path, "tests/fixtures/%s", nome);
  f = fopen(path, "rb"); assert(f);
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)n + 1); assert(fread(b, 1, (size_t)n, f) == (size_t)n);
  b[n] = 0; fclose(f); return b;
}
char *rede_baixar(const char *url, int s) {
  (void)s;
  if (strstr(url, "catalog.nuvio.tv") && strstr(url, "tt7587890")) {
    nuvioGets++; return ler("nuvio_the_rookie.json");
  }
  if (strstr(url, "cinemeta") && strstr(url, "tt7587890")) {
    cineGets++; return ler("cinemeta_the_rookie.json");
  }
  return NULL;
}
char *rede_baixar_com(const char *u, int s, const char *const *c) { (void)u; (void)s; (void)c; return NULL; }
char *rede_apagar(const char *u, int s, const char *const *c, int *st) { (void)u; (void)s; (void)c; if (st) *st = 0; return NULL; }
void rede_avisar_401(void (*cb)(const char *)) { (void)cb; }
char *rede_baixar_st(const char *u, int s, const char *const *c, int *st) {
  (void)u; (void)s; (void)c; if (st) *st = 0; return NULL;
}
int cwo_conta_a_seguir(const char *id) { (void)id; return 1; }   // tudo aqui e "a seguir"
void cwo_conta_trocar(const char *a, const char *b) { (void)a; (void)b; }
void cwo_marcar_estreia(const char *id, long long ms) { snprintf(estreiaId, sizeof estreiaId, "%s", id); estreiaMs = ms; }
int cwo_virada_aceita(long long e, long long a) { (void)e; (void)a; return 1; }
int ajustes_tmdb_cw(void) { return 0; }
const char *ajustes_tmdb_idioma(void) { return "en-US"; }
const char *desc_chave_tmdb(void) { return ""; }
const char *desc_tmdb_idioma(void) { return "en"; }
const char *i18n(const char *s) { return s; }
unsigned long long cat_historico_geracao(void) { return 1; }
int cat_historico_definir_se_geracao(const char *a, const char *b, int c, unsigned long long g) {
  (void)a; (void)b; (void)c; (void)g; return 1;
}

static int um(int t, int e, CatItem *o) {
  static CatItem v[1];
  memset(v, 0, sizeof v);
  snprintf(v[0].tipo, sizeof v[0].tipo, "series");
  snprintf(v[0].titulo, sizeof v[0].titulo, "The Rookie");
  snprintf(v[0].imdb, sizeof v[0].imdb, "tt7587890:%d:%d", t, e);
  v[0].temporada = t; v[0].episodio = e;
  int n = trakt_enfeitar_lote(v, 1);
  *o = v[0];
  return n;
}

int main(void) {
  CatItem r;
  // Nuvio tem o episodio: nao ha por que perguntar ao Cinemeta.
  nuvioGets = cineGets = 0;
  assert(um(9, 1, &r) == 1);
  assert(cineGets == 0 && nuvioGets >= 1);
  // So o Cinemeta lista o S9E2: o item fica, com nome/data do Cinemeta.
  cineGets = 0; estreiaMs = 0;
  assert(um(9, 2, &r) == 1);
  assert(cineGets == 1);
  assert(!strcmp(r.imdb, "tt7587890:9:2"));
  assert(r.nomeEpisodio[0]);
  assert(estreiaMs > 0);   // released do Cinemeta (2027)
  // Em nenhuma das duas: cai (nao ha S9E19 nem S10E1).
  assert(um(9, 19, &r) == 0);
  puts("trakt_ep_cinemeta: PASS");
  return 0;
}
