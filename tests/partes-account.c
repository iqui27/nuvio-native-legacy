// Exercise real cache/publication paths without network or account secrets.
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "../src/partes.c"

static unsigned account = 1;
static int requests, switchDuringRequest, paginated, lastFresh;
#define REQUESTED_HASH "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
unsigned debrid_tb_geracao(void) { return account; }
int debrid_tb_disponivel(void) { return 1; }
char *debrid_tb_mylist(int u, int id, int p, int f, int *st) {
  (void)u; (void)id; (void)p; (void)f;
  requests++; lastFresh = f; *st = 200;
  if (switchDuringRequest) account++;
  if (paginated) {
    if (p == 0) {
      char *page = calloc(DEBRID_TB_PAGINA * 160 + 64, 1);
      strcat(page, "{\"data\":[");
      for (int i = 0; i < DEBRID_TB_PAGINA; i++) {
        char entry[160];
        snprintf(entry, sizeof entry, "%s{\"id\":%d,\"files\":[{\"id\":1,\"name\":\"episode.mkv\",\"size\":300000000}]}", i ? "," : "", i + 1);
        strcat(page, entry);
      }
      strcat(page, "]}");
      return page;
    }
    return strdup("{\"data\":[{\"id\":9999,\"hash\":\"" REQUESTED_HASH "\",\"files\":[{\"id\":1,\"name\":\"episode.mkv\",\"size\":300000000}]}]}");
  }
  return strdup("account files");
}
int debrid_tb_link(int u, int id, int a, char *url, unsigned n) {
  (void)u; (void)id; (void)a; (void)url; (void)n; return 0;
}
int debrid_tb_adicionar(const char *h) { (void)h; return 0; }
int debrid_tb_airlock(int u, int id, int l, char *det, unsigned n) { (void)u; (void)id; (void)l; (void)det; (void)n; return 0; }
int dados_gravar(const char *n, const char *c) { (void)n; (void)c; return 0; }
char *dados_ler(const char *n) { (void)n; return NULL; }
char *rede_baixar_trecho64_final(const char *u, const char *h, int64_t a, int64_t b,
    long *s, int64_t *t, int *st, volatile int *c, char *f, int *x) {
  (void)u; (void)h; (void)a; (void)b; (void)s; (void)t; (void)st; (void)c; (void)f; (void)x;
  return NULL;
}
int main(void) {
  int st = 0;
  char *r = listaConta(0, 0, 0, &st, NULL);
  assert(r && requests == 1); free(r);
  r = listaConta(0, 0, 0, &st, NULL);
  assert(r && requests == 1); free(r);
  account++;
  r = listaConta(0, 0, 0, &st, NULL);
  assert(r && requests == 2); free(r);
  puts("PASS account/key change bypasses account-list cache");
  airlockNaMemoria(0, 42, 1);
  r = listaConta(0, 0, 0, &st, NULL);
  assert(r && requests == 3 && lastFresh == 1); free(r);
  r = listaConta(0, 0, 0, &st, NULL);
  assert(r && requests == 3); free(r);
  puts("PASS Airlock edit bypasses server cache once, then reuses the fresh list");

  static Stream source;
  static PartesLista list, out;
  strcpy(source.url, "https://addon.test/same");
  PedidoLista *pd = novoPedido(&source, "title", "source");
  assert(pd);
  pd->geracao = geracaoLista;
  list.estado = PARTES_PRONTO; list.item = 42;
  publicar(pd, &list);
  assert(memoPegarSemTrava("source", &out));
  account++;
  assert(!memoPegarSemTrava("source", &out));
  assert(partes_resultado(&out) == PARTES_VAZIO);
  publicar(pd, &list);
  assert(!memoPegarSemTrava("source", &out));
  assert(partes_resultado(&out) == PARTES_VAZIO);
  free(pd);
  puts("PASS account/key change rejects memo, visible result and stale publication");

  listaEsquecer(); switchDuringRequest = 1;
  r = listaConta(0, 0, 0, &st, NULL);
  assert(!r && st == 0);
  switchDuringRequest = 0;
  int before = requests;
  r = listaConta(0, 0, 0, &st, NULL);
  assert(r && requests == before + 1); free(r);
  listaEsquecer();
  puts("PASS account switch during network rejects response and cache publication");
  paginated = 1;
  strcpy(source.infoHash, REQUESTED_HASH);
  strcpy(source.arquivo, "episode.mkv");
  source.tamanhoBytes = 300000000;
  pd = novoPedido(&source, "title", "paginated");
  assert(pd);
  pd->geracao = geracaoLista;
  before = requests;
  fioLista(pd);
  assert(partes_resultado(&out) == PARTES_PRONTO);
  assert(out.item == 9999 && out.exato && requests == before + 2);
  puts("PASS explicit hash reaches page 2 despite identical filename and size on page 1");
  return 0;
}
