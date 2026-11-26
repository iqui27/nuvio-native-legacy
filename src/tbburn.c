// TorBox burn-in: see tbburn.h.
#include "tbburn.h"
#include "ajustes.h"
#include "debrid.h"
#include "partes.h"
#include "rede.h"
#include "dados.h"
#include "linguas.h"
#include "perfis.h"
#include <pthread.h>
#include <stdatomic.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#define TBB_URL_MAX 4096

// ---- pure ------------------------------------------------------------------

// Copies the JSON string that starts at `q` (just after its opening quote) to
// `out`, unescaping \/ \" \\ (and \uXXXX as '?'). Returns a pointer past the
// closing quote, or NULL when the string never closes.
static const char *cadeia(const char *q, char *out, unsigned n) {
  unsigned k = 0;
  while (*q && *q != '"') {
    char c = *q++;
    if (c == '\\' && *q) {
      char e = *q++;
      c = e == 'n' ? '\n' : e == 't' ? '\t' : e == 'r' ? '\r' : e;
      if (e == 'u') { int i; for (i = 0; i < 4 && *q; i++) q++; c = '?'; }
    }
    if (out && k + 1 < n) out[k++] = c;
  }
  if (out && n) out[k < n ? k : n - 1] = 0;
  return *q == '"' ? q + 1 : NULL;
}

