#include "catalogo.h"
#include "proximo.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static int cwTocarT, cwTocarE, cwEspera, requests, metadata;
static CatItem cwEsperaItem;
static unsigned cwEsperaDesde;
#define CW_ESPERA_MS 15000u
static unsigned SDL_GetTicks(void) { return 1000; }
static int ajustes_cw_concluido(void) { return 90; }
static int detail_indice(void) { return 0; }
static int detail_aberto(void) { return 1; }
static int player_aberto(void) { return 0; }
static void abrirTitulo(const void *it, int a, int b) { (void)it; (void)a; (void)b; metadata++; }
static void desc_episodios(int i, int t) { (void)i; (void)t; assert(metadata); }
static char requested[96];
static void detail_pedir_reproduzir(void) {
  assert(metadata && cat_n_episodios(0));
  assert(cat_id_stream(0, cwTocarT, cwTocarE, requested, sizeof requested));
  requests++;
}
int prog_ler(void *out, int max) { (void)out; (void)max; return 0; }
int ajustes_idioma(void) { return 0; }
const char *i18n(const char *s) { return s; }
const char *dados_dir(void) { return ""; }
const char *sessao_usuario(void) { return "cold-cw"; }
const char *desc_genero_pt(const char *s) { return s; }
int prox_seguinte(const CatItem *c, const CatEp *v, int n, int p, long long now, ProxSugestao *out) {
  (void)c; (void)v; (void)n; (void)p; (void)now; (void)out;
  assert(0 && "unfinished CW must resume its current episode"); return 0;
}
#include "cold-router.inc"
int main(void) {
  const char *ids[] = {"tmdb:123", "kitsu:7442", "tt1234567"};
  for (int i = 0; i < 3; i++) {
    CatItem c = {0}; strcpy(c.imdb, ids[i]); strcpy(c.tipo, "series");
    c.temporada = 2; c.episodio = 5; c.progresso = 42;
    cat_definir_tudo(&c, 1, NULL, 0);
    requests = metadata = cwEspera = 0;
    cold_open();
    assert(metadata == 1 && cwEspera && !requests);
    cold_wait(); assert(!requests);
    CatEp ep = {0}; ep.temporada = 2; ep.episodio = 5;
    snprintf(ep.vid, sizeof ep.vid, "%s:2:5", ids[i]);
    c.epsParciais = 1;
    cat_atualizar_item(0, &c);
    cat_definir_episodios(0, &ep, 1);
    cold_wait(); assert(cwEspera && !requests);
    // Opening CW while the provisional list is already visible must also wait.
    cold_open(); assert(cwEspera && !requests);
    cold_wait(); assert(cwEspera && !requests);
    c.epsParciais = 0;
    ep.episodio = 4;
    strcpy(ep.vid, "provider:other-video");
    cat_definir_episodios(0, &ep, 1);
    cat_atualizar_item(0, &c);
    cold_wait(); assert(cwEspera && !requests);
    cold_open(); assert(cwEspera && !requests);
    ep.episodio = 5;
    strcpy(ep.vid, "provider:opaque-video");
    cat_definir_episodios(0, &ep, 1);
    cold_wait();
    assert(requests == 1 && !cwEspera);
    assert(!strcmp(requested, ep.vid));
    char manual[96];
    assert(cat_id_stream(0, 2, 5, manual, sizeof manual));
    assert(!strcmp(requested, manual));
  }
  puts("PASS cold CW waits for metadata before requesting opaque episode IDs");
}
