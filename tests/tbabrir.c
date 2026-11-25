// tbabrir.c (TorBox file that never shows a picture): the recovery worker and
// the early lookup against stubbed TorBox calls. No network.
#include "tbabrir.h"
#include "debrid.h"
#include "partes.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int apagados[8], nApag, adds, links, achaCache, achaFresco, renovou;
static char addHash[41];
// Scenario knobs
static int temChave = 1, naConta, contaItem, contaArq, contaUsenet, depoisItem, depoisSemArquivo, addOk = 1;
static char contaHash[41];

int debrid_tb_disponivel(void) { return temChave; }
unsigned debrid_tb_geracao(void) { return 1; }
static int emCache = 1, checks;
int debrid_tb_em_cache(const char *h) { (void)h; checks++; return emCache; }
int debrid_tb_apagar(int item) { apagados[nApag++ % 8] = item; return 1; }
int debrid_tb_adicionar(const char *hash) { adds++; snprintf(addHash, sizeof addHash, "%s", hash); return addOk ? 900 : -1; }
int debrid_tb_link(int usenet, int id, int arquivo, char *url, unsigned n) {
  links++; snprintf(url, n, "https://cdn.tb-cdn.st/%s/%d/%d", usenet ? "u" : "t", id, arquivo); return 1;
}
void partes_listas_renovar(void) { renovou++; }
int partes_hashes_texto(const char *t, char out[][41], int max) {
  const char *p = t ? strstr(t, "btih:") : NULL;
  if (!p || max < 1) return 0;
  snprintf(out[0], 41, "%.40s", p + 5); return 1;
}
static int achar(int fresco, int *item, int *arq, int *us, char *como, unsigned n, char *hItem, int *sem) {
  if (sem) *sem = 0;
  if (hItem) hItem[0] = 0;
  if (fresco && adds) {   // after the add
    if (depoisSemArquivo) { if (sem) *sem = depoisSemArquivo; return 0; }
    if (!depoisItem) return 0;
    *item = depoisItem; *arq = 3; *us = 0; snprintf(como, n, "hash"); return 1;
  }
  if (!naConta) return 0;
  *item = contaItem; *arq = contaArq; *us = contaUsenet; snprintf(como, n, "filename");
  if (hItem) snprintf(hItem, 41, "%s", contaHash);
  return 1;
}
int partes_achar_exato(const char *u, const char *h, const char *a, uint64_t b, int *item, int *arq, int *us,
                       char *como, unsigned n, char *hItem) {
  (void)u; (void)h; (void)a; (void)b; achaCache++;
  return achar(0, item, arq, us, como, n, hItem, NULL);
}
int partes_achar_exato_x(const char *u, const char *h, const char *a, uint64_t b, int soFresco, int *item, int *arq,
                         int *us, char *como, unsigned n, char *hItem, int *sem) {
  (void)u; (void)h; (void)a; (void)b;
  if (soFresco) achaFresco++; else achaCache++;
  return achar(soFresco, item, arq, us, como, n, hItem, sem);
}

#define H "0123456789abcdef0123456789abcdef01234567"
static void zerar(void) {
  nApag = adds = links = achaCache = achaFresco = renovou = 0; addHash[0] = 0;
  naConta = contaItem = contaArq = contaUsenet = depoisItem = depoisSemArquivo = 0; addOk = 1; emCache = 1; checks = 0; contaHash[0] = 0;
}
static int esperar(char *url, unsigned n, char *m, unsigned mn) {
  for (int i = 0; i < 400; i++) { int r = tbabrir_resultado(url, n, m, mn); if (r != 0) return r; usleep(10000); }
  return 0;
}
static Stream fonte(const char *hash, const char *url) {
  Stream s; memset(&s, 0, sizeof s);
  snprintf(s.url, sizeof s.url, "%s", url ? url : "https://aio.example/playback/torbox/abc");
  snprintf(s.infoHash, sizeof s.infoHash, "%s", hash ? hash : "");
  snprintf(s.arquivo, sizeof s.arquivo, "Show.S02E05.mkv");
  s.tamanhoBytes = 2000000000ULL;
  return s;
}

