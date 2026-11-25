// Ver tbabrir.h. Owner 25/11/2026: subtitle-rewrite's TorBox start (early
// lookup, stuck item removed and re-added) for the normal, untranscoded
// playback. The TorBox calls are debrid.c's and partes.c's (shared).
#include "tbabrir.h"
#include "debrid.h"
#include "partes.h"
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define TBA_CHAVE 700
#define TBA_URL_MAX 4096
#ifndef TBA_CEDO_VIDA_MS
#define TBA_CEDO_VIDA_MS (30u * 60u * 1000u)   // a hit
#endif
#ifndef TBA_CEDO_ERRO_MS
#define TBA_CEDO_ERRO_MS 60000u                // a miss: no new EARLY lookup of that file
#endif
#ifndef TBA_REGISTRO_MS
#define TBA_REGISTRO_MS 3500u                  // after an add, the next look (owner 24/11)
#endif
#define TBA_CEDO_N 8

static unsigned msAgora(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (unsigned)(t.tv_sec * 1000u + t.tv_nsec / 1000000u);
}

static const char *hashDa(const Stream *s) {
  return s->hashTorrent[0] ? s->hashTorrent : s->infoHash;
}

static int contemSemCaixa(const char *s, const char *agulha) {
  size_t n = strlen(agulha);
  for (; s && *s; s++)
    if (!strncasecmp(s, agulha, n)) return 1;
  return 0;
}

static int servicoUsavel(int q) {
  return q == DEBRID_TB ? debrid_tb_disponivel() > 0 : debrid_conta_disponivel(q) > 0;
}
static const char *nomeDe(int q) {
  return q == DEBRID_TB ? "TorBox" : q == DEBRID_AD ? "AllDebrid" : q == DEBRID_RD ? "Real-Debrid" : "debrid";
}

// Add-on links (AIOStreams, Comet, Torrentio with a debrid key): the service
// is in the link or in the label ("[TB+]", "[AD+]", "[RD+]", "TorBox").
static int marcaDe(const Stream *s, int q) {
  switch (q) {
  case DEBRID_TB:
    return contemSemCaixa(s->url, "torbox") || contemSemCaixa(s->url, "tb-cdn.") ||
           contemSemCaixa(s->rotulo, "torbox") || strstr(s->rotulo, "[TB") || contemSemCaixa(s->provedor, "torbox");
  case DEBRID_AD:
    return contemSemCaixa(s->url, "alldebrid") || contemSemCaixa(s->url, ".debrid.it/") ||
           contemSemCaixa(s->rotulo, "alldebrid") || strstr(s->rotulo, "[AD") ||
           contemSemCaixa(s->provedor, "alldebrid");
  case DEBRID_RD:
    return contemSemCaixa(s->url, "real-debrid") || contemSemCaixa(s->url, "realdebrid") ||
           contemSemCaixa(s->url, "rdeb.io/") || contemSemCaixa(s->rotulo, "real-debrid") ||
           contemSemCaixa(s->rotulo, "realdebrid") || strstr(s->rotulo, "[RD") ||
           contemSemCaixa(s->provedor, "real-debrid") || contemSemCaixa(s->provedor, "realdebrid");
  }
  return 0;
}

int tbabrir_servico(const Stream *s) {
  static const int ordem[3] = { DEBRID_TB, DEBRID_AD, DEBRID_RD };
  if (!s) return -1;
  if (s->debridServico > 0) {
    int q = s->debridServico - 1;   // resolved by Nuvio itself: that service or none
    return (q == DEBRID_TB || q == DEBRID_AD || q == DEBRID_RD) && servicoUsavel(q) ? q : -1;
  }
  for (int i = 0; i < 3; i++)
    if (marcaDe(s, ordem[i])) return servicoUsavel(ordem[i]) ? ordem[i] : -1;
  return -1;
}

int tbabrir_fonte_torbox(const Stream *s) { return tbabrir_servico(s) == DEBRID_TB; }

