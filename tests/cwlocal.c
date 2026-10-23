// #158: production local CW builder with an offline metadata service. Reuse
// discovery doubles, replace only the enrichment policy that compacts rows.
#define main cwordem_regression_main
#include "../src/catalogo.h"
static int cwlocal_enfeitar_lote(CatItem *v, int n);
#define CWLOCAL_ENFEITAR
#include "cwordem_desc.c"
#include "jellyfin_stub.inc"
#undef main
#undef CWLOCAL_ENFEITAR

static int falhaMeta = 0, enriquecidos;
static int cwlocal_enfeitar_lote(CatItem *v, int n) {
  int w = 0;
  for (int i = 0; i < n; i++) {
    if (falhaMeta || !v[i].poster[0]) continue;
    if (idbase_e_imdb(v[i].imdb)) {
      snprintf(v[i].sinopse, sizeof v[i].sinopse, "Metadados novos");
      if (!v[i].titulo[0]) snprintf(v[i].titulo, sizeof v[i].titulo, "IMDb recuperado");
    }
    v[w++] = v[i];
  }
  enriquecidos = w;
  return w;
}
static CatItem *porId(CatItem *v, int n, const char *id) {
  for (int i = 0; i < n; i++) if (!strcmp(v[i].imdb, id)) return &v[i];
  assert(0 && "local progress disappeared"); return NULL;
}
static void verificar(CatItem *v, int n) {
  assert(n == 5);
  CatItem *c = porId(v, n, "tt101:1:3");
  assert(!strcmp(c->titulo, "IMDb serie salva") && !strcmp(c->origem, "fixture.provider"));
  assert(c->progresso == 40 && c->temporada == 1 && c->episodio == 3 && c->retomadoMs == 1000000);
  assert(!c->nomeEpisodio[0] && c->restanteMin == 3);
  c = porId(v, n, "tmdb:t101:2:3"); assert(!strcmp(c->titulo, "TMDB salvo") && c->poster[0]);
  c = porId(v, n, "kitsu:41370:1:5"); assert(!strcmp(c->titulo, "Kitsu salvo") && c->poster[0]);
  assert(!strcmp(c->origem, "fixture.provider"));
  c = porId(v, n, "provider:42"); assert(!strcmp(c->titulo, "Provider salvo") && c->progresso == 40);
  c = porId(v, n, "provider:99"); assert(!strcmp(c->titulo, "Filme") && !c->poster[0]);
}
int main(void) {
  static CatItem cache[6], lote[12];
  const char *ids[] = {"tt101:1:1", "tmdb:t101:2:1", "kitsu:41370:1:1", "provider:42", "tt101", "kitsu:41370:1:2"};
  const char *titulos[] = {"IMDb serie salva", "TMDB salvo", "Kitsu sem arte", "Provider salvo", "IMDb filme homonimo", "Kitsu salvo"};
  for (int i = 0; i < 6; i++) {
    snprintf(cache[i].imdb, sizeof cache[i].imdb, "%s", ids[i]);
    snprintf(cache[i].titulo, sizeof cache[i].titulo, "%s", titulos[i]);
    snprintf(cache[i].tipo, sizeof cache[i].tipo, "%s", (i == 3 || i == 4) ? "movie" : "series");
    if (i != 2) snprintf(cache[i].poster, sizeof cache[i].poster, "https://fixture.invalid/poster%d.jpg", i);
    snprintf(cache[i].origem, sizeof cache[i].origem, "fixture.provider");
    snprintf(cache[i].nomeEpisodio, sizeof cache[i].nomeEpisodio, "Episodio anterior");
    cache[i].temporada = 1; cache[i].episodio = 1;
  }
  cat_definir(cache, 6);
  CatItem copia;
  assert(cat_copiar_por_id("tt101:9:9", "movie", &copia) && !strcmp(copia.titulo, "IMDb filme homonimo"));
  assert(cat_copiar_por_id("tt101:9:9", "series", &copia) && !strcmp(copia.titulo, "IMDb serie salva"));
  assert(!cat_copiar_por_id("kitsu:1", "series", &copia));
  assert(cat_copiar_por_id("kitsu:41370:4", "series", &copia) && !strcmp(copia.titulo, "Kitsu salvo"));
  // Copy remains valid after catalog replacement and frame retirement.
  cat_definir(cache, 6); cat_quadro(); cat_quadro(); assert(!strcmp(copia.titulo, "Kitsu salvo"));
  prog_definir_relogio(relogioProg); prog_esquecer_tudo();
  assert(prog_gravar_local("tt101", 1, 3, 120, 300, -1));
  assert(prog_gravar_local("tmdb:t101", 2, 3, 120, 300, -1));
  assert(prog_gravar_local("kitsu:41370", 1, 5, 120, 300, -1));
  assert(prog_gravar_local("provider:42", 0, 0, 120, 300, -1));
  assert(prog_gravar_local("provider:99", 0, 0, 120, 300, -1));
  assert(prog_gravar_local("ttStarted", 0, 0, 1, 300, -1));
  assert(prog_gravar_local("ttDone", 0, 0, 270, 300, -1));
  assert(prog_gravar_local("ttShort", 0, 0, 10, 30, -1));
  int n = continuarLocal(lote, 12); verificar(lote, n); assert(enriquecidos == 4);
  assert(!strcmp(porId(lote, n, "tt101:1:3")->sinopse, "Metadados novos"));
  falhaMeta = 1; n = continuarLocal(lote, 12); verificar(lote, n); assert(enriquecidos == 0);
  // Removing a collection's catalog row must not remove local CW: snapshot
  // publication still carries its metadata, and an empty catalog keeps progress.
  cat_definir(NULL, 0); n = continuarLocal(lote, 12); assert(n == 5);
  assert(!strcmp(porId(lote, n, "tmdb:t101:2:3")->titulo, "Programa de TV"));
  assert(!porId(lote, n, "provider:99")->poster[0]);
  // Cached fallback text must not prevent real metadata after reconnection.
  static CatItem reserva;
  snprintf(reserva.imdb, sizeof reserva.imdb, "tt101:1:3");
  snprintf(reserva.tipo, sizeof reserva.tipo, "series");
  snprintf(reserva.titulo, sizeof reserva.titulo, "Programa de TV");
  snprintf(reserva.poster, sizeof reserva.poster, "https://fixture.invalid/known.jpg");
  cat_definir(&reserva, 1); falhaMeta = 0;
  n = continuarLocal(lote, 12);
  assert(!strcmp(porId(lote, n, "tt101:1:3")->titulo, "IMDb recuperado"));
  // "Trakt only" inherited from the main profile by a profile without Trakt:
  // the account progress must still fill the row, not an empty one.
  fonteTeste = AJ_CWF_TRAKT; traktAtivoTeste = 0; traktFalhouTeste = 1;
  n = montarContinuar(lote, 12);
  assert(n == 5 && porId(lote, n, "tt101:1:3")->progresso == 40);
  prog_esquecer_tudo();
  puts("cwlocal: IMDb/TMDB/Kitsu/provider cache, offline/uncached progress, base+type identity and original thresholds ok");
}
