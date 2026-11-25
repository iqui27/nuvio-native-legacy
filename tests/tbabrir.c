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

// AllDebrid / Real-Debrid stubs
static int adChave = 1, rdChave = 1, cAchou[4], cPronto[4], cAchar[4], cLinks[4], adUp[3], adUpN, adUps, adApag;
static char cId[4][24], adApagIds[4][24], adNovo[3][24], cLinkId[4][24];
int debrid_conta_disponivel(int q) { return q == DEBRID_AD ? adChave : q == DEBRID_RD ? rdChave : 0; }
unsigned debrid_conta_geracao(int q) { (void)q; return 1; }
int debrid_conta_achar(int q, const char *h, char *id, unsigned n, int *pronto) {
  (void)h; cAchar[q]++;
  if (!cAchou[q]) return 0;
  snprintf(id, n, "%s", cId[q]); *pronto = cPronto[q]; return 1;
}
int debrid_conta_link(int q, const char *id, const char *a, uint64_t b, int fi, char *url, unsigned n) {
  (void)a; (void)b; (void)fi; cLinks[q]++; snprintf(cLinkId[q], sizeof cLinkId[q], "%s", id);
  snprintf(url, n, "https://%s.example/dl/%s", q == DEBRID_AD ? "ad" : "rd", id); return 1;
}
int debrid_ad_enviar(const char *h, char *id, unsigned n) {
  int i = adUps < adUpN ? adUps : adUpN - 1;
  (void)h; adUps++;
  snprintf(id, n, "%s", adNovo[i]); return adUp[i];
}
int debrid_ad_apagar(const char *id) { snprintf(adApagIds[adApag++ % 4], 24, "%s", id); return 1; }

