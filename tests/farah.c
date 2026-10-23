// Real API fixtures: search must retain the Portuguese cut without an IMDb id.
// Reuse the discovery harness; only HTTP is replaced, catalog/parser are real.
#define main detalheanime_main
#include "detalheanime.c"
#undef main

static char *fixture(const char *name) {
  char path[256];
  snprintf(path, sizeof path, "tests/fixtures/farah/%s.json", name);
  FILE *f = fopen(path, "rb");
  assert(f);
  assert(!fseek(f, 0, SEEK_END));
  long n = ftell(f);
  assert(n >= 0 && !fseek(f, 0, SEEK_SET));
  char *body = malloc((size_t)n + 1);
  assert(body && fread(body, 1, (size_t)n, f) == (size_t)n);
  body[n] = 0;
  fclose(f);
  return body;
}

int main(void) {
  const char *queries[] = {"Meu Nome é Farah", "Meu Nome e Farah", "Adım Farah"};
  desc_alvos_busca_zerar();
  for (int q = 0; q < 3; q++) {
    char name[64];
    snprintf(name, sizeof name, "nuvio_search_%d", q);
    char *body = fixture(name);
    nRotas = 0; metaprov_zerar_pausa();
    rota("catalog.nuvio.tv", body);
    CatItem results[4];
    assert(consultarAlvo(&alvos[1], queries[q], results, 4) == 2);
    assert(!strcmp(results[0].imdb, "tt21874046"));
    assert(!strcmp(results[1].imdb, "tmdb:335918"));
    assert(!strcmp(results[1].titulo, "Meu Nome é Farah"));
    free(body);
  }
  nRotas = 0;
  puts("farah: search retains IMDb and TMDB versions, accented and Turkish titles");
  char *meta = fixture("nuvio_meta_tmdb_335918");
  rota("/meta/series/tmdb:335918.json", meta);
  limparCacheMeta(); metaprov_zerar_pausa();
  catalogoDe("tmdb:335918", "series", "Meu Nome é Farah", "cinemeta");
  abrir();
  assert(cat_n_episodios(0) == 89);
  assert(cat_item(0)->nTemporadas == 2);
  assert(cat_item(0)->temporadas[0] == 1 && cat_item(0)->temporadas[1] == 2);
  assert(!strcmp(cat_item(0)->imdb, "tmdb:335918"));
  assert(!pediu("v3-cinemeta"));
  int counts[3] = {0};
  for (int i = 0; i < 89; i++) {
    const CatEp *e = cat_episodio(0, i);
    assert(e && e->temporada >= 1 && e->temporada <= 2);
    counts[e->temporada]++;
    if (e->temporada == 2 && e->episodio == 89) {
      assert(!strcmp(e->vid, "tmdb:335918:2:89"));
      assert(!strcmp(e->duracao, "45 min"));
    }
  }
  assert(counts[1] == 47 && counts[2] == 42);
  const CatEp *last = cat_episodio(0, 88);
  assert(last->temporada == 2 && last->episodio == 89);
  puts("farah: native detail publishes 89 episodes and both seasons without IMDb");
  // Opening a TMDB credit must also work when external_ids has no IMDb.
  char *tv = fixture("tmdb_tv_335918");
  rota("/tv/335918/external_ids?", tv);
  catalogoDe("tt21874046", "series", "Original", "cinemeta");
  sobTmdb = 335918;
  snprintf(sobTipo, sizeof sobTipo, "series");
  sobTemSemente = 0; sobIndice = -1; sobFioVivo = 1;
  sobSessao = nv_perfil_sessao_ler(); // Same request ownership as pedirTmdb.
  buscarTitulo(NULL);
  assert(sobIndice == 1 && !sobFioVivo);
  assert(!strcmp(cat_item(1)->imdb, "tmdb:335918"));
  epItem = 1; fioEpVivo = 1; buscarEps(NULL);
  assert(cat_n_episodios(1) == 89 && cat_item(1)->nTemporadas == 2);
  puts("farah: on-demand title without external IMDb opens with its native id");
  free(tv); free(meta); nRotas = 0; limparCacheMeta();
  return 0;
}
