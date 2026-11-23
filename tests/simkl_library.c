// Exercise the real asynchronous worker with offline responses and failed metadata.
#include "../src/listas.c"
#include <assert.h>

int desc_vertudo_n_bruto(void) { return 0; }
int desc_vertudo_item_bruto(int i, CatItem *d) { (void)i; (void)d; return 0; }
int caca_n_filmes(int i) { (void)i; return 0; }
int caca_filme(int i, int j, CacaFilme *out) { (void)i; (void)j; (void)out; return 0; }
int caca_info(int i, CacaInfo *out) { (void)i; (void)out; return 0; }
void check_library_grid(int expected);
static char *fixture;
static const char *tab;
static int requests;
int perfis_sessao_atual(uint64_t s) { return s == 1; }
int perfis_sessao_travar(uint64_t s) { return s == 1; }
void perfis_sessao_destravar(void) {}
int trakt_enfeitar_lote_sessao(CatItem *v, int n, uint64_t s) {
  (void)v; (void)n; (void)s;
  return 0; // Metadata unavailable must never remove a library title.
}
// The Simkl lists come from the shared library (simkl_lista_corpo): a
// status-filtered slice of the body it already downloaded, no request of
// its own. The fake answers the slice from the fixture, like the library
// would.
char *simkl_lista_corpo(const char *tipo, const char *estado) {
  char response[600000];
  const char *kind = tipo;   // "shows", "anime" or "movies"
  const char *p = strstr(fixture, tab);
  const char *a = js_array(p + strlen(tab) + 3, NULL, kind);
  assert(!strcmp(estado, tab) && p);
  requests++;
  if (!a) {
    // An empty array has no first element for js_array to return.
    assert(js_array_presente(p + strlen(tab) + 3, NULL, kind));
    snprintf(response, sizeof response, "{\"%s\":[]}", kind);
    return strdup(response);
  }
  const char *end = js_fim(a - 1);
  assert(end);
  snprintf(response, sizeof response, "{\"%s\":%.*s}", kind, (int)(end - (a - 1)), a - 1);
  return strdup(response);
}
int main(int argc, char **argv) {
  FILE *f;
  long size;
  int failures = 0;
  const char *tabs[] = {"watching", "plantowatch", "completed", "hold", "dropped"};
  assert(argc == 2 && (f = fopen(argv[1], "rb")));
  fseek(f, 0, SEEK_END); size = ftell(f); rewind(f);
  fixture = malloc((size_t)size + 1);
  assert(fread(fixture, 1, (size_t)size, f) == (size_t)size);
  fixture[size] = 0; fclose(f);
  for (int t = 0; t < 5; t++) for (int tv = 0; tv < 2; tv++) {
    SimklTarefa *task = calloc(1, sizeof *task);
    tab = tabs[t]; requests = 0;
    task->sessao = 1; task->geracao = ++itGeracao;
    snprintf(task->estado, sizeof task->estado, "%s", tab);
    snprintf(task->midia, sizeof task->midia, "%s", tv ? "TV" : "MOVIE");
    itSimkl = 1;
    fioSimkl(task);
    // Movies reads movies + anime (anime_type "movie" joins the tab; the
    // dropped fixture keeps one as its only movie-side entry). TV reads
    // shows + anime (1001 + 7; the feature films stay out).
    int expected = tv ? 1008 : !strcmp(tab, "dropped") ? 1 : 1004;
    printf("%s %s: shown=%d fixture=%d\n", tab, tv ? "TV+anime" : "movies", lst_itens_n(), expected);
    if (lst_itens_n() != expected || requests != 2) failures++;
    if (lst_itens_n() == expected) check_library_grid(expected);
    int animeMovie = 0, dupId = 0;
    for (int i = 0; i < lst_itens_n(); i++) {
      CatItem item;
      assert(lst_item(i, &item) && item.titulo[0] && item.imdb[0]);
      animeMovie += !strcmp(item.imdb, "tt50000");
      dupId += !strcmp(item.imdb, "tt20000");
    }
    // The anime feature is on Movies exactly once, never on TV; a title the
    // fixture lists under both movies and anime shows up a single time.
    if (animeMovie != !tv) failures++;
    if (!tv && (!strcmp(tab, "plantowatch") || !strcmp(tab, "completed")) && dupId != 1) failures++;
    if (!tv && !strcmp(tab, "dropped")) {
      CatItem item;
      assert(lst_item(0, &item) && !strcmp(item.imdb, "tt50000") && !strcmp(item.tipo, "movie"));
    }
  }
  free(fixture);
  return failures ? 1 : 0;
}
