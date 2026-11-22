// Exercise real cache/publication paths without network or account secrets.
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "../src/partes.c"

static unsigned account = 1;
static int requests, switchDuringRequest, paginated, lastFresh, usenetMode, usenetRequests;
#define REQUESTED_HASH "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
unsigned debrid_tb_geracao(void) { return account; }
int debrid_tb_disponivel(void) { return 1; }
char *debrid_tb_mylist(int u, int id, int p, int f, int *st) {
  (void)u; (void)id; (void)p; (void)f;
  requests++; lastFresh = f; *st = 200;
  if (switchDuringRequest) account++;
  if (usenetMode) {   // burn-in lookup: the file is a usenet download, not a torrent
    if (u) { usenetRequests++; return strdup("{\"data\":[{\"id\":777,\"name\":\"Show\",\"files\":[{\"id\":3,\"name\":\"Show/Show.S01E01.mkv\",\"short_name\":\"Show.S01E01.mkv\",\"size\":6909147632}]}]}"); }
    return strdup("{\"data\":[{\"id\":5,\"hash\":\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\",\"files\":[{\"id\":1,\"name\":\"Other/Other.S01E01.mkv\",\"short_name\":\"Other.S01E01.mkv\",\"size\":1234567}]}]}");
  }
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

  // Burn-in (owner 18/11, log 10): a usenet download is found in the usenet list.
  paginated = 0; usenetMode = 1; listaEsquecer();
  { int item = 0, arq = -1, un = -1; char como[72];
    assert(partes_achar_exato("https://addon.test/x", "", "Show.S01E01.mkv", 6909147632ULL, &item, &arq, &un, como, sizeof como));
    assert(item == 777 && arq == 3 && un == 1 && strstr(como, "usenet"));
    assert(usenetRequests == 1);   // the cached pass found it: no fresh lists
    puts("PASS exact lookup finds a usenet download (type usenet)");
    usenetRequests = 0;
    assert(partes_achar_exato("https://addon.test/y", "", "Other.S01E01.mkv", 1234567ULL, &item, &arq, &un, como, sizeof como));
    assert(item == 5 && arq == 1 && un == 0 && usenetRequests == 0);
    puts("PASS a torrent hit never reads the usenet list");
    assert(!partes_achar_exato("https://addon.test/z", "", "Missing.mkv", 42ULL, &item, &arq, &un, como, sizeof como));
    assert(un == 0 && item == 0 && arq == -1);
    puts("PASS a miss on both lists");
    // Log 10: a 40-hex string in the link that is not the file's hash does not
    // block the exact file name + size match...
    assert(partes_achar_exato("https://addon.test/cccccccccccccccccccccccccccccccccccccccc/play", "",
                              "Other.S01E01.mkv", 1234567ULL, &item, &arq, &un, como, sizeof como));
    assert(item == 5 && un == 0 && !strcmp(como, "filename"));
    // ...but the add-on's own hash still wins: a wrong one is not overridden by the name.
    assert(!partes_achar_exato("https://addon.test/z", "dddddddddddddddddddddddddddddddddddddddd",
                               "Other.S01E01.mkv", 1234567ULL, &item, &arq, &un, como, sizeof como));
    puts("PASS an untrusted link hash falls back to name/size; the add-on's hash does not"); }
  return 0;
}