int main(void) {
  char url[512], m[160];
  Stream s;
  // Which sources are TorBox's.
  s = fonte(H, NULL); assert(tbabrir_fonte_torbox(&s));
  s = fonte(H, "https://real-debrid.example/d/x"); assert(!tbabrir_fonte_torbox(&s));
  snprintf(s.rotulo, sizeof s.rotulo, "[TB+] Torrentio"); assert(tbabrir_fonte_torbox(&s));
  s = fonte(H, "https://x/y"); s.debridServico = DEBRID_TB + 1; assert(tbabrir_fonte_torbox(&s));
  temChave = 0; assert(!tbabrir_fonte_torbox(&s)); temChave = 1;
  { char o[41];
    assert(tbabrir_hash_para_readicionar(H, NULL, NULL, o) && !strcmp(o, H));
    assert(tbabrir_hash_para_readicionar("", "ABCDEF0123456789ABCDEF0123456789ABCDEF01", NULL, o) &&
           !strcmp(o, "abcdef0123456789abcdef0123456789abcdef01"));
    assert(tbabrir_hash_para_readicionar("", "", "magnet:?xt=urn:btih:" H, o) && !strcmp(o, H));
    assert(!tbabrir_hash_para_readicionar("", "", "https://x/y", o)); }

  // 1. Stuck item in the account: removed, re-added, fresh link of the new item.
  zerar(); naConta = 1; contaItem = 55; contaArq = 2; depoisItem = 77;
  s = fonte(H, NULL);
  assert(tbabrir_recuperar(&s, s.url));
  assert(esperar(url, sizeof url, m, sizeof m) == 1);
  assert(nApag == 1 && apagados[0] == 55 && adds == 1 && !strcmp(addHash, H) && renovou >= 1);
  assert(!strcmp(url, "https://cdn.tb-cdn.st/t/77/3"));
  printf("ok: stuck item 55 removed, hash re-added, fresh link of item 77\n");

  // 2. Not in the account, hash known: added (nothing removed), then found.
  zerar(); depoisItem = 80;
  assert(tbabrir_recuperar(&s, s.url));
  assert(esperar(url, sizeof url, m, sizeof m) == 1 && nApag == 0 && adds == 1);
  printf("ok: not in the account -> added by hash, found 3.5 s later\n");

  // 3. Usenet download: only a fresh link, nothing removed or added.
  zerar(); naConta = 1; contaItem = 12; contaArq = 0; contaUsenet = 1;
  assert(tbabrir_recuperar(&s, s.url));
  assert(esperar(url, sizeof url, m, sizeof m) == 1 && nApag == 0 && adds == 0 && strstr(url, "/u/12/0"));
  printf("ok: usenet download -> fresh link only\n");

  // 4. No hash anywhere and not in the account: nothing added.
  zerar();
  { Stream sem = fonte("", "https://aio.example/playback/torbox/abc");
    assert(tbabrir_recuperar(&sem, sem.url));
    assert(esperar(url, sizeof url, m, sizeof m) == -1 && adds == 0 && nApag == 0); }
  printf("ok: no hash and not found -> gives up (%s)\n", m);

  // 5. The hash's torrent comes back without the playing file: removed again.
  zerar(); depoisSemArquivo = 91;
  assert(tbabrir_recuperar(&s, s.url));
  assert(esperar(url, sizeof url, m, sizeof m) == -1 && adds == 1 && nApag == 1 && apagados[0] == 91);
  printf("ok: added torrent without the file -> removed again (%s)\n", m);

  // 6. TorBox refuses the add (not cached): gives up.
  zerar(); naConta = 1; contaItem = 55; addOk = 0;
  assert(tbabrir_recuperar(&s, s.url));
  assert(esperar(url, sizeof url, m, sizeof m) == -1 && links == 0);
  printf("ok: add refused -> gives up (%s)\n", m);

  // 6b. Not cached any more: the item is never deleted, a fresh link only.
  zerar(); naConta = 1; contaItem = 55; contaArq = 2; emCache = 0;
  assert(tbabrir_recuperar(&s, s.url));
  assert(esperar(url, sizeof url, m, sizeof m) == 1 && checks == 1 && nApag == 0 && adds == 0 && strstr(url, "/t/55/2"));
  printf("ok: hash not cached -> item kept, fresh link only\n");

  // 7. Early lookup hit: the recovery uses it (no second cached lookup).
  zerar(); naConta = 1; contaItem = 55; contaArq = 2; depoisItem = 77;
  { Stream e = fonte("fedcba9876543210fedcba9876543210fedcba98", NULL);
    tbabrir_procurar_cedo(&e, "test");
    for (int i = 0; i < 200 && achaCache == 0; i++) usleep(5000);
    usleep(50000);
    int antes = achaCache;
    tbabrir_procurar_cedo(&e, "test again");   // a hit: not looked up twice
    usleep(50000);
    assert(achaCache == antes && antes == 1);
    assert(tbabrir_recuperar(&e, e.url));
    assert(esperar(url, sizeof url, m, sizeof m) == 1 && achaCache == antes && apagados[0] == 55); }
  printf("ok: early lookup hit kept and used by the recovery\n");

  // 8. Cancelled: nothing is published.
  zerar(); naConta = 1; contaItem = 55; depoisItem = 77;
  assert(tbabrir_recuperar(&s, s.url));
  tbabrir_cancelar();
  usleep(200000);
  assert(tbabrir_resultado(url, sizeof url, m, sizeof m) == -2);
  printf("PASS: tbabrir recovery and early lookup\n");
  return 0;
}