static unsigned geracaoDe(int q) { return q == DEBRID_TB ? debrid_tb_geracao() : debrid_conta_geracao(q); }

void tbabrir_chave(char *out, unsigned n, const char *url, const char *hash, const char *arquivo, uint64_t bytes) {
  char h[41] = "";
  const char *nome = arquivo ? strrchr(arquivo, '/') : NULL;
  int i;
  if (!out || !n) return;
  out[0] = 0;
  nome = nome ? nome + 1 : arquivo ? arquivo : "";
  if (hash && strlen(hash) == 40) {
    for (i = 0; i < 40 && isxdigit((unsigned char)hash[i]); i++) h[i] = (char)tolower((unsigned char)hash[i]);
    h[i == 40 ? 40 : 0] = 0;
  }
  if (h[0] || nome[0] || bytes) snprintf(out, n, "h:%s|n:%.400s|b:%llu", h, nome, (unsigned long long)bytes);
  else if (url && url[0]) snprintf(out, n, "u:%.*s", (int)n - 3, url);
}

int tbabrir_hash_para_readicionar(const char *hashFonte, const char *hashItem, const char *url, char out[41]) {
  char hs[4][41];
  const char *c = hashFonte && strlen(hashFonte) == 40 ? hashFonte
                : hashItem && strlen(hashItem) == 40 ? hashItem : NULL;
  int i;
  out[0] = 0;
  if (!c && url && partes_hashes_texto(url, hs, 4) > 0) c = hs[0];
  if (!c) return 0;
  for (i = 0; i < 40 && isxdigit((unsigned char)c[i]); i++) out[i] = (char)tolower((unsigned char)c[i]);
  if (i != 40) { out[0] = 0; return 0; }
  out[40] = 0;
  return 1;
}

// ---- early lookup ----------------------------------------------------------
typedef struct {
  char chave[TBA_CHAVE];
  int servico, achou, item, arquivo, usenet, pronto;
  char como[72], hash[41], id[24];   // id: AllDebrid magnet / Real-Debrid torrent
  unsigned em, conta;
} Cedo;
typedef struct {
  char url[TBA_URL_MAX], hash[48], arquivo[512], chave[TBA_CHAVE], porque[48];
  uint64_t bytes;
  int servico, fileIdx;
} CedoPedido;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cedoSinal = PTHREAD_COND_INITIALIZER;
static Cedo cedo[TBA_CEDO_N];
static int cedoProx, cedoRodando, cedoTemPend;
static char cedoAtual[TBA_CHAVE];
static CedoPedido cedoPend;