#define H "0123456789abcdef0123456789abcdef01234567"
static void zerar(void) {
  nApag = adds = links = achaCache = achaFresco = renovou = 0; addHash[0] = 0;
  naConta = contaItem = contaArq = contaUsenet = depoisItem = depoisSemArquivo = 0; addOk = 1; emCache = 1; checks = 0; contaHash[0] = 0;
  memset(cAchou, 0, sizeof cAchou); memset(cPronto, 0, sizeof cPronto); memset(cAchar, 0, sizeof cAchar);
  memset(cLinks, 0, sizeof cLinks); memset(cId, 0, sizeof cId); memset(cLinkId, 0, sizeof cLinkId);
  adUpN = 1; adUps = adApag = 0; adUp[0] = adUp[1] = adUp[2] = -1; memset(adNovo, 0, sizeof adNovo);
  memset(adApagIds, 0, sizeof adApagIds);
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
  usleep(100000);

  // ---- which service
  s = fonte(H, "https://torrentio.example/alldebrid/k/" H "/1"); assert(tbabrir_servico(&s) == DEBRID_AD);
  s = fonte(H, "https://x/y"); snprintf(s.rotulo, sizeof s.rotulo, "[RD+] Torrentio"); assert(tbabrir_servico(&s) == DEBRID_RD);
  s = fonte(H, "https://x/y"); s.debridServico = DEBRID_PM + 1; assert(tbabrir_servico(&s) == -1);
  s = fonte(H, "https://x/y"); s.debridServico = DEBRID_AD + 1; assert(tbabrir_servico(&s) == DEBRID_AD);
  adChave = 0; assert(tbabrir_servico(&s) == -1); adChave = 1;
  s = fonte(H, "https://x/y"); assert(tbabrir_servico(&s) == -1);
  printf("ok: AllDebrid / Real-Debrid / Premiumize sources told apart\n");

  // AD 1. Stuck magnet, upload ready with a NEW magnet: old one deleted after the answer, new one plays.
  zerar(); cAchou[DEBRID_AD] = 1; cPronto[DEBRID_AD] = 1; snprintf(cId[DEBRID_AD], 24, "111");
  adUp[0] = 1; snprintf(adNovo[0], 24, "222");
  s = fonte(H, NULL); s.debridServico = DEBRID_AD + 1;
  assert(tbabrir_recuperar(&s, s.url));
  assert(esperar(url, sizeof url, m, sizeof m) == 1);
  assert(adUps == 1 && adApag == 1 && !strcmp(adApagIds[0], "111") && !strcmp(url, "https://ad.example/dl/222"));
  printf("ok: AllDebrid stuck magnet replaced by the new ready one\n");

  // AD 2. Same magnet, upload ready: deleted, uploaded again, ready -> plays.
  zerar(); cAchou[DEBRID_AD] = 1; cPronto[DEBRID_AD] = 1; snprintf(cId[DEBRID_AD], 24, "111");
  adUpN = 2; adUp[0] = 1; snprintf(adNovo[0], 24, "111"); adUp[1] = 1; snprintf(adNovo[1], 24, "333");
  assert(tbabrir_recuperar(&s, s.url));
  assert(esperar(url, sizeof url, m, sizeof m) == 1);
  assert(adUps == 2 && adApag == 1 && !strcmp(adApagIds[0], "111") && !strcmp(url, "https://ad.example/dl/333"));
  printf("ok: AllDebrid same magnet ready -> deleted and uploaded again\n");

  // AD 3. Upload NOT ready: nothing deleted, fresh link of the stuck magnet only.
  zerar(); cAchou[DEBRID_AD] = 1; cPronto[DEBRID_AD] = 1; snprintf(cId[DEBRID_AD], 24, "111");
  adUp[0] = 0; snprintf(adNovo[0], 24, "111");
  assert(tbabrir_recuperar(&s, s.url));
  assert(esperar(url, sizeof url, m, sizeof m) == 1 && adApag == 0 && !strcmp(url, "https://ad.example/dl/111"));
  printf("ok: AllDebrid cache not confirmed -> nothing deleted, fresh link only\n");

  // AD 4. Upload refused (no answer): nothing deleted either.
  zerar(); cAchou[DEBRID_AD] = 1; cPronto[DEBRID_AD] = 1; snprintf(cId[DEBRID_AD], 24, "111"); adUp[0] = -1;
  assert(tbabrir_recuperar(&s, s.url));
  assert(esperar(url, sizeof url, m, sizeof m) == 1 && adApag == 0);
  printf("ok: AllDebrid upload refused -> nothing deleted\n");

  // AD 5. Not in the account (or a stale list), not cached: nothing deleted,
  // the upload may have answered with the person's own downloading magnet.
  zerar(); adUp[0] = 0; snprintf(adNovo[0], 24, "444");
  assert(tbabrir_recuperar(&s, s.url));
  assert(esperar(url, sizeof url, m, sizeof m) == -1 && adApag == 0);
  printf("ok: AllDebrid not cached -> nothing deleted (%s)\n", m);

  // AD 6. Not in the account, cached: uploaded and plays.
  zerar(); adUp[0] = 1; snprintf(adNovo[0], 24, "555");
  assert(tbabrir_recuperar(&s, s.url));
  assert(esperar(url, sizeof url, m, sizeof m) == 1 && adApag == 0 && !strcmp(url, "https://ad.example/dl/555"));
  printf("ok: AllDebrid not in the account but cached -> uploaded, plays\n");

  // RD 1. In the account, downloaded: fresh link only, nothing added or deleted.
  zerar(); cAchou[DEBRID_RD] = 1; cPronto[DEBRID_RD] = 1; snprintf(cId[DEBRID_RD], 24, "RDX9");
  s = fonte(H, "https://x/y"); s.debridServico = DEBRID_RD + 1;
  assert(tbabrir_recuperar(&s, s.url));
  assert(esperar(url, sizeof url, m, sizeof m) == 1 && !strcmp(url, "https://rd.example/dl/RDX9") && adUps == 0 && adApag == 0);
  printf("ok: Real-Debrid -> fresh link only\n");

  // RD 2. Not in the account: gives up, nothing added.
  zerar();
  assert(tbabrir_recuperar(&s, s.url));
  assert(esperar(url, sizeof url, m, sizeof m) == -1 && cLinks[DEBRID_RD] == 0);
  printf("ok: Real-Debrid not in the account -> usual failover (%s)\n", m);

  // RD 3. Early lookup hit is used by the recovery.
  zerar(); cAchou[DEBRID_RD] = 1; cPronto[DEBRID_RD] = 1; snprintf(cId[DEBRID_RD], 24, "RDE1");
  { Stream e = fonte("fedcba9876543210fedcba9876543210fedcba99", "https://x/y"); e.debridServico = DEBRID_RD + 1;
    tbabrir_procurar_cedo(&e, "test");
    for (int i = 0; i < 200 && cAchar[DEBRID_RD] == 0; i++) usleep(5000);
    usleep(50000);
    tbabrir_procurar_cedo(&e, "again");
    usleep(50000);
    assert(cAchar[DEBRID_RD] == 1);
    assert(tbabrir_recuperar(&e, e.url));
    assert(esperar(url, sizeof url, m, sizeof m) == 1 && cAchar[DEBRID_RD] == 1 && !strcmp(cLinkId[DEBRID_RD], "RDE1")); }
  printf("ok: Real-Debrid early lookup hit used by the recovery\n");

  printf("PASS: tbabrir recovery and early lookup\n");
  return 0;
}
