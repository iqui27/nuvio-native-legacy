// Exercise the real asynchronous worker with offline responses and failed metadata.
#include "../src/listas.c"
#include <assert.h>

int desc_vertudo_n_bruto(void) { return 0; }
int desc_vertudo_item_bruto(int i, CatItem *d) { (void)i; (void)d; return 0; }
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
char *rede_baixar_com(const char *url, int timeout, const char *const *headers) {
  char response[600000];
  const char *kind = strstr(url, "/anime/") ? "anime" : strstr(url, "/shows/") ? "shows" : "movies";
  const char *p = strstr(fixture, tab);
  const char *a = js_array(p + strlen(tab) + 3, NULL, kind);
  const char *end = js_fim(a - 1);
  (void)timeout; (void)headers;
  assert(!strstr(url, "date_from") && !strstr(url, "limit="));
  assert(a && end);
  snprintf(response, sizeof response, "{\"%s\":%.*s}", kind, (int)(end - (a - 1)), a - 1);
  requests++;
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
    int expected = tv ? 1008 : 1003;
    printf("%s %s: shown=%d fixture=%d\n", tab, tv ? "TV+anime" : "movies", lst_itens_n(), expected);
    if (lst_itens_n() != expected || requests != (tv ? 2 : 1)) failures++;
    if (lst_itens_n() == expected) check_library_grid(expected);
    for (int i = 0; i < lst_itens_n(); i++) {
      CatItem item;
      assert(lst_item(i, &item) && item.titulo[0] && item.imdb[0]);
    }
  }
  free(fixture);
  return failures ? 1 : 0;
}