// With `trava`. A still valid entry of `chave` (a hit; a miss only if `erros`).
static Cedo *cedoAchar(const char *chave, int servico, int erros) {
  unsigned agora = msAgora(), conta = geracaoDe(servico);
  for (int i = 0; i < TBA_CEDO_N; i++) {
    Cedo *c = &cedo[i];
    if (!c->chave[0] || strcmp(c->chave, chave) || c->servico != servico || c->conta != conta) continue;
    if (c->achou ? agora - c->em < TBA_CEDO_VIDA_MS : erros && agora - c->em < TBA_CEDO_ERRO_MS) return c;
  }
  return NULL;
}
static void cedoGuardar(const char *chave, int servico, int achou, int item, int arquivo, int usenet,
                        const char *como, const char *hash, const char *id, int pronto) {
  Cedo *c = NULL;
  for (int i = 0; i < TBA_CEDO_N && !c; i++) if (!strcmp(cedo[i].chave, chave)) c = &cedo[i];
  if (!c) { c = &cedo[cedoProx]; cedoProx = (cedoProx + 1) % TBA_CEDO_N; }
  snprintf(c->chave, sizeof c->chave, "%s", chave);
  c->servico = servico; c->achou = achou; c->item = item; c->arquivo = arquivo; c->usenet = usenet;
  c->pronto = pronto;
  snprintf(c->como, sizeof c->como, "%s", como ? como : "");
  snprintf(c->hash, sizeof c->hash, "%s", hash && strlen(hash) == 40 ? hash : "");
  snprintf(c->id, sizeof c->id, "%s", id ? id : "");
  c->em = msAgora(); c->conta = geracaoDe(servico);
}
static void cedoEsquecer(const char *chave) {
  for (int i = 0; i < TBA_CEDO_N; i++)
    if (cedo[i].chave[0] && !strcmp(cedo[i].chave, chave)) cedo[i].chave[0] = 0;
}
static void cedoLog(const CedoPedido *p, const char *o) {
  const char *b = strrchr(p->arquivo, '/');
  printf("[tbabrir] early lookup (%s, %s): %s | file=%s | size=%llu | hash=%s\n", nomeDe(p->servico), p->porque, o,
         b ? b + 1 : p->arquivo[0] ? p->arquivo : "-", (unsigned long long)p->bytes,
         p->hash[0] ? "from the add-on" : "none");
  fflush(stdout);
}
static void *fioCedo(void *arg) {
  CedoPedido *p = arg;
  for (;;) {
    char como[72] = "", hItem[41] = "", id[24] = "";
    int item = 0, arquivo = -1, usenet = 0, achou, pronto = 0;
    unsigned t0 = msAgora();
    if (p->servico == DEBRID_TB) {
      cedoLog(p, "looking the file up in your TorBox");
      achou = partes_achar_exato(p->url, p->hash, p->arquivo, p->bytes, &item, &arquivo, &usenet, como, sizeof como,
                                 hItem);
      if (achou) printf("[tbabrir] early lookup (%s): found item %d, file %d (by %s) in %u ms\n", p->porque, item,
                        arquivo, como, msAgora() - t0);
    } else {
      // AllDebrid / Real-Debrid: the account list by the torrent's hash.
      cedoLog(p, p->servico == DEBRID_AD ? "looking the torrent up in your AllDebrid"
                                         : "looking the torrent up in your Real-Debrid");
      achou = tbabrir_hash_para_readicionar(p->hash, NULL, p->url, hItem) &&
              debrid_conta_achar(p->servico, hItem, id, sizeof id, &pronto) == 1;
      snprintf(como, sizeof como, "hash");
      if (achou) printf("[tbabrir] early lookup (%s): found %s %s (%s) in %u ms\n", p->porque,
                        p->servico == DEBRID_AD ? "magnet" : "torrent", id, pronto ? "ready" : "not ready",
                        msAgora() - t0);
    }
    if (!achou) printf("[tbabrir] early lookup (%s): not found in %u ms\n", p->porque, msAgora() - t0);
    fflush(stdout);
    pthread_mutex_lock(&trava);
    cedoGuardar(p->chave, p->servico, achou, item, arquivo, usenet, como, hItem, id, pronto);
    cedoAtual[0] = 0;
    if (cedoTemPend) {
      *p = cedoPend; cedoTemPend = 0;
      snprintf(cedoAtual, sizeof cedoAtual, "%s", p->chave);
      pthread_cond_broadcast(&cedoSinal);
      pthread_mutex_unlock(&trava);
      continue;
    }
    cedoRodando = 0;
    pthread_cond_broadcast(&cedoSinal);
    pthread_mutex_unlock(&trava);
    break;
  }
  free(p);
  return NULL;
}

