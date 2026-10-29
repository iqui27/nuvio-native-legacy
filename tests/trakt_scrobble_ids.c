// #512: numeric episode ids from addon meta must survive into the scrobble body.
// idsScrobble used to point at a stack MetaEpInfo that dies on return.
#include "traktscrobble.h"
#include "metaep.h"
#include "js.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *ler(const char *nome) {
  char path[200]; FILE *f; char *b; long n;
  snprintf(path, sizeof path, "tests/fixtures/%s", nome);
  f = fopen(path, "rb"); assert(f);
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)n + 1); assert(fread(b, 1, (size_t)n, f) == (size_t)n);
  b[n] = 0; fclose(f); return b;
}

static void ids_do_meta(const char *meta, int t, int e, ScrobbleEpIds *out) {
  MetaEpInfo ep;
  memset(out, 0, sizeof *out);
  assert(metaep_buscar(meta, t, e, &ep));
  scrobble_ids_guardar(out, ep.tvdb, ep.tmdb, ep.trakt);
}

// Parse the JSON objects, rather than accepting an ID anywhere in the body.
// Official contract: episodeIdsRequestSchema supports episode TVDB/Trakt IDs;
// a show IMDb plus season/number remains the fallback when they are absent.
static void episodio(const char *body, int tvdb, int trakt, double pct) {
  char ep[300], ids[220], show[300];
  assert(js_bruto(body, NULL, "episode", ep, sizeof ep));
  assert(js_bruto(ep, NULL, "ids", ids, sizeof ids));
  assert(js_num(ids, NULL, "tvdb", 0) == tvdb);
  assert(js_num(ids, NULL, "trakt", 0) == trakt);
  assert(!js_tem(ids, NULL, "tmdb") && !js_tem(ids, NULL, "imdb"));
  assert(!js_tem(ep, NULL, "season") && !js_tem(ep, NULL, "number"));
  assert(!js_bruto(body, NULL, "show", show, sizeof show));
  assert(js_num(body, NULL, "progress", -1) == pct);
}
static void numerado(const char *body) {
  char ep[220], show[220], ids[160], imdb[64] = "";
  assert(js_bruto(body, NULL, "show", show, sizeof show));
  assert(js_bruto(show, NULL, "ids", ids, sizeof ids));
  assert(js_texto_raiz(ids, "imdb", imdb, sizeof imdb));
  assert(!strcmp(imdb, "tt29422052"));
  assert(!js_tem(ids, NULL, "tvdb") && !js_tem(ids, NULL, "tmdb") && !js_tem(ids, NULL, "trakt"));
  assert(js_bruto(body, NULL, "episode", ep, sizeof ep));
  assert(js_num(ep, NULL, "season", -1) == 2 && js_num(ep, NULL, "number", -1) == 1);
  assert(!js_tem(ep, NULL, "ids") && js_num(body, NULL, "progress", -1) == 41.0);
}

int main(void) {
  char corpo[500];
  ScrobbleEpIds ids;
  char *meta = ler("kaiju_tvdb_s2.json");
  char poison[sizeof(MetaEpInfo)];

  ids_do_meta(meta, 2, 1, &ids);
  memset(poison, 'Z', sizeof poison);
  assert(ids.tvdb && !strcmp(ids.tvdb, "10600123"));
  assert(ids.tmdb && !strcmp(ids.tmdb, "5234501"));
  assert(ids.trakt && !strcmp(ids.trakt, "9923456"));
  scrobble_corpo_ep(corpo, sizeof corpo, "tt29422052:2:1", 41.0, &ids);
  printf("episode identity body: %s\n", corpo); fflush(stdout);
  episodio(corpo, 10600123, 9923456, 41.0);
  puts("ok scrobble_ids: owned metadata IDs identify episode, never show");
  scrobble_ids_limpar(&ids);

  scrobble_ids_guardar(&ids, "10600123", NULL, NULL);
  scrobble_corpo_ep(corpo, sizeof corpo, "tt29422052:2:1", 41.0, &ids);
  episodio(corpo, 10600123, 0, 41.0);
  puts("ok scrobble_ids: TVDB-only episode identity");

  scrobble_ids_guardar(&ids, NULL, NULL, "9923456");
  scrobble_corpo_ep(corpo, sizeof corpo, "tt29422052:2:1", 41.0, &ids);
  episodio(corpo, 0, 9923456, 41.0);
  puts("ok scrobble_ids: Trakt-only episode identity");

  scrobble_ids_guardar(&ids, NULL, "5234501", NULL);
  scrobble_corpo_ep(corpo, sizeof corpo, "tt29422052:2:1", 41.0, &ids);
  numerado(corpo);
  puts("ok scrobble_ids: unsupported TMDB-only identity retains numbered fallback");

  scrobble_ids_limpar(&ids);
  scrobble_corpo_ep(corpo, sizeof corpo, "tt29422052:2:1", 41.0, &ids);
  numerado(corpo);
  scrobble_corpo(corpo, sizeof corpo, "tt29422052:2:1", 41.0);
  numerado(corpo);
  puts("ok scrobble_ids: empty and legacy identities retain show IMDb and source pair");

  scrobble_ids_guardar(&ids, "10600123", NULL, "9923456");
  scrobble_corpo_ep(corpo, sizeof corpo, "tt29422052", 41.0, &ids);
  assert(!strcmp(corpo, "{\"movie\":{\"ids\":{\"imdb\":\"tt29422052\"}},\"progress\":41.00}"));
  puts("ok scrobble_ids: movie body ignores episode IDs");

  scrobble_corpo_ep(corpo, sizeof corpo, "tt29422052:2:1", -1.0, &ids);
  episodio(corpo, 10600123, 9923456, 0.0);
  scrobble_corpo_ep(corpo, sizeof corpo, "tt29422052:2:1", 101.0, &ids);
  episodio(corpo, 10600123, 9923456, 100.0);
  puts("ok scrobble_ids: episode identity preserves progress bounds");

  metaep_scrobble_corpo(corpo, sizeof corpo, "", 2, 1, NULL, 41.0);
  assert(!corpo[0]);
  puts("ok scrobble_ids: absent title remains unsendable");

  scrobble_ids_limpar(&ids);
  {
    const char *especiais = "{\"videos\":[{\"season\":0,\"episode\":2},"
                            "{\"season\":0,\"episode\":1}]}";
    MetaEpInfo especial;
    assert(metaep_buscar(especiais, 0, 1, &especial));
    assert(especial.temporada == 0 && especial.episodio == 1);
    assert(metaep_proximo(especiais, 0, 1, &especial));
    assert(especial.temporada == 0 && especial.episodio == 2);
    scrobble_corpo(corpo, sizeof corpo, "tt12409194:0:1", 41.0);
    assert(strstr(corpo, "\"show\"") && !strstr(corpo, "\"movie\""));
    assert(strstr(corpo, "\"season\":0") && strstr(corpo, "\"number\":1"));
    assert(!metaep_existe(especiais, 0, 0));
  }

  free(meta);
  puts("trakt_scrobble_ids: PASS");
  return 0;
}