int tbburn_json_texto(const char *corpo, const char *chave, char *out, unsigned n) {
  char pad[96];
  const char *p;
  if (out && n) out[0] = 0;
  if (!corpo || !chave || !out || n < 2) return 0;
  snprintf(pad, sizeof pad, "\"%s\"", chave);
  for (p = strstr(corpo, pad); p; p = strstr(p + 1, pad)) {
    const char *v = p + strlen(pad);
    while (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r') v++;
    if (*v != ':') continue;
    v++;
    while (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r') v++;
    if (*v != '"') continue;
    if (cadeia(v + 1, out, n) && out[0]) return 1;
  }
  out[0] = 0;
  return 0;
}

/* A JSON true/false value (log 15, owner 23/11/2026: createstream's
 * needs_transcoding / is_transcoding). 1 true, 0 false, -1 absent. */
int tbburn_json_bool(const char *corpo, const char *chave) {
  char pad[96];
  const char *p;
  if (!corpo || !chave) return -1;
  snprintf(pad, sizeof pad, "\"%s\"", chave);
  for (p = strstr(corpo, pad); p; p = strstr(p + 1, pad)) {
    const char *v = p + strlen(pad);
    while (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r') v++;
    if (*v != ':') continue;
    v++;
    while (*v == ' ' || *v == '\t' || *v == '\n' || *v == '\r') v++;
    if (!strncmp(v, "true", 4)) return 1;
    if (!strncmp(v, "false", 5)) return 0;
  }
  return -1;
}

int tbburn_achar_m3u8(const char *corpo, char *out, unsigned n) {
  const char *p;
  char *tmp;
  if (out && n) out[0] = 0;
  if (!corpo || !out || n < 16) return 0;
  tmp = malloc(TBB_URL_MAX);
  if (!tmp) return 0;
  // Every JSON string value, in order; the first that is an http(s) URL with
  // ".m3u8" in its path wins.
  for (p = strchr(corpo, '"'); p; ) {
    const char *fim = cadeia(p + 1, tmp, TBB_URL_MAX);
    if (!fim) break;
    if ((!strncmp(tmp, "https://", 8) || !strncmp(tmp, "http://", 7))) {
      const char *q = strstr(tmp, ".m3u8");
      const char *query = strchr(tmp, '?');
      if (q && (!query || q < query)) {
        snprintf(out, n, "%s", tmp);
        free(tmp);
        return 1;
      }
    }
    p = strchr(fim, '"');
  }
  // Not JSON (a plain-text answer): a bare URL. A JSON body never gets here.
  for (p = corpo; *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'; p++) {}
  if (*p != '{' && *p != '[' && (p = strstr(corpo, "http")) != NULL) {
    size_t k = strcspn(p, " \t\r\n\"'<>");
    if (k < TBB_URL_MAX) {
      const char *q, *query;
      memcpy(tmp, p, k); tmp[k] = 0;
      q = strstr(tmp, ".m3u8"); query = strchr(tmp, '?');
      if (q && (!query || q < query) && (!strncmp(tmp, "https://", 8) || !strncmp(tmp, "http://", 7))) {
        snprintf(out, n, "%s", tmp);
        free(tmp);
        return 1;
      }
    }
  }
  free(tmp);
  return 0;
}

int tbburn_eh_item_sumido(int st, const char *r) {
  return (r && strstr(r, "ITEM_NOT_FOUND")) || (st == 404 && r && strstr(r, "\"success\":false"));
}

int tbburn_eh_recusa_plano(int st, const char *corpo) {
  if (corpo && (strstr(corpo, "PLAN_RESTRICTED") || strstr(corpo, "not available on your plan") ||
                strstr(corpo, "only available for Pro") || strstr(corpo, "Pro users") ||
                strstr(corpo, "upgrade your plan"))) return 1;
  return corpo && debrid_eh_sem_plano(st, corpo);
}

// ---- what may go to the log (pure) -------------------------------------------
// [v, fim) becomes `por`; a span shorter than `por` becomes '*' of its own
// length (the text never grows). Returns the end of what was written.
static char *logCortar(char *v, char *fim, const char *por) {
  size_t np = strlen(por);
  if ((size_t)(fim - v) < np) { memset(v, '*', (size_t)(fim - v)); return fim; }
  memmove(v + np, fim, strlen(fim) + 1);
  memcpy(v, por, np);
  return v + np;
}
static int logFimUrl(int c) { return !c || c == '"' || c == ' ' || c == '|' || c == ',' || c == '\n' || c == '\r'; }
// End of a value: a JSON escape (backslash + character) is part of it.
static char *logFimValor(char *q) {
  while (*q && *q != '"' && *q != '&' && *q != '|' && *q != ' ' && *q != ',' && *q != '}' && *q != '\n' && *q != '\r')
    q += (*q == '\\' && q[1]) ? 2 : 1;
  return q;
}
void tbburn_log_mascarar(char *s) {
  char *p;
  if (!s) return;
  // 1. Absolute URLs, plain or JSON-escaped: the host stays, nothing after it.
  for (p = s; *p; ) {
    char *h = NULL, *ini, *a;
    if (!strncasecmp(p, "http://", 7)) h = p + 7;
    else if (!strncasecmp(p, "https://", 8)) h = p + 8;
    else if (!strncasecmp(p, "http:\\/\\/", 9)) h = p + 9;
    else if (!strncasecmp(p, "https:\\/\\/", 10)) h = p + 10;
    if (!h) { p++; continue; }
    ini = h;
    while (*h && *h != '/' && *h != '\\' && *h != '?' && *h != '#' && !logFimUrl((unsigned char)*h)) h++;
    for (a = ini; a < h && *a != '@'; a++) {}
    if (a < h) { char *n = logCortar(ini, a, "***"); h -= a - n; }   // user:password@
    { char *f = h;
      while (!logFimUrl((unsigned char)*f)) f++;
      p = f > h ? logCortar(h, f, "/...") : h; }
  }
  // 2. Query strings of what is left (relative URIs in a playlist).
  for (p = s; *p; p++) {
    char *q;
    if (*p != '?') continue;
    for (q = p + 1; isalnum((unsigned char)*q) || *q == '_' || *q == '-'; q++) {}
    if (q == p + 1 || *q != '=') continue;
    q = p + 1;
    while (!logFimUrl((unsigned char)*q) && *q != '\\') q++;
    p = logCortar(p + 1, q, "***") - 1;
  }
  // 3. Values of anything named ...token (JSON field, query, "token: x").
  for (p = s; *p; ) {
    char *q;
    if (strncasecmp(p, "token", 5)) { p++; continue; }
    q = p + 5;
    if (*q == '"' || *q == '\'') q++;
    while (*q == ' ') q++;
    if (*q != '=' && *q != ':') { p += 5; continue; }
    q++;
    while (*q == ' ' || *q == '"' || *q == '\'') q++;
    p = logCortar(q, logFimValor(q), "***");
  }
}

void tbburn_rota_criar(char *out, unsigned n, int item, int arquivo, int usenet, int sub, int audio) {
  snprintf(out, n, "stream/createstream?id=%d&file_id=%d&type=%s&chosen_subtitle_index=%d"
                   "&chosen_audio_index=%d&scrobbling_enabled=false", item, arquivo, usenet ? "usenet" : "torrent",
           sub, audio < 0 ? 0 : audio);
}

void tbburn_chave(char *out, unsigned n, const char *url, const char *hash, const char *arquivo, uint64_t bytes) {
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
  if (h[0] || nome[0] || bytes) snprintf(out, n, "h:%s|n:%s|b:%llu", h, nome, (unsigned long long)bytes);
  else if (url && url[0]) snprintf(out, n, "u:%s", url);
}

int tbburn_parece_mkv(const char *url, const char *arquivo) {
  static const char *const outros[] = { "mp4", "m4v", "mov", "avi", "ts", "m2ts", "mts", "webm", "wmv", "flv",
                                        "mpg", "mpeg", "iso", "m3u8", "mpd", NULL };
  char nome[512] = "";
  const char *b, *ponto;
  size_t k;
  if (arquivo && arquivo[0]) {
    b = strrchr(arquivo, '/');
    snprintf(nome, sizeof nome, "%s", b ? b + 1 : arquivo);
  } else if (url && url[0]) {
    const char *ini = strstr(url, "://");
    ini = ini ? ini + 3 : url;
    k = strcspn(ini, "?#");
    snprintf(nome, sizeof nome, "%.*s", (int)(k < sizeof nome - 1 ? k : sizeof nome - 1), ini);
    b = strrchr(nome, '/');
    if (!b) return 1;   // a bare host: nothing to judge by
    memmove(nome, b + 1, strlen(b + 1) + 1);
  }
  ponto = strrchr(nome, '.');
  if (!ponto || !ponto[1] || strlen(ponto + 1) > 5) return 1;
  for (int i = 0; outros[i]; i++) if (!strcasecmp(ponto + 1, outros[i])) return 0;
  return 1;
}

// ---- state -----------------------------------------------------------------

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static struct {
  char url[TBB_URL_MAX], hash[48], arquivo[512];
  uint64_t bytes;
} fonte;
// The file found for fonte.url (cached: one account lookup per source).
// hash: the item's info hash from TorBox's list when it was found (owner
// 22/11/2026: the re-add needs it after the item left the list).
static struct { char url[TBB_URL_MAX]; int item, arquivo, usenet, achou, procurou; char como[72], hash[41]; } ids;
static int escolha = -1, temAss, recusado;
static int cedoFontePend;   // tbburn_fonte -> tbburn_cedo_bombear (the early lookup at Play)
static struct {
  int estado;
  unsigned geracao;
  char m3u8[TBB_URL_MAX], motivo[160];
} res;

// The saved last link ("Reuse last link", fontevolta) opens without the add-on's
// list: app.c hands its saved record here first, and an empty tbburn_fonte for
// the same URL takes it (owner's log 12/11: file=- size=0 -> not found).
static struct { char url[TBB_URL_MAX], hash[48], arquivo[512]; uint64_t bytes; } guardada;
void tbburn_fonte_guardada(const char *url, const char *hash, const char *arquivo, uint64_t bytes) {
  pthread_mutex_lock(&trava);
  snprintf(guardada.url, sizeof guardada.url, "%s", url ? url : "");
  snprintf(guardada.hash, sizeof guardada.hash, "%s", hash ? hash : "");
  snprintf(guardada.arquivo, sizeof guardada.arquivo, "%s", arquivo ? arquivo : "");
  guardada.bytes = bytes;
  pthread_mutex_unlock(&trava);
}

void tbburn_fonte(const char *url, const char *hash, const char *arquivo, uint64_t bytes) {
  pthread_mutex_lock(&trava);
  if (url && url[0] && !strcmp(url, guardada.url) && !(hash && hash[0]) && !(arquivo && arquivo[0]) && !bytes) {
    hash = guardada.hash; arquivo = guardada.arquivo; bytes = guardada.bytes;
    printf("[tbburn] source details from the saved last link (Reuse last link): hash %s, file name %s, size %s\n",
           strlen(guardada.hash) == 40 ? "yes" : "none", guardada.arquivo[0] ? "yes" : "none", guardada.bytes ? "yes" : "none");
    fflush(stdout);
  }
  if (!url || strcmp(url, fonte.url)) {
    snprintf(fonte.url, sizeof fonte.url, "%s", url ? url : "");
    snprintf(fonte.hash, sizeof fonte.hash, "%s", hash ? hash : "");
    snprintf(fonte.arquivo, sizeof fonte.arquivo, "%s", arquivo ? arquivo : "");
    fonte.bytes = bytes;
    temAss = 0;
  }
  cedoFontePend = url && url[0];   // tbburn_cedo_bombear, next frame
  pthread_mutex_unlock(&trava);
}

void tbburn_ids(const char *url, int usenet, int item, int arquivo) {
  if (!url || !url[0] || item <= 0 || arquivo < 0) return;
  pthread_mutex_lock(&trava);
  snprintf(ids.url, sizeof ids.url, "%s", url);
  ids.item = item; ids.arquivo = arquivo; ids.usenet = usenet != 0; ids.achou = ids.procurou = 1;
  snprintf(ids.como, sizeof ids.como, "the file picked");
  pthread_mutex_unlock(&trava);
}

int tbburn_recusado(void) { return recusado; }
int tbburn_pode(void) {
  int e = escolha;
  if (e == 0 || debrid_tb_disponivel() == 0) return 0;   // a plan refusal no longer turns it off (owner 19/11)
  if (!ajustes_tb_pro()) return 0;
  return e == 1 || ajustes_leg_queimar();
}
int tbburn_escolha(void) { return escolha; }
void tbburn_definir_escolha(int v) {
  escolha = v < 0 ? -1 : v ? 1 : 0;
  printf("[tbburn] playback row: burn in %s\n", escolha < 0 ? "follows Settings" : escolha ? "On" : "Off");
  fflush(stdout);
}
static int manterProximo;
// The file whose stuck TorBox item was removed and re-added in this playback
// (readicionar, owner 24/11/2026): once per file per playback. Under trava.
static char apagadoUrl[TBB_URL_MAX];
void tbburn_manter_proximo(void) { manterProximo = 1; }
void tbburn_sessao_nova(void) {
  pthread_mutex_lock(&trava); apagadoUrl[0] = 0; pthread_mutex_unlock(&trava);
  tbburn_original_parou();
  if (manterProximo) { manterProximo = 0; return; }
  escolha = -1;
}
void tbburn_arquivo_tem_ass(int sim) { temAss = sim != 0; }
// Owner 19/11: the row stays even when the file was not found in TorBox (a
// pick looks it up again; TorBox's lists can lag behind the account).
int tbburn_linha_visivel(void) {
  return temAss && ajustes_tb_pro() && debrid_tb_disponivel() != 0;
}

// ---- the early lookup (tbburn.h, owner 18/11) ----------------------------------
#define TBB_CHAVE 640
#ifndef TBB_CEDO_VIDA_MS
#define TBB_CEDO_VIDA_MS (30u * 60u * 1000u)   // a hit
#endif
#ifndef TBB_CEDO_ERRO_MS
#define TBB_CEDO_ERRO_MS 60000u                // a miss: no new EARLY lookup of that file
#endif
#ifndef TBB_CEDO_ESPERA_MS
#define TBB_CEDO_ESPERA_MS 20000u              // the worker waits this long for a running one
#endif
#define TBB_CEDO_N 8
typedef struct {
  char chave[TBB_CHAVE];
  int achou, item, arquivo, usenet;
  char como[72], hash[41];   // hash: the item's info hash from TorBox's list ("" = none)
  unsigned em, conta;
} Cedo;
typedef struct {
  char url[TBB_URL_MAX], hash[48], arquivo[512], chave[TBB_CHAVE], porque[48];
  uint64_t bytes;
} CedoPedido;
static Cedo cedo[TBB_CEDO_N];
static int cedoProx;
static pthread_cond_t cedoSinal = PTHREAD_COND_INITIALIZER;
static int cedoRodando, cedoTemPend;
static char cedoAtual[TBB_CHAVE];   // the key being looked up now
static char trabAtual[TBB_CHAVE];   // the key the burn-in request is looking up now (pegarIds)
static CedoPedido cedoPend;         // the newest one waiting (latest wins)

static unsigned msAgora(void);
// With `trava`. A still valid entry of `chave` (a hit; a miss only if `erros`).
static Cedo *cedoAchar(const char *chave, int erros) {
  unsigned agora = msAgora(), conta = debrid_tb_geracao();
  for (int i = 0; i < TBB_CEDO_N; i++) {
    Cedo *c = &cedo[i];
    if (!c->chave[0] || strcmp(c->chave, chave) || c->conta != conta) continue;
    if (c->achou ? agora - c->em < TBB_CEDO_VIDA_MS : erros && agora - c->em < TBB_CEDO_ERRO_MS) return c;
  }
  return NULL;
}
static void cedoGuardar(const char *chave, int achou, int item, int arquivo, int usenet, const char *como,
                        const char *hash) {
  Cedo *c = NULL;
  for (int i = 0; i < TBB_CEDO_N && !c; i++) if (!strcmp(cedo[i].chave, chave)) c = &cedo[i];
  if (!c) { c = &cedo[cedoProx]; cedoProx = (cedoProx + 1) % TBB_CEDO_N; }
  snprintf(c->chave, sizeof c->chave, "%s", chave);
  c->achou = achou; c->item = item; c->arquivo = arquivo; c->usenet = usenet;
  snprintf(c->como, sizeof c->como, "%s", como ? como : "");
  snprintf(c->hash, sizeof c->hash, "%s", hash && strlen(hash) == 40 ? hash : "");
  c->em = msAgora(); c->conta = debrid_tb_geracao();
}
// TorBox no longer has the item a hit pointed to: forget it (with `trava`).
static void cedoEsquecer(const char *chave) {
  for (int i = 0; i < TBB_CEDO_N; i++)
    if (cedo[i].chave[0] && !strcmp(cedo[i].chave, chave)) cedo[i].chave[0] = 0;
}
static void cedoLogPedido(const CedoPedido *p, const char *o) {
  const char *b = strrchr(p->arquivo, '/');
  printf("[tbburn] early lookup (%s): %s | file=%s | size=%llu | hash=%s\n", p->porque, o,
         b ? b + 1 : p->arquivo[0] ? p->arquivo : "-", (unsigned long long)p->bytes, p->hash[0] ? "from the add-on" : "none");
  fflush(stdout);
}
static void *fioCedo(void *arg) {
  CedoPedido *p = arg;
  for (;;) {
    char como[72] = "", hItem[41] = "";
    int item = 0, arquivo = -1, usenet = 0, achou;
    unsigned t0 = msAgora();
    cedoLogPedido(p, "looking the file up in your TorBox");
    achou = partes_achar_exato(p->url, p->hash, p->arquivo, p->bytes, &item, &arquivo, &usenet, como, sizeof como, hItem);
    if (achou) printf("[tbburn] early lookup (%s): found item %d, file %d (by %s) in %u ms; item hash %s%.8s\n", p->porque, item, arquivo,
                      como, msAgora() - t0, hItem[0] ? "" : "none", hItem);
    else printf("[tbburn] early lookup (%s): not found in %u ms (the burn-in request looks again)\n", p->porque, msAgora() - t0);
    fflush(stdout);
    pthread_mutex_lock(&trava);
    cedoGuardar(p->chave, achou, item, arquivo, usenet, como, hItem);
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
  rede_fio_fechar();   // no idle socket left behind by this thread
  return NULL;
}

void tbburn_procurar_cedo(const char *url, const char *hash, const char *arquivo, uint64_t bytes, const char *porque) {
  CedoPedido *p;
  pthread_t f;
  if (!tbburn_pode() || !tbburn_parece_mkv(url, arquivo)) return;
  p = calloc(1, sizeof *p);
  if (!p) return;
  tbburn_chave(p->chave, sizeof p->chave, url, hash, arquivo, bytes);
  if (!p->chave[0]) { free(p); return; }
  snprintf(p->url, sizeof p->url, "%s", url ? url : "");
  snprintf(p->hash, sizeof p->hash, "%s", hash ? hash : "");
  snprintf(p->arquivo, sizeof p->arquivo, "%s", arquivo ? arquivo : "");
  snprintf(p->porque, sizeof p->porque, "%s", porque ? porque : "");
  p->bytes = bytes;
  pthread_mutex_lock(&trava);
  if (cedoAchar(p->chave, 1) || (cedoRodando && !strcmp(cedoAtual, p->chave)) ||
      (cedoTemPend && !strcmp(cedoPend.chave, p->chave))) {
    pthread_mutex_unlock(&trava); free(p); return;   // known, just missed, or already asked for
  }
  if (!strcmp(trabAtual, p->chave)) {   // log 10: both fetched TorBox's 13 MB list at once
    pthread_mutex_unlock(&trava);
    cedoLogPedido(p, "not needed: the burn-in request is looking it up now");
    free(p);
    return;
  }
  if (cedoRodando) {
    if (cedoTemPend) cedoLogPedido(&cedoPend, "dropped: a newer one waits instead");
    cedoPend = *p; cedoTemPend = 1;
    pthread_mutex_unlock(&trava);
    cedoLogPedido(p, "waits for the running one");
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

// The frame after Play: the source tbburn_fonte just set, unless the file is
// already known (a file picked in "Files in source": tbburn_ids).
void tbburn_cedo_bombear(void) {
  char url[TBB_URL_MAX], hash[48], arquivo[512];
  uint64_t bytes;
  pthread_mutex_lock(&trava);
  if (!cedoFontePend) { pthread_mutex_unlock(&trava); return; }
  cedoFontePend = 0;
  if (ids.procurou && ids.achou && !strcmp(ids.url, fonte.url)) {
    pthread_mutex_unlock(&trava);
    printf("[tbburn] early lookup (Play): not needed, the TorBox file is known (%s)\n", ids.como);
    fflush(stdout);
    return;
  }
  snprintf(url, sizeof url, "%s", fonte.url);
  snprintf(hash, sizeof hash, "%s", fonte.hash);
  snprintf(arquivo, sizeof arquivo, "%s", fonte.arquivo);
  bytes = fonte.bytes;
  pthread_mutex_unlock(&trava);
  tbburn_procurar_cedo(url, hash, arquivo, bytes, "Play");
}

// ---- the worker -------------------------------------------------------------

typedef struct {
  char url[TBB_URL_MAX], hash[48], arquivo[512];
  uint64_t bytes;
  int sub, audio;
  unsigned geracao;
  // tbburn_readicionar (owner 22/11/2026): straight to the re-add of item
  // readdItem (TorBox sent no video for it), with its hash from the list.
  int readd, readdItem, readdArq;
  char readdHash[41];
} Pedido;

static unsigned msAgora(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (unsigned)(t.tv_sec * 1000u + t.tv_nsec / 1000000u);
}

// Logs up to ~1500 bytes of a TorBox answer: the key, every ...token value,
// query strings and everything after a URL's host are masked first.
static void logCorpo(const char *rotulo, int st, unsigned ms, const char *r) {
  char *c = malloc(1600);
  if (!c) return;
  snprintf(c, 1600, "%s", r ? r : "(no body)");
  debrid_tb_redigir(c);
  tbburn_log_mascarar(c);
  for (char *p = c; *p; p++) if (*p == '\n' || *p == '\r') *p = ' ';
  printf("[tbburn] %s: HTTP %d in %u ms: %s%s\n", rotulo, st, ms, c, r && strlen(r) >= 1599 ? " (...)" : "");
  fflush(stdout);
  free(c);
}

static int publicar(unsigned geracao, int estado, const char *m3u8, const char *motivo) {
  int ok;
  pthread_mutex_lock(&trava);
  ok = geracao == res.geracao;
  if (ok) {
    res.estado = estado;
    snprintf(res.m3u8, sizeof res.m3u8, "%s", m3u8 ? m3u8 : "");
    snprintf(res.motivo, sizeof res.motivo, "%s", motivo ? motivo : "");
  }
  pthread_mutex_unlock(&trava);
  return ok;
}

/* *sem: the torrent of the add-on's/link's hash is listed without the playing
 * file (its item id), 0 otherwise (owner 24/11/2026: then it isn't added). */
static int pegarIds(const Pedido *p, int *item, int *arquivo, int *usenet, char *como, unsigned n, int *sem) {
  int achou;
  *usenet = 0; *sem = 0;
  pthread_mutex_lock(&trava);
  // Only a FOUND file is reused: a miss is looked up again next time (owner
  // 14/11, Horimiya E07: one miss was reused for every retry).
  if (ids.procurou && ids.achou && !strcmp(ids.url, p->url)) {
    *item = ids.item; *arquivo = ids.arquivo; *usenet = ids.usenet; snprintf(como, n, "%.56s (remembered)", ids.como);
    achou = ids.achou;
    pthread_mutex_unlock(&trava);
    return achou;
  }
  // The early lookup of this file: wait for it if it is running, use its hit.
  {
    char chave[TBB_CHAVE];
    unsigned t0 = msAgora();
    int esperou = 0;
    Cedo *c;
    tbburn_chave(chave, sizeof chave, p->url, p->hash, p->arquivo, p->bytes);
    while (chave[0] && cedoRodando && !strcmp(cedoAtual, chave) && msAgora() - t0 < TBB_CEDO_ESPERA_MS) {
      struct timespec ate;
      clock_gettime(CLOCK_REALTIME, &ate);
      ate.tv_nsec += 200000000L;
      if (ate.tv_nsec >= 1000000000L) { ate.tv_sec++; ate.tv_nsec -= 1000000000L; }
      pthread_cond_timedwait(&cedoSinal, &trava, &ate);
      esperou = 1;
    }
    c = chave[0] ? cedoAchar(chave, 0) : NULL;
    if (c) {
      *item = c->item; *arquivo = c->arquivo; *usenet = c->usenet;
      snprintf(como, n, "%.48s (early lookup)", c->como);
      snprintf(ids.url, sizeof ids.url, "%s", p->url);
      ids.item = c->item; ids.arquivo = c->arquivo; ids.usenet = c->usenet; ids.achou = ids.procurou = 1;
      snprintf(ids.como, sizeof ids.como, "%s", como);
      snprintf(ids.hash, sizeof ids.hash, "%s", c->hash);
      pthread_mutex_unlock(&trava);
      printf("[tbburn] burn-in request: the early lookup had the file (waited %u ms for it)\n", esperou ? msAgora() - t0 : 0u);
      fflush(stdout);
      return 1;
    }
    pthread_mutex_unlock(&trava);
    if (esperou) {
      printf("[tbburn] burn-in request: the early lookup did not find the file (waited %u ms); looking again\n", msAgora() - t0);
      fflush(stdout);
    }
  }
  pthread_mutex_lock(&trava);
  tbburn_chave(trabAtual, sizeof trabAtual, p->url, p->hash, p->arquivo, p->bytes);
  pthread_mutex_unlock(&trava);
  { char hItem[41] = "";
  achou = partes_achar_exato_x(p->url, p->hash, p->arquivo, p->bytes, 0, item, arquivo, usenet, como, n, hItem, sem);
  pthread_mutex_lock(&trava);
  trabAtual[0] = 0;
  snprintf(ids.url, sizeof ids.url, "%s", p->url);
  ids.item = *item; ids.arquivo = *arquivo; ids.usenet = *usenet; ids.achou = achou; ids.procurou = 1;
  snprintf(ids.como, sizeof ids.como, "%s", como);
  snprintf(ids.hash, sizeof ids.hash, "%s", achou ? hItem : "");
  if (achou) {
    char chave[TBB_CHAVE];
    tbburn_chave(chave, sizeof chave, p->url, p->hash, p->arquivo, p->bytes);
    if (chave[0]) cedoGuardar(chave, 1, *item, *arquivo, *usenet, como, hItem);
  } }
  pthread_mutex_unlock(&trava);
  return achou;
}

// The master playlist, once, for the log (codecs, resolution, audio). One
// short request to TorBox's stream host before the player opens it.
static void logPlaylist(const char *m3u8) {
  int st = 0;
  unsigned t0 = msAgora();
  char *r = rede_baixar_st(m3u8, 10, NULL, &st), *c;
  if (!r) { printf("[tbburn] playlist: no answer (HTTP %d)\n", st); fflush(stdout); return; }
  c = malloc(2400);
  if (c) {
    snprintf(c, 2400, "%s", r);
    debrid_tb_redigir(c);
    tbburn_log_mascarar(c);
    for (char *p = c; *p; p++) if (*p == '\n') *p = '|'; else if (*p == '\r') *p = ' ';
    printf("[tbburn] playlist: HTTP %d in %u ms, %u bytes: %s%s\n", st, msAgora() - t0, (unsigned)strlen(r), c,
           strlen(r) >= 2399 ? " (...)" : "");
    fflush(stdout);
    free(c);
  }
  free(r);
}

// ---- re-adding a file that left the account (owner 19/11/2026) ---------------
// Log 12: TorBox's list showed the item, createstream answered ITEM_NOT_FOUND.
// The owner: adding the hash on TorBox's dashboard by hand always fixes it.
// So Nuvio does it: createtorrent with add_only_if_cached (debrid_tb_adicionar;
// never starts a real download), then asks for the stream. The torrent stays
// on the dashboard.
// THE SCHEDULE (owner 24/11/2026, log 24; replaces the four asks of 23/11):
//  1. 3.5 s after the add: look in a fresh list (never one asked for before
//     the add). Found and TorBox answers -> switch (automatic).
//  2. Not listed, or listed but not playable: the original plays on with
//     Nuvio's subtitles, and Nuvio removes the torrent and adds the hash again
//     (once per file per playback).
//  3. 7 s into the original's playback (its first frame, or the first frame
//     after a failed switch; never sooner than 3.5 s after the last add): look
//     again (fresh). Found -> switch. Not -> stop; Burn in asks again.
// The stuck item (TorBox listed it but sent no video) uses the same schedule
// (owner OK 24/11/2026): removed + added first, then steps 1 and 3.
// A torrent of the hash that is listed without the playing file (the link's
// hash was another torrent) is deleted again and the request stops: no loop.
#ifndef TBB_READD_OLHAR_MS
#define TBB_READD_OLHAR_MS 3500u      // step 1: this long after the add
#endif
#ifndef TBB_READD_TOCANDO_MS
#define TBB_READD_TOCANDO_MS 7000u    // step 3: this long into the original's playback
#endif
#ifndef TBB_READD_TOCAR_TETO_MS
#define TBB_READD_TOCAR_TETO_MS 30000u   // playback not started by then: look anyway
#endif
#define TBB_READD_JANELA_MS (2u * TBB_READD_OLHAR_MS + TBB_READD_TOCAR_TETO_MS + TBB_READD_TOCANDO_MS + 5000u)

static int pedidoVivo(const Pedido *p) {
  int v;
  pthread_mutex_lock(&trava); v = res.geracao == p->geracao; pthread_mutex_unlock(&trava);
  return v;
}
/* Sleeps `ms` in short slices; 0 = the request was cancelled meanwhile. */
static int dormirVivo(const Pedido *p, unsigned ms) {
  unsigned t0 = msAgora();
  while (msAgora() - t0 < ms) {
    struct timespec t = { 0, 100000000L };
    if (!pedidoVivo(p)) return 0;
    nanosleep(&t, NULL);
  }
  return pedidoVivo(p);
}

/* The original's playback (video_tpk.c, owner 24/11/2026): when the original
 * file last started showing frames (its first frame, or the first after a
 * failed switch), 0 = not playing. Under trava. */
static struct { char url[TBB_URL_MAX]; unsigned em; } orig;
void tbburn_original_tocando(const char *url) {
  pthread_mutex_lock(&trava);
  snprintf(orig.url, sizeof orig.url, "%s", url ? url : "");
  orig.em = msAgora() ? msAgora() : 1u;
  pthread_mutex_unlock(&trava);
}
void tbburn_original_parou(void) {
  pthread_mutex_lock(&trava); orig.em = 0; orig.url[0] = 0; pthread_mutex_unlock(&trava);
}
static unsigned origTocando(const char *url) {
  unsigned v;
  pthread_mutex_lock(&trava); v = orig.em && !strcmp(orig.url, url) ? orig.em : 0u; pthread_mutex_unlock(&trava);
  return v;
}
/* Waits for step 3's moment: TBB_READD_TOCANDO_MS after the original started
 * playing, never sooner than TBB_READD_OLHAR_MS after `ultimoAdd` (0 = no
 * add). 0 = cancelled. */
static int esperarTocando(Pedido *p, unsigned ultimoAdd, char *quando, unsigned qn) {
  unsigned toc, t0 = msAgora(), alvo, agora;
  while (!(toc = origTocando(p->url)) && msAgora() - t0 < TBB_READD_TOCAR_TETO_MS)
    if (!dormirVivo(p, 200)) return 0;
  if (!toc) {
    printf("[tbburn] the original did not start playing in %u s; looking anyway\n", TBB_READD_TOCAR_TETO_MS / 1000u);
    fflush(stdout);
    toc = msAgora();
  }
  alvo = toc + TBB_READD_TOCANDO_MS;
  if (ultimoAdd && (int)(alvo - (ultimoAdd + TBB_READD_OLHAR_MS)) < 0) alvo = ultimoAdd + TBB_READD_OLHAR_MS;
  agora = msAgora();
  snprintf(quando, qn, "%u s into playback%s", TBB_READD_TOCANDO_MS / 1000u,
           ultimoAdd && alvo == ultimoAdd + TBB_READD_OLHAR_MS ? " (3.5 s after the add)" : "");
  if ((int)(alvo - agora) > 0 && !dormirVivo(p, alvo - agora)) return 0;
  return pedidoVivo(p);
}

/* The file being registered after a re-add (log 12, owner 23/11/2026). A new
 * Burn in press for the same file used to cancel the wait and look the file up
 * in lists that didn't have the new torrent yet ("not in your TorBox"). Now a
 * press joins the wait and asks createstream with the re-added torrent and the
 * file index from the earlier match (a torrent re-added by its hash keeps its
 * file list). Under trava. */
static struct { char url[TBB_URL_MAX]; int torrent, arquivo, usenet; char hash[41]; unsigned em; } reAdd;
static void reAddGuardar(const char *url, int torrent, int arquivo, int usenet, const char *h) {
  pthread_mutex_lock(&trava);
  snprintf(reAdd.url, sizeof reAdd.url, "%s", url);
  reAdd.torrent = torrent; reAdd.arquivo = arquivo; reAdd.usenet = usenet; reAdd.em = msAgora();
  snprintf(reAdd.hash, sizeof reAdd.hash, "%.40s", h ? h : "");
  pthread_mutex_unlock(&trava);
}
static void reAddLimpar(void) {
  pthread_mutex_lock(&trava); reAdd.url[0] = 0; reAdd.torrent = 0; pthread_mutex_unlock(&trava);
}
/* 1 = `url` is being registered (re-added less than the whole wait ago). */
static int reAddPendente(const char *url, int *torrent, int *arquivo, int *usenet, char *h, unsigned *em) {
  int v;
  pthread_mutex_lock(&trava);
  v = reAdd.torrent > 0 && reAdd.url[0] && !strcmp(reAdd.url, url) &&
      msAgora() - reAdd.em < TBB_READD_JANELA_MS;
  if (v) { *torrent = reAdd.torrent; *arquivo = reAdd.arquivo; *usenet = reAdd.usenet; *em = reAdd.em;
           snprintf(h, 41, "%s", reAdd.hash); }
  pthread_mutex_unlock(&trava);
  return v;
}

/* perguntarRegistro's answers. */
enum { REG_CANCELADO = -1, REG_NAO_LISTADO = 0, REG_PRONTO = 1, REG_PLANO = 2, REG_NAO_TOCA = 3, REG_OUTRO_TORRENT = 4 };
/* One look for the added torrent `tid` (hash `h`): createstream with the
 * torrent itself (file `arq`), else a fresh list (never one asked for before
 * this look) by hash/name/size. REG_PRONTO = TorBox answered (*r);
 * REG_PLANO = plan refused (*r); REG_NAO_TOCA = listed (item *i2) but
 * createstream failed; REG_OUTRO_TORRENT = torrent `tid` is listed without the
 * playing file. */
static int perguntarRegistro(Pedido *p, const char *quando, int tid, int arq, int us, const char *h,
                             int *i2, int *f2, int *u2, char *como, unsigned cn, char **r, int *st) {
  char rota[512];
  int sem = 0;
  *r = NULL; *st = 0; *i2 = 0; *f2 = -1; *u2 = 0;
  if (arq >= 0 && tid > 0) {   // the torrent itself: no list needed
    tbburn_rota_criar(rota, sizeof rota, tid, arq, us, p->sub, p->audio);
    *r = debrid_tb_api_get(rota, 30, st);
    logCorpo("createstream", *st, 0, *r);
    if (!pedidoVivo(p)) { free(*r); *r = NULL; return REG_CANCELADO; }
    if (*r && *st >= 200 && *st < 300 && !tbburn_eh_item_sumido(*st, *r)) {
      *i2 = tid; *f2 = arq; *u2 = us; snprintf(como, cn, "re-added torrent");
      printf("[tbburn] %s: torrent %d, file %d: found, TorBox answered\n", quando, tid, arq);
      fflush(stdout);
      return REG_PRONTO;
    }
    if (tbburn_eh_recusa_plano(*st, *r)) return REG_PLANO;
    { int sumido = tbburn_eh_item_sumido(*st, *r);
      printf("[tbburn] %s: torrent %d, file %d: %s (HTTP %d)\n", quando, tid, arq,
             sumido ? "not listed yet" : "no stream, looking in a fresh list", *st);
      fflush(stdout);
      free(*r); *r = NULL;
      if (sumido) return REG_NAO_LISTADO; }   // not registered yet: the lists (13 MB) can't have it either
  }
  partes_listas_renovar();   // log 24: a list from before this look is never reused
  if (!partes_achar_exato_x(p->url, h, p->arquivo, p->bytes, 1, i2, f2, u2, como, cn, NULL, &sem)) {
    if (tid > 0 && sem == tid) {
      printf("[tbburn] %s: torrent %d (hash %.8s...) is in your TorBox, but the playing file is not in it "
             "(the hash is another torrent)\n", quando, tid, h);
      fflush(stdout);
      *i2 = sem;
      return REG_OUTRO_TORRENT;
    }
    printf("[tbburn] %s: not listed in your fresh TorBox lists\n", quando);
    fflush(stdout);
    return REG_NAO_LISTADO;
  }
  tbburn_rota_criar(rota, sizeof rota, *i2, *f2, *u2, p->sub, p->audio);
  *r = debrid_tb_api_get(rota, 30, st);
  logCorpo("createstream", *st, 0, *r);
  if (!pedidoVivo(p)) { free(*r); *r = NULL; return REG_CANCELADO; }
  if (tbburn_eh_recusa_plano(*st, *r)) return REG_PLANO;
  if (!*r || *st < 200 || *st >= 300 || tbburn_eh_item_sumido(*st, *r)) {
    printf("[tbburn] %s: item %d is listed, but not playable (createstream HTTP %d%s)\n", quando, *i2, *st,
           tbburn_eh_item_sumido(*st, *r) ? ", ITEM_NOT_FOUND" : "");
    fflush(stdout);
    free(*r); *r = NULL;
    return REG_NAO_TOCA;
  }
  printf("[tbburn] %s: found item %d, file %d (by %s): TorBox answered\n", quando, *i2, *f2, como);
  fflush(stdout);
  return REG_PRONTO;
}

/* Removes `item` and adds hash `h` again (step 2): once per file per playback.
 * The new torrent id, 0 = not done (already done, or TorBox refused the add),
 * -1 = cancelled. */
static int removerAdicionar(Pedido *p, int item, const char *h) {
  int ja, ok, tid;
  pthread_mutex_lock(&trava); ja = !strcmp(apagadoUrl, p->url); pthread_mutex_unlock(&trava);
  if (ja) {
    printf("[tbburn] this file was already removed and added again in this playback: not again\n");
    fflush(stdout);
    return 0;
  }
  // Never take out of the account a file TorBox no longer has cached: the
  // re-add (cached only) would be refused and the item lost (as tbabrir.c).
  if (item > 0) {
    int c = debrid_tb_em_cache(h);
    if (!pedidoVivo(p)) return -1;
    if (c != 1) {
      printf("[tbburn] remove and add: hash %.8s... %s on TorBox: the item stays in your TorBox\n", h,
             c == 0 ? "not cached" : "cache check had no answer");
      fflush(stdout);
      return 0;
    }
  }
  pthread_mutex_lock(&trava); snprintf(apagadoUrl, sizeof apagadoUrl, "%s", p->url); pthread_mutex_unlock(&trava);
  ok = item > 0 ? debrid_tb_apagar(item) : 0;
  if (!pedidoVivo(p)) return -1;
  if (item > 0) printf("[tbburn] remove and add: item %d %s\n", item, ok ? "removed" : "NOT removed (see the controltorrent line): adding anyway");
  tid = debrid_tb_adicionar(h);
  if (!pedidoVivo(p)) return -1;
  if (tid <= 0) printf("[tbburn] remove and add: TorBox did not add hash %.8s... again (see the createtorrent line)\n", h);
  else printf("[tbburn] remove and add: hash %.8s... added again as torrent %d\n", h, tid);
  fflush(stdout);
  return tid > 0 ? tid : 0;
}

/* Steps 1-3 above for torrent `tid` added at `desde` (hash `h`, file `arq`
 * from an earlier match, -1 unknown). `agora` = a Burn in press joined the
 * wait: it looks once now, then only the looks still to come. `podeRemover` =
 * step 2 may remove + add (not after a stuck item was already removed). 1 = a
 * createstream answer is in *r / *st (the caller goes on as usual); 0 = no:
 * `motivo` says why; -1 = cancelled. */
static int esperarRegistro(Pedido *p, int tid, int arq, int us, const char *h, unsigned desde, int agora, int podeRemover,
                           int *item, int *arquivo, int *usenet, char **r, int *st, char *motivo, unsigned mn) {
  char como[72] = "", quando[64];
  int i2 = 0, f2 = -1, u2 = 0, v = REG_NAO_LISTADO;
  unsigned ultimoAdd = desde;
  *r = NULL; *st = 0;
  if (agora) {
    v = perguntarRegistro(p, "your press", tid, arq, us, h, &i2, &f2, &u2, como, sizeof como, r, st);
    if (v == REG_CANCELADO) return -1;
    if (v == REG_PLANO) return 1;   // the caller reports the plan refusal
    if (v == REG_PRONTO) goto achou;
  }
  // Step 1: 3.5 s after the add (skipped when a press comes later than that).
  if (!agora || (int)(desde + TBB_READD_OLHAR_MS - msAgora()) > 0) {
    unsigned alvo = desde + TBB_READD_OLHAR_MS, t = msAgora();
    if ((int)(alvo - t) > 0 && !dormirVivo(p, alvo - t)) return -1;
    if (!pedidoVivo(p)) return -1;
    v = perguntarRegistro(p, "3.5 s after the add", tid, arq, us, h, &i2, &f2, &u2, como, sizeof como, r, st);
    if (v == REG_CANCELADO) return -1;
    if (v == REG_PLANO) return 1;
    if (v == REG_PRONTO) goto achou;
  }
  if (v == REG_OUTRO_TORRENT) {   // owner OK 24/11/2026: delete it again and stop, no loop
    int ok = debrid_tb_apagar(i2);
    printf("[tbburn] hash %.8s... is another torrent: item %d %s; not adding it again. Nuvio draws the subtitle "
           "(pick Burn in to ask again)\n", h, i2, ok ? "removed from your TorBox again" : "NOT removed (see the controltorrent line)");
    fflush(stdout);
    reAddLimpar();
    snprintf(motivo, mn, "the hash is another torrent");
    return 0;
  }
  // Step 2: the original plays on with Nuvio's subtitles; remove + add.
  if (!publicar(p->geracao, TBB_ESPERANDO, NULL, "TorBox is registering the added file")) return -1;
  if (podeRemover) {
    int novo;
    printf("[tbburn] 3.5 s after the add: %s -> the original plays with Nuvio's subtitles; removing and adding "
           "the hash again\n", v == REG_NAO_TOCA ? "not playable" : "not listed");
    fflush(stdout);
    novo = removerAdicionar(p, v == REG_NAO_TOCA && i2 > 0 ? i2 : tid, h);
    if (novo < 0) return -1;
    if (novo > 0) {
      tid = novo; ultimoAdd = msAgora();
      reAddGuardar(p->url, tid, arq, us, h);
    }
  } else {
    printf("[tbburn] 3.5 s after the add: %s -> the original plays with Nuvio's subtitles (already removed and "
           "added in this playback)\n", v == REG_NAO_TOCA ? "not playable" : "not listed");
    fflush(stdout);
  }
  // Step 3: 7 s into playback.
  if (!esperarTocando(p, ultimoAdd, quando, sizeof quando)) return -1;
  v = perguntarRegistro(p, quando, tid, arq, us, h, &i2, &f2, &u2, como, sizeof como, r, st);
  if (v == REG_CANCELADO) return -1;
  if (v == REG_PLANO) return 1;
  if (v == REG_PRONTO) {
    printf("[tbburn] %s: found -> switching\n", quando);
    fflush(stdout);
    goto achou;
  }
  if (v == REG_OUTRO_TORRENT) {
    int ok = debrid_tb_apagar(i2);
    printf("[tbburn] hash %.8s... is another torrent: item %d %s\n", h, i2, ok ? "removed from your TorBox again" : "NOT removed");
    fflush(stdout);
  }
  reAddLimpar();
  printf("[tbburn] %s: still not in your TorBox: Nuvio draws the subtitle (pick Burn in to ask again)\n", quando);
  fflush(stdout);
  snprintf(motivo, mn, "TorBox did not register the added file");
  return 0;
achou:
  *item = i2; *arquivo = f2; *usenet = u2;
  reAddLimpar();
  { char chave[TBB_CHAVE];
    tbburn_chave(chave, sizeof chave, p->url, p->hash, p->arquivo, p->bytes);
    pthread_mutex_lock(&trava);
    snprintf(ids.url, sizeof ids.url, "%s", p->url);
    ids.item = i2; ids.arquivo = f2; ids.usenet = u2; ids.achou = ids.procurou = 1;
    snprintf(ids.como, sizeof ids.como, "re-added (%.40s)", como);
    snprintf(ids.hash, sizeof ids.hash, "%.40s", h);
    if (chave[0]) cedoGuardar(chave, 1, i2, f2, u2, ids.como, h);
    pthread_mutex_unlock(&trava); }
  return 1;
}

/* No hash at all (owner 24/11/2026, step 5): the first look missed; look again
 * (fresh) 7 s into the original's playback. Same results as readicionar. */
static int olharDeNovo(Pedido *p, int semHash, int *item, int *arquivo, int *usenet, char **r, int *st, char *motivo, unsigned mn) {
  char como[72] = "", quando[64], rota[512];
  *r = NULL; *st = 0;
  if (!publicar(p->geracao, TBB_ESPERANDO, NULL, "looking again 7 s into playback")) return -1;
  if (!esperarTocando(p, 0, quando, sizeof quando)) return -1;
  char hItem[41] = "";
  partes_listas_renovar();
  /* semHash: the hash's torrent is listed without this file; looking by it
   * again would stop there, so the file name and size are tried instead. */
  if (!partes_achar_exato_x(p->url, semHash ? "" : p->hash, p->arquivo, p->bytes, 1, item, arquivo, usenet, como, sizeof como,
                            hItem, NULL)) {
    printf("[tbburn] %s: still not in your TorBox: Nuvio draws the subtitle (pick Burn in to ask again)\n", quando);
    fflush(stdout);
    snprintf(motivo, mn, "file not in your TorBox");
    return 0;
  }
  if (!pedidoVivo(p)) return -1;
  printf("[tbburn] %s: found item %d, file %d (by %s) -> switching\n", quando, *item, *arquivo, como);
  fflush(stdout);
  { char chave[TBB_CHAVE];
    tbburn_chave(chave, sizeof chave, p->url, p->hash, p->arquivo, p->bytes);
    pthread_mutex_lock(&trava);
    snprintf(ids.url, sizeof ids.url, "%s", p->url);
    ids.item = *item; ids.arquivo = *arquivo; ids.usenet = *usenet; ids.achou = ids.procurou = 1;
    snprintf(ids.como, sizeof ids.como, "%s", como);
    snprintf(ids.hash, sizeof ids.hash, "%s", hItem);
    if (chave[0]) cedoGuardar(chave, 1, *item, *arquivo, *usenet, como, hItem);
    pthread_mutex_unlock(&trava); }
  tbburn_rota_criar(rota, sizeof rota, *item, *arquivo, *usenet, p->sub, p->audio);
  *r = debrid_tb_api_get(rota, 30, st);
  logCorpo("createstream", *st, 0, *r);
  if (!pedidoVivo(p)) { free(*r); *r = NULL; return -1; }
  return 1;
}

/* The hash to add `url` by: the add-on's (or the saved link's), else the
 * link's own 40 hex (log 24, owner 24/11/2026). "" = none. */
static const char *hashDoLink(const Pedido *p, char out[41]) {
  char hs[1][41];
  out[0] = 0;
  if (strlen(p->hash) == 40) {
    for (int i = 0; i <= 40; i++) out[i] = (char)tolower((unsigned char)p->hash[i]);
    return "the add-on's or the saved link's";
  }
  if (partes_hashes_texto(p->url, hs, 1) == 1) { snprintf(out, 41, "%s", hs[0]); return "the link's"; }
  return "";
}

/* 1 = a createstream answer for the re-added file is in *r / *st (the caller
 * goes on as usual: plan, playlist...); 0 = no: `motivo` says why; -1 = the
 * request was cancelled (nothing to publish). `itemVelho` 0 = the file was in
 * no list at all (then only the add-on's or the link's hash can add it). */
static int readicionar(Pedido *p, int itemVelho, int arqVelho, const char *hashVelho, int *item, int *arquivo, int *usenet,
                       char **r, int *st, char *motivo, unsigned mn) {
  char h[48] = "", como[72] = "";
  const char *de = "";
  unsigned t0;
  int tid, removeu = 0;
  *r = NULL; *st = 0;
  if (strlen(p->hash) == 40) {
    de = hashDoLink(p, h);
  } else if (hashVelho && strlen(hashVelho) == 40) {
    // Owner 22/11/2026 (log 1, YURASUKA): the item had left TorBox's list by
    // the time of the re-add; its hash was kept from the lookup that found it.
    for (int i = 0; i <= 40; i++) h[i] = (char)tolower((unsigned char)hashVelho[i]);
    de = "TorBox's list when the file was found";
  } else if (itemVelho > 0) {
    int i2 = 0, f2 = -1, u2 = 0;
    if (partes_achar_exato_hash(p->url, "", p->arquivo, p->bytes, &i2, &f2, &u2, como, sizeof como, h) && i2 != itemVelho) h[0] = 0;
    if (h[0]) de = "TorBox's list";
  }
  if (!h[0] && !*usenet) de = hashDoLink(p, h);
  if (!h[0]) {
    printf("[tbburn] re-add: no info hash for this file (%s): Nuvio draws the subtitle\n",
           *usenet ? "a usenet download" : itemVelho > 0 ? "TorBox's list gives none" : "the add-on and the link gave none");
    fflush(stdout);
    snprintf(motivo, mn, "file not in your TorBox");
    return 0;
  }
  /* Owner 24/11/2026 (logs 19-20, Fate PMR): an item TorBox keeps listing but
   * whose stream never sends segment 0 is stuck; adding its hash again only
   * gives the same item back. Removing it on the dashboard and re-adding it
   * made TorBox's encoder work. So Nuvio deletes it first: torrents only, once
   * per file per playback. */
  if (p->readd && itemVelho > 0 && !*usenet) {
    int ja;
    pthread_mutex_lock(&trava); ja = !strcmp(apagadoUrl, p->url); pthread_mutex_unlock(&trava);
    if (ja) {
      printf("[tbburn] re-add: item %d is stuck, but this file was already removed and re-added in this playback: "
             "not removing again\n", itemVelho);
      fflush(stdout);
    } else if (debrid_tb_em_cache(h) != 1) {
      // Same rule as removerAdicionar: an uncached file is never removed.
      if (!pedidoVivo(p)) return -1;
      printf("[tbburn] re-add: item %d is stuck, but TorBox does not report hash %.8s... cached: not removing it\n",
             itemVelho, h);
      fflush(stdout);
    } else {
      int ok;
      if (!pedidoVivo(p)) return -1;
      printf("[tbburn] re-add: TorBox sent no video for item %d (still in your TorBox): removing it from your TorBox "
             "first, then adding it again by its hash\n", itemVelho);
      fflush(stdout);
      ok = debrid_tb_apagar(itemVelho);
      if (!pedidoVivo(p)) return -1;
      pthread_mutex_lock(&trava); snprintf(apagadoUrl, sizeof apagadoUrl, "%s", p->url); pthread_mutex_unlock(&trava);
      removeu = 1;
      printf("[tbburn] re-add: item %d %s\n", itemVelho, ok ? "removed" : "NOT removed (see the controltorrent line): adding anyway");
      fflush(stdout);
    }
  }
  printf("[tbburn] %s: adding hash %.8s... (%s; only if TorBox has it cached; it stays on your dashboard)\n",
         p->readd ? "re-add" : itemVelho > 0 ? "not in your TorBox any more" : "start: not in your TorBox", h, de);
  fflush(stdout);
  tid = debrid_tb_adicionar(h);
  if (!pedidoVivo(p)) return -1;
  if (tid <= 0) {
    printf("[tbburn] re-add: TorBox did not add it (not cached, or refused: see the createtorrent line): "
           "Nuvio draws the subtitle\n");
    fflush(stdout);
    snprintf(motivo, mn, "not cached on TorBox");
    return 0;
  }
  /* Log 15 (owner 23/11/2026): TorBox sent no video for an item still in the
   * account; adding its hash gave the same item back, and asking for minutes
   * cannot change that. The original plays (video_tpk.c waits for TorBox's
   * encoder itself). */
  if (p->readd && itemVelho > 0 && tid == itemVelho) {
    printf("[tbburn] re-add: TorBox gave back the same item %d (still in your TorBox): not re-adding\n", tid);
    fflush(stdout);
    snprintf(motivo, mn, "item still in your TorBox");
    return 0;
  }
  printf("[tbburn] added as torrent %d. Looking for it %.1f s after the add, then %u s into playback (the original "
         "plays with Nuvio's subtitles meanwhile)\n", tid, TBB_READD_OLHAR_MS / 1000.0, TBB_READD_TOCANDO_MS / 1000u);
  fflush(stdout);
  t0 = msAgora();
  /* The re-added torrent keeps the file list of the item that left: the
   * file index from that match asks for it directly (no list lag). */
  reAddGuardar(p->url, tid, itemVelho > 0 ? arqVelho : -1, *usenet, h);
  { int ja;
    pthread_mutex_lock(&trava); ja = !strcmp(apagadoUrl, p->url); pthread_mutex_unlock(&trava);
    return esperarRegistro(p, tid, itemVelho > 0 ? arqVelho : -1, *usenet, h, t0, 0, !removeu && !ja && !p->readd,
                           item, arquivo, usenet, r, st, motivo, mn); }
}

static void *trabalhar(void *arg) {
  Pedido *p = arg;
  char como[72] = "", rota[512], m3u8[TBB_URL_MAX], *r = NULL, *log = NULL;
  int item = 0, arquivo = -1, usenet = 0, st = 0, pico = 0;
  unsigned t0 = msAgora(), t;
  if (p->readd) {   // TorBox sent no video for the item (tbburn_readicionar)
    char motivo[96] = "TorBox sent no video";
    int v;
    printf("[tbburn] TorBox sent no video for item %d: re-adding it to your TorBox (hash %s)\n", p->readdItem,
           strlen(p->hash) == 40 ? "from the add-on" : p->readdHash[0] ? "from TorBox's list" : "looked up");
    fflush(stdout);
    v = readicionar(p, p->readdItem, p->readdArq, p->readdHash, &item, &arquivo, &usenet, &r, &st, motivo, sizeof motivo);
    if (v <= 0) {
      if (!v) publicar(p->geracao, TBB_FALHOU, NULL, motivo);
      free(r); free(p);
      return NULL;
    }
    goto resposta;
  }
  { int tid = 0, arq = -1, us = 0; char h[41] = ""; unsigned desde = 0;
    if (reAddPendente(p->url, &tid, &arq, &us, h, &desde)) {
      char motivo[96] = "TorBox is registering the re-added file";
      int v;
      printf("[tbburn] press during registering: joined the wait for torrent %d (re-added %u s ago); asking with it now\n",
             tid, (msAgora() - desde) / 1000u);
      fflush(stdout);
      { int ja;
        pthread_mutex_lock(&trava); ja = !strcmp(apagadoUrl, p->url); pthread_mutex_unlock(&trava);
        v = esperarRegistro(p, tid, arq, us, h, desde, 1, !ja, &item, &arquivo, &usenet, &r, &st, motivo, sizeof motivo); }
      if (v <= 0) {
        if (!v) publicar(p->geracao, TBB_FALHOU, NULL, motivo);
        free(r); free(p);
        return NULL;
      }
      goto resposta;
    } }
  { int sem = 0;
  if (!pegarIds(p, &item, &arquivo, &usenet, como, sizeof como, &sem)) {
    char motivo[96] = "file not in your TorBox", h[41];
    const char *de = hashDoLink(p, h);
    int v;
    /* Owner 24/11/2026 (log 24): not in the list -> add the hash (the add-on's,
     * the saved link's, or the link's own) right away, then look 3.5 s later
     * and 7 s into playback (readicionar). No hash: look again 7 s into
     * playback only. The hash's torrent already listed without this file:
     * adding it does nothing, so the same as no hash. */
    if (sem > 0)
      printf("[tbburn] start: not in your TorBox (%u ms); the hash (%s) is torrent %d in your TorBox, which doesn't "
             "have this file: not adding it; looking again %u s into playback\n", msAgora() - t0, de, sem,
             TBB_READD_TOCANDO_MS / 1000u);
    else if (h[0])
      printf("[tbburn] start: not in your TorBox by an exact match (hash/file name/size) in %u ms: adding hash %.8s... (%s)\n",
             msAgora() - t0, h, de);
    else
      printf("[tbburn] start: not in your TorBox by an exact match (file name/size) in %u ms, and no hash: looking again "
             "%u s into playback\n", msAgora() - t0, TBB_READD_TOCANDO_MS / 1000u);
    fflush(stdout);
    v = h[0] && sem <= 0 ? readicionar(p, 0, -1, NULL, &item, &arquivo, &usenet, &r, &st, motivo, sizeof motivo)
                         : olharDeNovo(p, sem > 0, &item, &arquivo, &usenet, &r, &st, motivo, sizeof motivo);
    if (v <= 0) {
      if (!v) publicar(p->geracao, TBB_FALHOU, NULL, motivo);
      free(r); free(p);
      return NULL;
    }
    goto resposta;
  } }
  printf("[tbburn] TorBox file: item %d, file %d (by %s) in %u ms; asking for subtitle %d, audio %d\n",
         item, arquivo, como, msAgora() - t0, p->sub, p->audio);
  fflush(stdout);
  tbburn_rota_criar(rota, sizeof rota, item, arquivo, usenet, p->sub, p->audio);
  t = msAgora();
  r = debrid_tb_api_get(rota, 30, &st);
  logCorpo("createstream", st, msAgora() - t, r);
  // Log 12 (owner 19/11): TorBox's own cached list still had the item, but
  // createstream answered ITEM_NOT_FOUND (the item was gone from the account).
  // Forget it, look the file up in fresh lists once, and ask again.
  if (tbburn_eh_item_sumido(st, r) && !tbburn_eh_recusa_plano(st, r)) {
    char chave[TBB_CHAVE], como2[72] = "", hVelho[41] = "";
    int item2 = 0, arquivo2 = -1, usenet2 = 0, achou2;
    unsigned t2 = msAgora();
    tbburn_chave(chave, sizeof chave, p->url, p->hash, p->arquivo, p->bytes);
    pthread_mutex_lock(&trava);
    if (!strcmp(ids.url, p->url) && ids.item == item) snprintf(hVelho, sizeof hVelho, "%s", ids.hash);
    if (chave[0]) cedoEsquecer(chave);
    if (!strcmp(ids.url, p->url)) ids.procurou = ids.achou = 0;
    pthread_mutex_unlock(&trava);
    /* Owner 23/11/2026 (logs 17-18: AIOStreams removes torrents from the
     * dashboard): with the hash known, re-add at once instead of first
     * downloading the whole list again (13 MB, ~2.5 s). TorBox gives back
     * the torrent's current id if it's still there under another one. */
    if (strlen(hVelho) == 40 || strlen(p->hash) == 40) {
      char motivo[96] = "file not in your TorBox";
      int v;
      printf("[tbburn] TorBox says item %d is not in your account (ITEM_NOT_FOUND): re-adding it by its hash right away "
             "(no list download)\n", item);
      fflush(stdout);
      free(r); r = NULL;
      v = readicionar(p, item, arquivo, hVelho, &item, &arquivo, &usenet, &r, &st, motivo, sizeof motivo);
      if (v <= 0) {
        if (!v) publicar(p->geracao, TBB_FALHOU, NULL, motivo);
        free(r); free(p);
        return NULL;
      }
      goto resposta;
    }
    printf("[tbburn] TorBox says item %d is not in your account (ITEM_NOT_FOUND): the list was out of date; "
           "looking the file up again in fresh lists\n", item);
    fflush(stdout);
    achou2 = partes_achar_exato_fresco(p->url, p->hash, p->arquivo, p->bytes, &item2, &arquivo2, &usenet2, como2, sizeof como2);
    if (!achou2 || (item2 == item && arquivo2 == arquivo && usenet2 == usenet)) {
      char motivo[96] = "file not in your TorBox";
      int v;
      printf("[tbburn] %s in %u ms: re-adding it to your TorBox\n",
             achou2 ? "the fresh list has the same item TorBox just refused" : "not in your TorBox's fresh lists either", msAgora() - t2);
      fflush(stdout);
      free(r); r = NULL;
      v = readicionar(p, item, arquivo, hVelho, &item, &arquivo, &usenet, &r, &st, motivo, sizeof motivo);
      if (v <= 0) {
        if (!v) publicar(p->geracao, TBB_FALHOU, NULL, motivo);
        free(r); free(p);
        return NULL;
      }
      goto resposta;
    }
    printf("[tbburn] TorBox file now: item %d, file %d (by %s) in %u ms; asking again\n", item2, arquivo2, como2, msAgora() - t2);
    fflush(stdout);
    item = item2; arquivo = arquivo2; usenet = usenet2;
    pthread_mutex_lock(&trava);
    snprintf(ids.url, sizeof ids.url, "%s", p->url);
    ids.item = item; ids.arquivo = arquivo; ids.usenet = usenet; ids.achou = ids.procurou = 1;
    snprintf(ids.como, sizeof ids.como, "%s", como2);
    ids.hash[0] = 0;
    if (chave[0]) cedoGuardar(chave, 1, item, arquivo, usenet, como2, NULL);
    pthread_mutex_unlock(&trava);
    free(r);
    tbburn_rota_criar(rota, sizeof rota, item, arquivo, usenet, p->sub, p->audio);
    t = msAgora();
    r = debrid_tb_api_get(rota, 30, &st);
    logCorpo("createstream", st, msAgora() - t, r);
  }
resposta:
  if (tbburn_eh_recusa_plano(st, r)) {
    recusado = 1;
    printf("[tbburn] TorBox refused the plan for this request: Nuvio draws the subtitle; burn-in stays on and asks again on the next pick or file\n");
    fflush(stdout);
    publicar(p->geracao, TBB_FALHOU, NULL, "TorBox plan refused");
    free(r); free(p);
    return NULL;
  }
  if (!tbburn_achar_m3u8(r, m3u8, sizeof m3u8)) {
    // No playlist yet: getstreamdata with the tokens createstream gave.
    char pres[600] = "", tok[600] = "";
    if (r && tbburn_json_texto(r, "presigned_token", pres, sizeof pres) &&
        (tbburn_json_texto(r, "token", tok, sizeof tok) || tbburn_json_texto(r, "user_token", tok, sizeof tok))) {
      char *q = malloc(2048);
      free(r); r = NULL;
      if (q) {
        snprintf(q, 2048, "stream/getstreamdata?presigned_token=%s&token=%s&chosen_subtitle_index=%d&chosen_audio_index=%d",
                 pres, tok, p->sub, p->audio < 0 ? 0 : p->audio);
        t = msAgora();
        r = debrid_tb_api_get(q, 30, &st);
        logCorpo("getstreamdata", st, msAgora() - t, r);
        free(q);
      }
    }
    if (!r || !tbburn_achar_m3u8(r, m3u8, sizeof m3u8)) {
      printf("[tbburn] no playlist URL in TorBox's answer: Nuvio draws the subtitle\n");
      fflush(stdout);
      publicar(p->geracao, TBB_FALHOU, NULL, "no playlist from TorBox");
      free(r); free(p);
      return NULL;
    }
  }
  { int nt = tbburn_json_bool(r, "needs_transcoding"), it = tbburn_json_bool(r, "is_transcoding");
    free(r);
    log = malloc(TBB_URL_MAX);
    if (log) {
      snprintf(log, TBB_URL_MAX, "%s", m3u8);
      debrid_tb_redigir(log);
      tbburn_log_mascarar(log);   // the host only
      // Log 15 (owner 23/11/2026): a file TorBox must encode answered slowly.
      printf("[tbburn] stream ready in %u ms (needs_transcoding %s, is_transcoding %s): %s\n", msAgora() - t0,
             nt < 0 ? "?" : nt ? "true" : "false", it < 0 ? "?" : it ? "true" : "false", log);
      fflush(stdout);
      free(log);
    } }
  // Ready first: the player opens now. The playlist read below is for the log
  // only and used to hold the start for one more round trip to TorBox's
  // stream host (up to 10 s), on every burn-in start.
  publicar(p->geracao, TBB_PRONTO, m3u8, NULL);
  logPlaylist(m3u8);
  (void)rede_torbox_em_voo(&pico);
  printf("[tbburn] connections: Nuvio's own to TorBox now %d, most since the last line %d (the TV player is not counted)\n",
         rede_torbox_em_voo(NULL), pico);
  fflush(stdout);
  free(p);
  return NULL;
}

void tbburn_pedir(const char *url, int sub, int audio) {
  Pedido *p = calloc(1, sizeof *p);
  pthread_t f;
  if (!p || !url || !url[0]) { free(p); return; }
  pthread_mutex_lock(&trava);
  snprintf(p->url, sizeof p->url, "%s", url);
  if (!strcmp(url, fonte.url)) {
    snprintf(p->hash, sizeof p->hash, "%s", fonte.hash);
    snprintf(p->arquivo, sizeof p->arquivo, "%s", fonte.arquivo);
    p->bytes = fonte.bytes;
  }
  p->sub = sub; p->audio = audio;
  p->geracao = ++res.geracao;
  res.estado = TBB_BUSCANDO; res.m3u8[0] = res.motivo[0] = 0;
  pthread_mutex_unlock(&trava);
  {
    const char *b = strrchr(p->arquivo, '/');
    printf("[tbburn] asking TorBox: subtitle %d, audio %d | file=%s | size=%llu | hash=%s\n", sub, audio,
           b ? b + 1 : p->arquivo[0] ? p->arquivo : "-", (unsigned long long)p->bytes, p->hash[0] ? "from the add-on" : "none");
    fflush(stdout);
  }
  if (pthread_create(&f, NULL, trabalhar, p) == 0) pthread_detach(f);
  else { publicar(p->geracao, TBB_FALHOU, NULL, "no thread"); free(p); }
}

/* Log 15 (owner 23/11/2026): the item TorBox gave the stream for is in the
 * account (the lookup found it; createstream answered). Re-adding it by its
 * hash only gives the same item back (153924165 there). */
/* The saved link keeps the hash (owner 24/11/2026, log 24): the info hash the
 * TorBox match of `url` used or found (40 hex, lower case). 1 = known. */
int tbburn_hash_achado(const char *url, char out[41]) {
  int v = 0;
  out[0] = 0;
  if (!url || !url[0]) return 0;
  pthread_mutex_lock(&trava);
  if (ids.achou && !strcmp(ids.url, url) && strlen(ids.hash) == 40) { snprintf(out, 41, "%s", ids.hash); v = 1; }
  else if (strlen(fonte.hash) == 40 && !strcmp(fonte.url, url)) {
    for (int i = 0; i <= 40; i++) out[i] = (char)tolower((unsigned char)fonte.hash[i]);
    v = 1;
  }
  pthread_mutex_unlock(&trava);
  return v;
}

int tbburn_item_na_conta(const char *url) {
  int v = 0;
  if (!url || !url[0]) return 0;
  pthread_mutex_lock(&trava);
  if (ids.achou && ids.item > 0 && !strcmp(ids.url, url)) v = ids.item;
  pthread_mutex_unlock(&trava);
  return v;
}

void tbburn_readicionar(const char *url, int sub, int audio) {
  Pedido *p = calloc(1, sizeof *p);
  pthread_t f;
  if (!p || !url || !url[0]) { free(p); return; }
  pthread_mutex_lock(&trava);
  snprintf(p->url, sizeof p->url, "%s", url);
  if (!strcmp(url, fonte.url)) {
    snprintf(p->hash, sizeof p->hash, "%s", fonte.hash);
    snprintf(p->arquivo, sizeof p->arquivo, "%s", fonte.arquivo);
    p->bytes = fonte.bytes;
  }
  p->readd = 1;
  if (ids.achou && !strcmp(ids.url, url)) {
    p->readdItem = ids.item; p->readdArq = ids.arquivo;
    snprintf(p->readdHash, sizeof p->readdHash, "%s", ids.hash);
  }
  { char chave[TBB_CHAVE];   // the next request looks the file up again
    tbburn_chave(chave, sizeof chave, p->url, p->hash, p->arquivo, p->bytes);
    if (chave[0]) cedoEsquecer(chave);
    if (!strcmp(ids.url, url)) ids.procurou = ids.achou = 0; }
  p->sub = sub; p->audio = audio;
  p->geracao = ++res.geracao;
  res.estado = TBB_BUSCANDO; res.m3u8[0] = res.motivo[0] = 0;
  pthread_mutex_unlock(&trava);
  if (pthread_create(&f, NULL, trabalhar, p) == 0) pthread_detach(f);
  else { publicar(p->geracao, TBB_FALHOU, NULL, "no thread"); free(p); }
}

int tbburn_resultado(char *m3u8, unsigned n, char *motivo, unsigned mn) {
  int e;
  pthread_mutex_lock(&trava);
  e = res.estado;
  if (m3u8 && n) snprintf(m3u8, n, "%s", e == TBB_PRONTO ? res.m3u8 : "");
  if (motivo && mn) snprintf(motivo, mn, "%s", res.motivo);
  pthread_mutex_unlock(&trava);
  return e;
}

void tbburn_cancelar(void) {
  pthread_mutex_lock(&trava);
  res.geracao++;
  res.estado = TBB_NADA; res.m3u8[0] = res.motivo[0] = 0;
  pthread_mutex_unlock(&trava);
}

// ---- the burned layer: automatic choice + memory per show -------------------

static int ehLetreiro(const TbFaixa *f) {
  char n[96];
  size_t i;
  if (f->tipo == LING_LEG_LETREIROS || f->tipo == LING_LEG_FORCADA || f->letreiro) return 1;
  for (i = 0; f->nome[i] && i + 1 < sizeof n; i++) n[i] = (char)tolower((unsigned char)f->nome[i]);
  n[i] = 0;
  return strstr(n, "sign") || strstr(n, "song") || strstr(n, "s&s") || strstr(n, "forced");
}
static int mesmoIdioma(const char *a, const char *b) {
  return a && b && a[0] && b[0] && ling_casa(a, b) && ling_casa(b, a);
}
int tbburn_escolher_auto(const TbFaixa *f, int n, int mem, const TbFaixa *m, const char *pref) {
  int i, passo;
  if (!f || n <= 0 || mem == 1) return -1;
  if (mem == 2 && m) {
    for (i = 0; i < n; i++) if (m->nome[0] && !strcmp(f[i].nome, m->nome) && !strcasecmp(f[i].idioma, m->idioma)) return i;
    for (i = 0; i < n; i++) if (ehLetreiro(&f[i]) == ehLetreiro(m) && mesmoIdioma(f[i].idioma, m->idioma)) return i;
  }
  // Signs & Songs (in your language first), then the dialogue track.
  for (passo = 0; passo < 2; passo++)
    for (i = 0; i < n; i++)
      if (ehLetreiro(&f[i]) && (passo || !pref || !pref[0] || mesmoIdioma(f[i].idioma, pref))) return i;
  for (passo = 0; passo < 2; passo++)
    for (i = 0; i < n; i++)
      if (passo || !pref || !pref[0] || mesmoIdioma(f[i].idioma, pref)) return i;
  return -1;
}

// File: one line per show, "titulo\tquando\tmodo\tidioma\ttipo\tletreiro\tnome\thdr"
// (modo o = Off, f = a track; hdr h = burn-in turned On on an HDR file of this
// show, owner 13/11; older lines have no 8th column = not allowed). Per
// profile, like legmemoria.
#define TBMEM_MAX 150
typedef struct { char titulo[64]; long long quando; char modo, hdr; TbFaixa f; } TbMem;
static TbMem tbMem[TBMEM_MAX];
static int nTbMem, tbMemPerfil = -1;
static char tituloAtual[64];

static const char *tbMemArquivo(void) {
  static char nome[48];
  snprintf(nome, sizeof nome, "tbburn-memoria-p%d.txt", tbMemPerfil);
  return nome;
}
static char *campoTab(char **p) {
  char *ini = *p, *t;
  if (!ini) return (char *)"";
  t = strchr(ini, '\t');
  if (t) { *t = 0; *p = t + 1; } else *p = NULL;
  return ini;
}
static void tbMemCarregar(void) {
  char *b, *linha, *prox;
  int perfil = perfis_ativo();
  if (perfil == tbMemPerfil) return;
  tbMemPerfil = perfil; nTbMem = 0;
  b = dados_ler(tbMemArquivo());
  if (!b) return;
  for (linha = b; linha && *linha && nTbMem < TBMEM_MAX; linha = prox) {
    char *p = linha, *fim = strchr(linha, '\n');
    TbMem *m;
    prox = fim ? fim + 1 : NULL;
    if (fim) *fim = 0;
    if (linha[0] == '#' || !linha[0]) continue;
    m = &tbMem[nTbMem];
    memset(m, 0, sizeof *m);
    snprintf(m->titulo, sizeof m->titulo, "%s", campoTab(&p));
    m->quando = atoll(campoTab(&p));
    m->modo = campoTab(&p)[0];
    snprintf(m->f.idioma, sizeof m->f.idioma, "%s", campoTab(&p));
    m->f.tipo = atoi(campoTab(&p));
    m->f.letreiro = atoi(campoTab(&p));
    snprintf(m->f.nome, sizeof m->f.nome, "%s", campoTab(&p));
    m->hdr = campoTab(&p)[0] == 'h' ? 'h' : 0;
    if (m->titulo[0] && (m->modo == 'o' || m->modo == 'f')) nTbMem++;
  }
  free(b);
}
static void tbMemGravar(void) {
  size_t cap = (size_t)TBMEM_MAX * 220u + 64u, k = 0;
  char *buf = malloc(cap);
  int i;
  if (!buf) return;
  k += (size_t)snprintf(buf + k, cap - k, "# nuvio burn-in v1\n");
  for (i = 0; i < nTbMem && k + 1 < cap; i++)
    k += (size_t)snprintf(buf + k, cap - k, "%s\t%lld\t%c\t%s\t%d\t%d\t%s\t%s\n", tbMem[i].titulo, tbMem[i].quando,
                          tbMem[i].modo, tbMem[i].f.idioma, tbMem[i].f.tipo, tbMem[i].f.letreiro, tbMem[i].f.nome,
                          tbMem[i].hdr == 'h' ? "h" : "");
  dados_gravar(tbMemArquivo(), buf);
  free(buf);
}
void tbburn_titulo(const char *titulo) {
  snprintf(tituloAtual, sizeof tituloAtual, "%s", titulo ? titulo : "");
}
int tbburn_memoria(TbFaixa *out) {
  int i;
  if (!tituloAtual[0]) return 0;
  tbMemCarregar();
  for (i = 0; i < nTbMem; i++)
    if (!strcmp(tbMem[i].titulo, tituloAtual)) {
      if (tbMem[i].modo == 'o') return 1;
      if (out) *out = tbMem[i].f;
      return 2;
    }
  return 0;
}
void tbburn_lembrar(const TbFaixa *f) {
  int i, k = -1;
  char hdr = 0;
  if (!tituloAtual[0]) return;
  tbMemCarregar();
  for (i = 0; i < nTbMem; i++) if (!strcmp(tbMem[i].titulo, tituloAtual)) k = i;
  if (k < 0) {
    if (nTbMem >= TBMEM_MAX) {   // full: the oldest goes
      int v = 0;
      for (i = 1; i < nTbMem; i++) if (tbMem[i].quando < tbMem[v].quando) v = i;
      k = v;
    } else k = nTbMem++;
  }
  if (k < nTbMem && !strcmp(tbMem[k].titulo, tituloAtual)) hdr = tbMem[k].hdr;   // the HDR allowance stays
  memset(&tbMem[k], 0, sizeof tbMem[k]);
  tbMem[k].hdr = hdr;
  snprintf(tbMem[k].titulo, sizeof tbMem[k].titulo, "%s", tituloAtual);
  tbMem[k].quando = (long long)time(NULL);
  tbMem[k].modo = f ? 'f' : 'o';
  if (f) {
    tbMem[k].f = *f;
    for (char *c = tbMem[k].f.nome; *c; c++) if ((unsigned char)*c < 32) *c = ' ';
  }
  tbMemGravar();
  printf("[tbburn] remembered for %s: %s%s%s\n", tituloAtual, f ? f->nome : "Off", f ? " / " : "", f ? f->idioma : "");
  fflush(stdout);
}

// ---- HDR (owner 13/11) ----------------------------------------------------------
// TorBox's transcode loses HDR: on an HDR file (the source's HDR badge or the
// file's own colour info, either one) burn-in starts Off. Turning it On in
// playback is remembered for the show (the 8th column above).
static int hdrFonte;
void tbburn_hdr_fonte(int sim) { hdrFonte = sim ? 1 : 0; }
int  tbburn_hdr_da_fonte(void) { return hdrFonte; }
int  tbburn_hdr_permitido(void) {
  int i;
  if (!tituloAtual[0]) return 0;
  tbMemCarregar();
  for (i = 0; i < nTbMem; i++) if (!strcmp(tbMem[i].titulo, tituloAtual)) return tbMem[i].hdr == 'h';
  return 0;
}
void tbburn_hdr_lembrar(int sim) {
  int i;
  if (!tituloAtual[0]) return;
  tbMemCarregar();
  for (i = 0; i < nTbMem; i++)
    if (!strcmp(tbMem[i].titulo, tituloAtual)) {
      if ((tbMem[i].hdr == 'h') == (sim != 0)) return;
      tbMem[i].hdr = sim ? 'h' : 0;
      tbMemGravar();
      printf("[tbburn] HDR files of %s: burn-in %s\n", tituloAtual, sim ? "On (remembered)" : "Off by default again");
      fflush(stdout);
      return;
    }
}