void tbabrir_procurar_cedo(const Stream *s, const char *porque) {
  CedoPedido *p;
  pthread_t f;
  int q = tbabrir_servico(s);
  if (q < 0) return;
  p = calloc(1, sizeof *p);
  if (!p) return;
  p->servico = q; p->fileIdx = s->fileIdx;
  tbabrir_chave(p->chave, sizeof p->chave, s->url, hashDa(s), s->arquivo, s->tamanhoBytes);
  if (!p->chave[0]) { free(p); return; }
  snprintf(p->url, sizeof p->url, "%s", s->url);
  snprintf(p->hash, sizeof p->hash, "%s", hashDa(s));
  snprintf(p->arquivo, sizeof p->arquivo, "%s", s->arquivo);
  snprintf(p->porque, sizeof p->porque, "%s", porque ? porque : "");
  p->bytes = s->tamanhoBytes;
  pthread_mutex_lock(&trava);
  if (cedoAchar(p->chave, q, 1) || (cedoRodando && !strcmp(cedoAtual, p->chave)) ||
      (cedoTemPend && !strcmp(cedoPend.chave, p->chave))) {
    pthread_mutex_unlock(&trava); free(p); return;   // known, just missed, or already asked for
  }
  if (cedoRodando) {
    if (cedoTemPend) cedoLog(&cedoPend, "dropped: a newer one waits instead");
    cedoPend = *p; cedoTemPend = 1;
    pthread_mutex_unlock(&trava);
    cedoLog(p, "waits for the running one");
    free(p);
    return;
  }
  cedoRodando = 1;
  snprintf(cedoAtual, sizeof cedoAtual, "%s", p->chave);
  pthread_mutex_unlock(&trava);
  if (pthread_create(&f, NULL, fioCedo, p) == 0) { pthread_detach(f); return; }
  pthread_mutex_lock(&trava);
  cedoRodando = 0; cedoAtual[0] = 0;
  pthread_cond_broadcast(&cedoSinal);
  pthread_mutex_unlock(&trava);
  free(p);
}

// ---- recovery --------------------------------------------------------------
typedef struct {
  char url[TBA_URL_MAX], hash[48], arquivo[512], chave[TBA_CHAVE];
  uint64_t bytes;
  unsigned geracao;
  int servico, fileIdx;
} Pedido;
static unsigned recGeracao;
static int recEstado = -2;            // tbabrir_resultado
static char recUrl[TBA_URL_MAX], recMotivo[160];

static int vivo(const Pedido *p) {
  int v;
  pthread_mutex_lock(&trava);
  v = p->geracao == recGeracao;
  pthread_mutex_unlock(&trava);
  return v;
}
static int dormirVivo(const Pedido *p, unsigned ms) {
  unsigned t0 = msAgora();
  while (msAgora() - t0 < ms) {
    if (!vivo(p)) return 0;
    usleep(50000);
  }
  return vivo(p);
}
static void publicar(const Pedido *p, int estado, const char *url, const char *motivo) {
  pthread_mutex_lock(&trava);
  if (p->geracao == recGeracao) {
    recEstado = estado;
    snprintf(recUrl, sizeof recUrl, "%s", url ? url : "");
    snprintf(recMotivo, sizeof recMotivo, "%s", motivo ? motivo : "");
  }
  pthread_mutex_unlock(&trava);
  if (estado == 1) printf("[tbabrir] recovery: fresh %s link ready, reopening at the same point\n", nomeDe(p->servico));
  else printf("[tbabrir] recovery: %s\n", motivo);
  fflush(stdout);
}

// The early lookup running for this file: wait for it (up to 20 s) instead of
// asking TorBox for the same lists twice.
static void esperarCedo(const char *chave) {
  unsigned t0 = msAgora();
  pthread_mutex_lock(&trava);
  while (cedoRodando && !strcmp(cedoAtual, chave) && msAgora() - t0 < 20000u) {
    struct timespec ate;
    clock_gettime(CLOCK_REALTIME, &ate);
    ate.tv_nsec += 200000000L;
    if (ate.tv_nsec >= 1000000000L) { ate.tv_sec++; ate.tv_nsec -= 1000000000L; }
    pthread_cond_timedwait(&cedoSinal, &trava, &ate);
  }
  pthread_mutex_unlock(&trava);
}

