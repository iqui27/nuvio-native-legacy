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

// ---- state -----------------------------------------------------------------

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static struct {
  char url[TBB_URL_MAX], hash[48], arquivo[512];
  uint64_t bytes;
} fonte;
// The file found for fonte.url (cached: one account lookup per source).
static struct { char url[TBB_URL_MAX]; int item, arquivo, usenet, achou, procurou; char como[72]; } ids;
static int escolha = -1, temAss, recusado;
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
    printf("[tbburn] source details from the saved last link (Reuse last link)\n");
    fflush(stdout);
  }
  if (!url || strcmp(url, fonte.url)) {
    snprintf(fonte.url, sizeof fonte.url, "%s", url ? url : "");
    snprintf(fonte.hash, sizeof fonte.hash, "%s", hash ? hash : "");
    snprintf(fonte.arquivo, sizeof fonte.arquivo, "%s", arquivo ? arquivo : "");
    fonte.bytes = bytes;
    temAss = 0;
  }
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
  if (recusado || e == 0 || debrid_tb_disponivel() == 0) return 0;
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
void tbburn_manter_proximo(void) { manterProximo = 1; }
void tbburn_sessao_nova(void) {
  if (manterProximo) { manterProximo = 0; return; }
  escolha = -1;
}
void tbburn_arquivo_tem_ass(int sim) { temAss = sim != 0; }
int tbburn_linha_visivel(void) {
  int ok;
  if (!temAss || recusado || !ajustes_tb_pro() || debrid_tb_disponivel() == 0) return 0;
  pthread_mutex_lock(&trava);
  ok = !(ids.procurou && !ids.achou && !strcmp(ids.url, fonte.url));
  pthread_mutex_unlock(&trava);
  return ok;
}

// ---- the worker -------------------------------------------------------------

typedef struct {
  char url[TBB_URL_MAX], hash[48], arquivo[512];
  uint64_t bytes;
  int sub, audio;
  unsigned geracao;
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

static int pegarIds(const Pedido *p, int *item, int *arquivo, int *usenet, char *como, unsigned n) {
  int achou;
  *usenet = 0;
  pthread_mutex_lock(&trava);
  // Only a FOUND file is reused: a miss is looked up again next time (owner
  // 14/11, Horimiya E07: one miss was reused for every retry).
  if (ids.procurou && ids.achou && !strcmp(ids.url, p->url)) {
    *item = ids.item; *arquivo = ids.arquivo; *usenet = ids.usenet; snprintf(como, n, "%.56s (remembered)", ids.como);
    achou = ids.achou;
    pthread_mutex_unlock(&trava);
    return achou;
  }
  pthread_mutex_unlock(&trava);
  achou = partes_achar_exato(p->url, p->hash, p->arquivo, p->bytes, item, arquivo, como, n);
  pthread_mutex_lock(&trava);
  snprintf(ids.url, sizeof ids.url, "%s", p->url);
  ids.item = *item; ids.arquivo = *arquivo; ids.usenet = 0; ids.achou = achou; ids.procurou = 1;
  snprintf(ids.como, sizeof ids.como, "%s", como);
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

static void *trabalhar(void *arg) {
  Pedido *p = arg;
  char como[72] = "", rota[512], m3u8[TBB_URL_MAX], *r = NULL, *log = NULL;
  int item = 0, arquivo = -1, usenet = 0, st = 0, pico = 0;
  unsigned t0 = msAgora(), t;
  if (!pegarIds(p, &item, &arquivo, &usenet, como, sizeof como)) {
    printf("[tbburn] file not found in your TorBox by an exact match (hash/file name/size) in %u ms: "
           "Nuvio draws the subtitle\n", msAgora() - t0);
    fflush(stdout);
    publicar(p->geracao, TBB_FALHOU, NULL, "file not in your TorBox");
    free(p);
    return NULL;
  }
  printf("[tbburn] TorBox file: item %d, file %d (by %s) in %u ms; asking for subtitle %d, audio %d\n",
         item, arquivo, como, msAgora() - t0, p->sub, p->audio);
  fflush(stdout);
  tbburn_rota_criar(rota, sizeof rota, item, arquivo, usenet, p->sub, p->audio);
  t = msAgora();
  r = debrid_tb_api_get(rota, 30, &st);
  logCorpo("createstream", st, msAgora() - t, r);
  if (tbburn_eh_recusa_plano(st, r)) {
    recusado = 1;
    printf("[tbburn] TorBox refused the plan: burn-in is off until Nuvio restarts\n");
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
  free(r);
  log = malloc(TBB_URL_MAX);
  if (log) {
    snprintf(log, TBB_URL_MAX, "%s", m3u8);
    debrid_tb_redigir(log);
    tbburn_log_mascarar(log);   // the host only
    printf("[tbburn] stream ready in %u ms: %s\n", msAgora() - t0, log);
    fflush(stdout);
    free(log);
  }
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