// After an add: look in fresh lists 3.5 s later, then once more 3.5 s after.
// 1 = found (*item/*arq/*us); *semArquivo = the hash's torrent listed without
// the playing file.
static int olharDepoisDoAdd(Pedido *p, int *item, int *arq, int *us, int *semArquivo) {
  char como[72];
  // 3.5 s, then 2 s more: both fit in the app's 12 s hold (TB_REC_PRAZO_MS).
  for (int vez = 0; vez < 2; vez++) {
    if (!dormirVivo(p, vez ? TBA_REGISTRO_MS * 4 / 7 : TBA_REGISTRO_MS)) return 0;
    partes_listas_renovar();
    if (partes_achar_exato_x(p->url, p->hash, p->arquivo, p->bytes, 1, item, arq, us, como, sizeof como, NULL,
                             semArquivo)) {
      printf("[tbabrir] recovery: TorBox lists the file again (item %d, file %d, by %s)\n", *item, *arq, como);
      fflush(stdout);
      return 1;
    }
    if (*semArquivo) return 0;
  }
  return 0;
}

static void trabalharAD(Pedido *p);
static void trabalharRD(Pedido *p);
static void *trabalhar(void *arg) {
  Pedido *p = arg;
  char como[72] = "", hItem[41] = "", h[41] = "", link[TBA_URL_MAX];
  int item = 0, arq = -1, us = 0, achou = 0, semArquivo = 0, tid;
  unsigned t0 = msAgora();

  if (p->servico == DEBRID_AD || p->servico == DEBRID_RD) {
    if (p->servico == DEBRID_AD) trabalharAD(p); else trabalharRD(p);
    free(p);
    return NULL;
  }
  esperarCedo(p->chave);
  pthread_mutex_lock(&trava);
  { Cedo *c = cedoAchar(p->chave, DEBRID_TB, 0);
    if (c) {
      achou = 1; item = c->item; arq = c->arquivo; us = c->usenet;
      snprintf(como, sizeof como, "%s, early lookup", c->como);
      snprintf(hItem, sizeof hItem, "%s", c->hash);
    }
    cedoEsquecer(p->chave);   // whatever happens now, the item changes
  }
  pthread_mutex_unlock(&trava);
  if (!achou)
    achou = partes_achar_exato_x(p->url, p->hash, p->arquivo, p->bytes, 0, &item, &arq, &us, como, sizeof como,
                                 hItem, &semArquivo);
  if (!vivo(p)) goto fim;
  tbabrir_hash_para_readicionar(p->hash, hItem, p->url, h);
  printf("[tbabrir] recovery: the TorBox file showed no picture; %s%s%s, hash %s\n",
         achou ? "item in your TorBox (" : "not in your TorBox lists", achou ? como : "", achou ? ")" : "",
         h[0] ? "known" : "unknown");
  fflush(stdout);

  if (achou && us) {
    // A usenet download has no hash to add again: a fresh link only.
    if (debrid_tb_link(1, item, arq, link, sizeof link) && link[0]) { publicar(p, 1, link, NULL); goto fim; }
    publicar(p, -1, NULL, "usenet download: TorBox gave no fresh link");
    goto fim;
  }
  if (!h[0]) {
    if (achou && debrid_tb_link(0, item, arq, link, sizeof link) && link[0]) { publicar(p, 1, link, NULL); goto fim; }
    publicar(p, -1, NULL, achou ? "no hash to add again and no fresh link" : "not in your TorBox and no hash to add");
    goto fim;
  }
  if (!achou && semArquivo) {
    publicar(p, -1, NULL, "the hash's torrent is in your TorBox without this file: not added");
    goto fim;
  }
  if (achou) {
    // Never take out of the account a file TorBox no longer has cached: the
    // re-add (cached only) would be refused and the item lost. Then only a
    // fresh link of the item as it is.
    int c = debrid_tb_em_cache(h), ok;
    if (c != 1) {
      printf("[tbabrir] recovery: hash %s on TorBox: the item stays, asking for a fresh link only\n",
             c == 0 ? "not cached" : "cache check had no answer");
      fflush(stdout);
      if (debrid_tb_link(0, item, arq, link, sizeof link) && link[0]) publicar(p, 1, link, NULL);
      else publicar(p, -1, NULL, "TorBox gave no fresh link for the item");
      goto fim;
    }
    ok = debrid_tb_apagar(item);
    printf("[tbabrir] recovery: removing the stuck item %d: %s\n", item, ok ? "done" : "TorBox refused");
    fflush(stdout);
  }
  if (!vivo(p)) goto fim;
  tid = debrid_tb_adicionar(h);
  printf("[tbabrir] recovery: adding the hash again (cached only): %s\n", tid >= 0 ? "TorBox took it" : "refused");
  fflush(stdout);
  if (tid < 0) { publicar(p, -1, NULL, "TorBox did not take the hash (not cached, or the account refused)"); goto fim; }
  partes_listas_renovar();
  item = 0; arq = -1; us = 0; semArquivo = 0;
  if (!olharDepoisDoAdd(p, &item, &arq, &us, &semArquivo)) {
    if (!vivo(p)) goto fim;
    if (semArquivo) {
      // The hash was another torrent: take back what was just added.
      int ok = debrid_tb_apagar(semArquivo);
      printf("[tbabrir] recovery: the added torrent (item %d) lacks the playing file; removed again: %s\n",
             semArquivo, ok ? "done" : "TorBox refused");
      publicar(p, -1, NULL, "the hash was another torrent");
    } else publicar(p, -1, NULL, "TorBox does not list the file 5.5 s after the add");
    goto fim;
  }
  if (!vivo(p)) goto fim;
  if (debrid_tb_link(us, item, arq, link, sizeof link) && link[0]) {
    printf("[tbabrir] recovery took %u ms\n", msAgora() - t0);
    publicar(p, 1, link, NULL);
  } else publicar(p, -1, NULL, "TorBox gave no link for the re-added file");
fim:
  free(p);
  return NULL;
}

// ---- AllDebrid and Real-Debrid ---------------------------------------------
// The early lookup's hit for this file (waited for if it is running), taken
// out of the cache: whatever happens now, the item changes.
static int cedoTomar(const Pedido *p, char *id, unsigned n, int *pronto, char hItem[41]) {
  int achou = 0;
  esperarCedo(p->chave);
  pthread_mutex_lock(&trava);
  { Cedo *c = cedoAchar(p->chave, p->servico, 0);
    if (c) {
      achou = 1; snprintf(id, n, "%s", c->id); *pronto = c->pronto;
      if (hItem) snprintf(hItem, 41, "%s", c->hash);
    }
    cedoEsquecer(p->chave); }
  pthread_mutex_unlock(&trava);
  return achou;
}

// A ready magnet's file: AllDebrid lists the files of a just uploaded cached
// magnet at once, but give it one more look 1.5 s later.
static int adLink(Pedido *p, const char *id, char *link, unsigned n) {
  for (int vez = 0; vez < 2; vez++) {
    if (vez && !dormirVivo(p, 1500)) return 0;
    if (!vivo(p)) return 0;
    if (debrid_conta_link(DEBRID_AD, id, p->arquivo, p->bytes, p->fileIdx, link, n) && link[0]) return 1;
  }
  return 0;
}

// ALLDEBRID (owner 25/11/2026). AllDebrid has no separate cache check: the
// upload's `ready` is its word that the files are on AllDebrid. So the hash is
// uploaded FIRST, and the stuck magnet is deleted only after that answer:
// - in the account, upload ready with a new magnet: the stuck one is deleted,
//   the new one plays;
// - in the account, upload ready on the same magnet: deleted and uploaded
//   again (AllDebrid said ready, so it comes back); not ready the second time:
//   it stays in the account (AllDebrid fetches it) and the app fails over;
// - in the account, upload not ready / refused: nothing is deleted; a fresh
//   unlock of the magnet's file if it is ready;
// - not in the account: uploaded; ready plays, not ready stays (never a
//   delete without `ready`).
static void trabalharAD(Pedido *p) {
  char id[24] = "", novo[24] = "", h[41] = "", hItem[41] = "", link[TBA_URL_MAX];
  int pronto = 0, achou, up;
  unsigned t0 = msAgora();
  achou = cedoTomar(p, id, sizeof id, &pronto, hItem);
  tbabrir_hash_para_readicionar(p->hash, hItem, p->url, h);
  if (!h[0]) { publicar(p, -1, NULL, "AllDebrid: no torrent hash to look the file up by"); return; }
  if (!achou) achou = debrid_conta_achar(DEBRID_AD, h, id, sizeof id, &pronto) == 1;
  if (!vivo(p)) return;
  printf("[tbabrir] recovery: the AllDebrid file showed no picture; %s%s%s\n",
         achou ? "magnet " : "not in your AllDebrid", achou ? id : "", achou ? (pronto ? " (ready)" : " (not ready)") : "");
  fflush(stdout);
  up = debrid_ad_enviar(h, novo, sizeof novo);
  printf("[tbabrir] recovery: AllDebrid upload of the hash: %s%s%s\n",
         up == 1 ? "ready (cached)" : up == 0 ? "not ready" : "refused or no answer",
         novo[0] ? ", magnet " : "", novo);
  fflush(stdout);
  if (!vivo(p)) return;
  if (!achou) {
    if (up == 1 && novo[0] && adLink(p, novo, link, sizeof link)) {
      printf("[tbabrir] recovery took %u ms\n", msAgora() - t0);
      publicar(p, 1, link, NULL); return;
    }
    // Not ready: left in the account. AllDebrid answers an upload of a hash
    // it already holds with that magnet, and a stale or failed list read says
    // "not in the account", so deleting here could remove the person's own
    // download. Only a `ready` answer ever leads to a delete.
    if (up == 0 && novo[0])
      printf("[tbabrir] recovery: magnet %s is not cached; it stays in your AllDebrid, nothing deleted\n", novo);
    if (vivo(p)) publicar(p, -1, NULL, up == 1 ? "AllDebrid gave no link for the uploaded magnet"
                                     : "not in your AllDebrid and not cached there");
    return;
  }
  if (up == 1 && novo[0]) {
    const char *usar = novo;
    if (strcmp(novo, id)) {
      int ok = debrid_ad_apagar(id);
      printf("[tbabrir] recovery: AllDebrid made a new ready magnet %s; the stuck magnet %s removed: %s\n", novo, id,
             ok ? "done" : "AllDebrid refused");
    } else {
      int ok = debrid_ad_apagar(id), up2;
      printf("[tbabrir] recovery: AllDebrid says the magnet is ready; removing the stuck magnet %s: %s\n", id,
             ok ? "done" : "AllDebrid refused");
      fflush(stdout);
      if (!vivo(p)) return;
      if (ok) {
        up2 = debrid_ad_enviar(h, novo, sizeof novo);
        printf("[tbabrir] recovery: uploading the hash again: %s%s%s\n",
               up2 == 1 ? "ready" : up2 == 0 ? "not ready (it stays in your AllDebrid)" : "refused",
               novo[0] ? ", magnet " : "", novo);
        if (up2 != 1 || !novo[0]) {
          if (vivo(p)) publicar(p, -1, NULL, "AllDebrid did not have the magnet ready again");
          return;
        }
      }
    }
    fflush(stdout);
    if (adLink(p, usar, link, sizeof link)) {
      printf("[tbabrir] recovery took %u ms\n", msAgora() - t0);
      publicar(p, 1, link, NULL); return;
    }
    if (vivo(p)) publicar(p, -1, NULL, "AllDebrid gave no link for the re-uploaded magnet");
    return;
  }
  // Not confirmed cached: the magnet stays; a fresh link of its file only.
  printf("[tbabrir] recovery: AllDebrid did not confirm the cache; magnet %s stays, asking for a fresh link only\n", id);
  fflush(stdout);
  if (pronto && adLink(p, id, link, sizeof link)) { publicar(p, 1, link, NULL); return; }
  if (vivo(p)) publicar(p, -1, NULL, pronto ? "AllDebrid gave no fresh link for the magnet"
                                   : "the AllDebrid magnet is not ready and not confirmed cached");
}

// REAL-DEBRID (owner 25/11/2026): no cache check on Real-Debrid, so nothing is
// ever deleted or added here. The torrent in the account (the early lookup's
// hit, else the account list by hash) gives a fresh unrestricted link of the
// playing file; anything else is the app's usual failover.
static void trabalharRD(Pedido *p) {
  char id[24] = "", h[41] = "", hItem[41] = "", link[TBA_URL_MAX];
  int pronto = 0, achou;
  achou = cedoTomar(p, id, sizeof id, &pronto, hItem);
  tbabrir_hash_para_readicionar(p->hash, hItem, p->url, h);
  if (!achou && h[0]) achou = debrid_conta_achar(DEBRID_RD, h, id, sizeof id, &pronto) == 1;
  if (!vivo(p)) return;
  printf("[tbabrir] recovery: the Real-Debrid file showed no picture; %s%s%s (nothing is deleted on Real-Debrid)\n",
         achou ? "torrent " : h[0] ? "not in your Real-Debrid" : "no torrent hash", achou ? id : "",
         achou ? (pronto ? ", downloaded" : ", not downloaded") : "");
  fflush(stdout);
  if (!achou) { publicar(p, -1, NULL, h[0] ? "not in your Real-Debrid" : "Real-Debrid: no torrent hash"); return; }
  if (!pronto) { publicar(p, -1, NULL, "the Real-Debrid torrent is not downloaded"); return; }
  if (debrid_conta_link(DEBRID_RD, id, p->arquivo, p->bytes, p->fileIdx, link, sizeof link) && link[0]) {
    publicar(p, 1, link, NULL); return;
  }
  if (vivo(p)) publicar(p, -1, NULL, "Real-Debrid gave no fresh link for the file");
}

int tbabrir_recuperar(const Stream *s, const char *urlAtual) {
  Pedido *p;
  pthread_t f;
  int q = tbabrir_servico(s);
  if (q < 0) return 0;
  p = calloc(1, sizeof *p);
  if (!p) return 0;
  p->servico = q; p->fileIdx = s->fileIdx;
  snprintf(p->url, sizeof p->url, "%s", urlAtual && urlAtual[0] ? urlAtual : s->url);
  snprintf(p->hash, sizeof p->hash, "%s", hashDa(s));
  snprintf(p->arquivo, sizeof p->arquivo, "%s", s->arquivo);
  p->bytes = s->tamanhoBytes;
  tbabrir_chave(p->chave, sizeof p->chave, s->url, hashDa(s), s->arquivo, s->tamanhoBytes);
  if (!p->hash[0] && !p->arquivo[0] && !p->bytes) {
    char hs[1][41];
    if (partes_hashes_texto(p->url, hs, 1) <= 0) { free(p); return 0; }   // nothing to find the file by
  }
  pthread_mutex_lock(&trava);
  p->geracao = ++recGeracao;
  recEstado = 0; recUrl[0] = recMotivo[0] = 0;
  pthread_mutex_unlock(&trava);
  if (pthread_create(&f, NULL, trabalhar, p) == 0) { pthread_detach(f); return 1; }
  pthread_mutex_lock(&trava);
  recEstado = -2;
  pthread_mutex_unlock(&trava);
  free(p);
  return 0;
}

int tbabrir_resultado(char *url, unsigned n, char *motivo, unsigned mn) {
  int e;
  pthread_mutex_lock(&trava);
  e = recEstado;
  if (e == 1 || e == -1) {
    if (url && n) snprintf(url, n, "%s", recUrl);
    if (motivo && mn) snprintf(motivo, mn, "%s", recMotivo);
    recEstado = -2;
  }
  pthread_mutex_unlock(&trava);
  return e;
}

void tbabrir_cancelar(void) {
  pthread_mutex_lock(&trava);
  recGeracao++;
  recEstado = -2;
  pthread_mutex_unlock(&trava);
}
