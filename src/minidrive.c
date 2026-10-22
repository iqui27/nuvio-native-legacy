// Seek-bar thumbnail sets in the owner's Google Drive. See minidrive.h.
#include "minidrive.h"
#include "miniplat.h"

#ifdef NV_MINIATURAS
#include "dados.h"
#include "js.h"
#include "rede.h"
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef NV_GDRIVE_CLIENT_ID
#define NV_GDRIVE_CLIENT_ID ""
#endif
#ifndef NV_GDRIVE_CLIENT_SECRET
#define NV_GDRIVE_CLIENT_SECRET ""
#endif

#define ARQ_TOKEN "gdrive.txt"
#define ARQ_LIMPEZA "gdrive-limpeza.txt"
#define ESCOPO "https://www.googleapis.com/auth/drive.appdata"
#define DIAS_SEM_USO 40

static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static GdrEstado estado;
static char codigo[32], urlVerif[128], erro[160];
static char refresh[512];
static char acesso[2048]; static time_t acessoAte;
static int fioVivo;
static volatile int cancelar;

int minidrive_disponivel(void) { return NV_GDRIVE_CLIENT_ID[0] != 0; }

int minidrive_ligado(void) {
  int r; pthread_mutex_lock(&mu); r = refresh[0] != 0; pthread_mutex_unlock(&mu); return r;
}
GdrEstado minidrive_estado(void) {
  GdrEstado e; pthread_mutex_lock(&mu); e = estado; pthread_mutex_unlock(&mu); return e;
}
// Read on the draw thread; written only while the flow runs (short strings).
const char *minidrive_codigo(void) { return codigo; }
const char *minidrive_url(void) { return urlVerif; }
const char *minidrive_erro(void) { return erro[0] ? erro : NULL; }

void minidrive_carregar(void) {
  char *t = dados_ler(ARQ_TOKEN);
  pthread_mutex_lock(&mu);
  refresh[0] = 0;
  if (t) {
    size_t n = strcspn(t, "\r\n");
    if (n < sizeof refresh) { memcpy(refresh, t, n); refresh[n] = 0; }
  }
  estado = refresh[0] ? GDR_LIGADO : GDR_PARADO;
  pthread_mutex_unlock(&mu);
  free(t);
  printf("[miniatura] google drive %s\n", refresh[0] ? "linked" : "not linked"); fflush(stdout);
}

void minidrive_desconectar(void) {
  pthread_mutex_lock(&mu);
  refresh[0] = 0; acesso[0] = 0; acessoAte = 0; estado = GDR_PARADO; erro[0] = 0;
  pthread_mutex_unlock(&mu);
  dados_apagar(ARQ_TOKEN);
  printf("[miniatura] google drive unlinked on this TV\n"); fflush(stdout);
}

// ------------------------------------------------------------------ helpers
static void codificar(const char *s, char *d, size_t n) {
  static const char hex[] = "0123456789ABCDEF";
  size_t k = 0;
  for (; *s && k + 4 < n; s++) {
    unsigned char c = (unsigned char)*s;
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') d[k++] = (char)c;
    else { d[k++] = '%'; d[k++] = hex[c >> 4]; d[k++] = hex[c & 15]; }
  }
  d[k] = 0;
}

static char *postarForm(const char *url, const char *corpo, int *st) {
  const char *cab[] = { "Content-Type: application/x-www-form-urlencoded", NULL };
  return rede_postar_st(url, 15, cab, corpo, st);
}

// Bearer token, refreshed when it is about to expire. 0 ok.
static int token(char *cab, size_t n) {
  char r[sizeof refresh], corpo[1400], id[256], sec[256];
  int st = 0;
  pthread_mutex_lock(&mu);
  if (acesso[0] && time(NULL) < acessoAte - 60) {
    snprintf(cab, n, "Authorization: Bearer %s", acesso);
    pthread_mutex_unlock(&mu);
    return 0;
  }
  snprintf(r, sizeof r, "%s", refresh);
  pthread_mutex_unlock(&mu);
  if (!r[0]) return -1;
  codificar(NV_GDRIVE_CLIENT_ID, id, sizeof id);
  codificar(NV_GDRIVE_CLIENT_SECRET, sec, sizeof sec);
  char rc[1100]; codificar(r, rc, sizeof rc);
  snprintf(corpo, sizeof corpo, "client_id=%s&client_secret=%s&refresh_token=%s&grant_type=refresh_token", id, sec, rc);
  char *resp = postarForm("https://oauth2.googleapis.com/token", corpo, &st);
  char tok[2048] = "";
  double exp = 0;
  if (resp && st == 200) {
    js_texto(resp, NULL, "access_token", tok, sizeof tok);
    exp = js_num(resp, NULL, "expires_in", 3600);
  }
  if (st == 400 && resp && strstr(resp, "invalid_grant")) {
    // Revoked or expired (an OAuth app left in "Testing" expires them after 7 days).
    printf("[miniatura] google drive: sign-in no longer valid, link it again in Settings\n"); fflush(stdout);
    pthread_mutex_lock(&mu); refresh[0] = 0; estado = GDR_PARADO; pthread_mutex_unlock(&mu);
    dados_apagar(ARQ_TOKEN);
  }
  free(resp);
  if (!tok[0]) { printf("[miniatura] google drive: token refresh failed http=%d\n", st); fflush(stdout); return -1; }
  pthread_mutex_lock(&mu);
  snprintf(acesso, sizeof acesso, "%s", tok);
  acessoAte = time(NULL) + (time_t)exp;
  snprintf(cab, n, "Authorization: Bearer %s", acesso);
  pthread_mutex_unlock(&mu);
  return 0;
}

// One Drive request through rede_pedir (binary bodies both ways).
static int pedir(const char *metodo, const char *url, const char *tipo, const void *corpo, size_t n,
                 RedeResposta *r, unsigned prazoMs) {
  char auth[2100], ct[160];
  const char *cab[3] = { auth, NULL, NULL };
  if (token(auth, sizeof auth) < 0) return -1;
  if (tipo) { snprintf(ct, sizeof ct, "Content-Type: %s", tipo); cab[1] = ct; }
  RedePedido p = {0};
  p.metodo = metodo; p.url = url; p.cabecalhos = cab;
  p.corpo = corpo; p.n_corpo = n;
  p.prazo_ms = prazoMs; p.max_bytes = REDE_CORPO_MAXIMO; p.seguir = 1;
  if (!rede_pedir(&p, r) && !r->status) return -1;
  return r->status;
}

// ------------------------------------------------------------------ device flow
static void definir(GdrEstado e, const char *msg) {
  pthread_mutex_lock(&mu);
  estado = e;
  snprintf(erro, sizeof erro, "%s", msg ? msg : "");
  pthread_mutex_unlock(&mu);
}

static void *fluxo(void *u) {
  (void)u;
  char corpo[1200], id[256], sec[256], dev[512] = "";
  int st = 0;
  double intervalo = 5, expira = 600;
  codificar(NV_GDRIVE_CLIENT_ID, id, sizeof id);
  codificar(NV_GDRIVE_CLIENT_SECRET, sec, sizeof sec);
  char esc[200]; codificar(ESCOPO, esc, sizeof esc);
  snprintf(corpo, sizeof corpo, "client_id=%s&scope=%s", id, esc);
  char *r = postarForm("https://oauth2.googleapis.com/device/code", corpo, &st);
  if (!r || st != 200) {
    // Google's short error code only (invalid_client, unauthorized_client...): never the body or keys.
    char ge[64] = "";
    if (r) js_texto(r, NULL, "error", ge, sizeof ge);
    const char *sufixo = ".apps.googleusercontent.com";
    size_t ni = strlen(NV_GDRIVE_CLIENT_ID), ns = strlen(sufixo);
    int idOk = ni > ns + 10 && !strcmp(NV_GDRIVE_CLIENT_ID + ni - ns, sufixo);
    printf("[miniatura] google drive: code request failed http=%d error=%s%s\n", st, ge[0] ? ge : "-",
           idOk ? "" : " (the built-in client id does not look like a Google OAuth client id)");
    fflush(stdout);
    definir(GDR_ERRO, "Não foi possível agora. Tente novamente.");
    free(r); goto fim;
  }
  pthread_mutex_lock(&mu);
  js_texto(r, NULL, "user_code", codigo, sizeof codigo);
  if (!js_texto(r, NULL, "verification_url", urlVerif, sizeof urlVerif))
    js_texto(r, NULL, "verification_uri", urlVerif, sizeof urlVerif);
  pthread_mutex_unlock(&mu);
  js_texto(r, NULL, "device_code", dev, sizeof dev);
  intervalo = js_num(r, NULL, "interval", 5);
  expira = js_num(r, NULL, "expires_in", 600);
  free(r);
  if (!dev[0] || !codigo[0]) { definir(GDR_ERRO, "Não foi possível agora. Tente novamente."); goto fim; }
  definir(GDR_AGUARDANDO, NULL);
  {
    char dc[700]; codificar(dev, dc, sizeof dc);
    snprintf(corpo, sizeof corpo,
             "client_id=%s&client_secret=%s&device_code=%s&grant_type=urn%%3Aietf%%3Aparams%%3Aoauth%%3Agrant-type%%3Adevice_code",
             id, sec, dc);
  }
  time_t fimEm = time(NULL) + (time_t)expira;
  while (!cancelar && time(NULL) < fimEm) {
    for (int k = 0; k < (int)(intervalo * 10) && !cancelar; k++) usleep(100000);
    if (cancelar) break;
    st = 0;
    r = postarForm("https://oauth2.googleapis.com/token", corpo, &st);
    if (r && st == 200) {
      char ref[sizeof refresh] = "", ac[2048] = "";
      js_texto(r, NULL, "refresh_token", ref, sizeof ref);
      js_texto(r, NULL, "access_token", ac, sizeof ac);
      double exp = js_num(r, NULL, "expires_in", 3600);
      free(r);
      if (!ref[0]) { definir(GDR_ERRO, "Não foi possível agora. Tente novamente."); goto fim; }
      if (!dados_gravar(ARQ_TOKEN, ref)) printf("[miniatura] google drive: could not save the sign-in on this TV\n");
      pthread_mutex_lock(&mu);
      snprintf(refresh, sizeof refresh, "%s", ref);
      snprintf(acesso, sizeof acesso, "%s", ac);
      acessoAte = time(NULL) + (time_t)exp;
      estado = GDR_LIGADO; erro[0] = 0;
      pthread_mutex_unlock(&mu);
      printf("[miniatura] google drive linked\n"); fflush(stdout);
      goto fim;
    }
    char e[64] = "";
    if (r) js_texto(r, NULL, "error", e, sizeof e);
    free(r);
    if (!strcmp(e, "authorization_pending")) continue;
    if (!strcmp(e, "slow_down")) { intervalo += 5; continue; }
    if (!strcmp(e, "access_denied")) { definir(GDR_ERRO, "Não foi possível agora. Tente novamente."); goto fim; }
    if (!strcmp(e, "expired_token")) break;
    if (st == 0 || st >= 500) continue;     // network blip: keep polling until the code expires
    printf("[miniatura] google drive: sign-in failed http=%d error=%s\n", st, e[0] ? e : "-"); fflush(stdout);
    definir(GDR_ERRO, "Não foi possível agora. Tente novamente.");
    goto fim;
  }
  if (!cancelar) definir(GDR_ERRO, "o código expirou — OK pede outro");
fim:
  pthread_mutex_lock(&mu);
  if (cancelar && estado != GDR_LIGADO) estado = refresh[0] ? GDR_LIGADO : GDR_PARADO;
  fioVivo = 0;
  pthread_mutex_unlock(&mu);
  return NULL;
}

void minidrive_comecar(void) {
  pthread_t f;
  if (!minidrive_disponivel()) { definir(GDR_ERRO, "indisponível nesta versão"); return; }
  pthread_mutex_lock(&mu);
  if (fioVivo) { pthread_mutex_unlock(&mu); return; }
  fioVivo = 1; cancelar = 0; estado = GDR_PEDINDO; codigo[0] = 0; urlVerif[0] = 0; erro[0] = 0;
  pthread_mutex_unlock(&mu);
  if (pthread_create(&f, NULL, fluxo, NULL) == 0) pthread_detach(f);
  else { pthread_mutex_lock(&mu); fioVivo = 0; estado = GDR_ERRO; pthread_mutex_unlock(&mu); }
}

void minidrive_cancelar(void) {
  cancelar = 1;
  pthread_mutex_lock(&mu);
  if (estado != GDR_LIGADO) estado = refresh[0] ? GDR_LIGADO : GDR_PARADO;
  erro[0] = 0;
  pthread_mutex_unlock(&mu);
}

// ------------------------------------------------------------------ name key (SHA-256)
typedef struct { uint32_t h[8]; uint64_t n; unsigned char b[64]; size_t k; } Sha;
static const uint32_t K[64] = {
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
static void shaBloco(Sha *s, const unsigned char *p) {
  uint32_t w[64], a, b, c, d, e, f, g, h;
  for (int i = 0; i < 16; i++) w[i] = (uint32_t)p[4*i] << 24 | (uint32_t)p[4*i+1] << 16 | (uint32_t)p[4*i+2] << 8 | p[4*i+3];
  for (int i = 16; i < 64; i++) {
    uint32_t s0 = ROR(w[i-15], 7) ^ ROR(w[i-15], 18) ^ (w[i-15] >> 3);
    uint32_t s1 = ROR(w[i-2], 17) ^ ROR(w[i-2], 19) ^ (w[i-2] >> 10);
    w[i] = w[i-16] + s0 + w[i-7] + s1;
  }
  a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3]; e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
  for (int i = 0; i < 64; i++) {
    uint32_t t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
    uint32_t t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
    h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
  }
  s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}
static void shaIni(Sha *s) {
  static const uint32_t h0[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
  memcpy(s->h, h0, sizeof h0); s->n = 0; s->k = 0;
}
static void shaMais(Sha *s, const void *d, size_t n) {
  const unsigned char *p = d;
  s->n += n;
  while (n--) { s->b[s->k++] = *p++; if (s->k == 64) { shaBloco(s, s->b); s->k = 0; } }
}
static void shaFim(Sha *s, unsigned char out[32]) {
  uint64_t bits = s->n * 8;
  unsigned char z = 0x80;
  shaMais(s, &z, 1);
  z = 0;
  while (s->k != 56) shaMais(s, &z, 1);
  for (int i = 7; i >= 0; i--) { unsigned char c = (unsigned char)(bits >> (8 * i)); shaMais(s, &c, 1); }
  for (int i = 0; i < 8; i++) { out[4*i] = (unsigned char)(s->h[i] >> 24); out[4*i+1] = (unsigned char)(s->h[i] >> 16);
                                out[4*i+2] = (unsigned char)(s->h[i] >> 8); out[4*i+3] = (unsigned char)s->h[i]; }
}

void minidrive_chave_nome(const char *nome, uint64_t bytes, char saida[33]) {
  char base[512], num[32];
  const char *b = nome ? nome : "";
  const char *barra = strrchr(b, '/');
  if (barra) b = barra + 1;
  size_t k = 0;
  for (; *b && k + 1 < sizeof base; b++) base[k++] = (char)tolower((unsigned char)*b);
  while (k && isspace((unsigned char)base[k - 1])) k--;
  base[k] = 0;
  saida[0] = 0;
  if (!base[0] || !bytes) return;
  Sha s; unsigned char h[32];
  shaIni(&s); shaMais(&s, base, strlen(base));
  snprintf(num, sizeof num, "|%llu", (unsigned long long)bytes);
  shaMais(&s, num, strlen(num)); shaFim(&s, h);
  for (int i = 0; i < 16; i++) snprintf(saida + 2 * i, 3, "%02x", h[i]);
}

// ------------------------------------------------------------------ Drive files
static void hoje(char d[9]) {
  time_t t = time(NULL); struct tm g; gmtime_r(&t, &g);
  strftime(d, 9, "%Y%m%d", &g);
}

static int listar(const char *chave, const char *valor, char *id, size_t nid) {
  char q[256], qe[512], url[900];
  RedeResposta r;
  snprintf(q, sizeof q, "appProperties has { key='%s' and value='%s' } and trashed = false", chave, valor);
  codificar(q, qe, sizeof qe);
  snprintf(url, sizeof url, "https://www.googleapis.com/drive/v3/files?spaces=appDataFolder"
           "&fields=files(id)&pageSize=5&orderBy=modifiedTime%%20desc&q=%s", qe);
  int st = pedir("GET", url, NULL, NULL, 0, &r, 15000);
  int achou = 0;
  if (st == 200 && r.corpo) {
    const char *f = js_array(r.corpo, NULL, "files");
    if (f && js_texto(f, js_fim(f), "id", id, nid) && id[0]) achou = 1;
  }
  rede_resposta_limpar(&r);
  if (st != 200) return -1;
  return achou;
}

int minidrive_buscar(const MiniMeta *m, unsigned char **dados, size_t *n, char *id, size_t nid) {
  *dados = NULL; *n = 0; id[0] = 0;
  if (!minidrive_ligado()) return -1;
  int r = m->fp[0] ? listar("fp", m->fp, id, nid) : 0;
  const char *como = "fingerprint";
  if (r == 0 && m->nk[0]) { r = listar("nk", m->nk, id, nid); como = "file name + size"; }
  if (r <= 0) {
    printf("[miniatura] google drive: %s\n", r < 0 ? "lookup failed" : "no saved set for this file"); fflush(stdout);
    return r;
  }
  char url[300];
  RedeResposta rr;
  snprintf(url, sizeof url, "https://www.googleapis.com/drive/v3/files/%s?alt=media", id);
  int st = pedir("GET", url, NULL, NULL, 0, &rr, 60000);
  if (st == 200 && rr.corpo && rr.n_corpo > 0) {
    *dados = (unsigned char *)rr.corpo; *n = rr.n_corpo; rr.corpo = NULL;
    printf("[miniatura] google drive: found a saved set by %s (%zu KB)\n", como, *n / 1024); fflush(stdout);
  }
  rede_resposta_limpar(&rr);
  return *dados ? 1 : -1;
}

static void propriedades(const MiniMeta *m, char *dst, size_t n) {
  char d[9]; hoje(d);
  snprintf(dst, n, "\"appProperties\":{\"v\":\"1\",\"lu\":\"%s\",\"fp\":\"%s\",\"nk\":\"%s\",\"dur\":\"%ld\","
                   "\"imdb\":\"%s\",\"s\":\"%d\",\"e\":\"%d\",\"n\":\"%d\"}",
           d, m->fp, m->nk, m->durMs, m->imdb, m->temporada, m->episodio, m->quadros);
}

int minidrive_enviar(const char *id, const MiniMeta *m, const unsigned char *dados, size_t n) {
  if (!minidrive_ligado() || !dados || !n) return -1;
  char meta[800], props[600], url[300];
  const char *lim = "nvmini7c1e9f42b8";
  propriedades(m, props, sizeof props);
  if (id && id[0]) snprintf(meta, sizeof meta, "{%s}", props);
  else snprintf(meta, sizeof meta, "{\"name\":\"%s.nvmt\",\"parents\":[\"appDataFolder\"],%s}",
                m->fp[0] ? m->fp : m->nk, props);
  char cab1[1100], cab2[128], fim[64];
  int n1 = snprintf(cab1, sizeof cab1, "--%s\r\nContent-Type: application/json; charset=UTF-8\r\n\r\n%s\r\n"
                    "--%s\r\nContent-Type: application/octet-stream\r\n\r\n", lim, meta, lim);
  int n3 = snprintf(fim, sizeof fim, "\r\n--%s--\r\n", lim);
  (void)cab2;
  size_t total = (size_t)n1 + n + (size_t)n3;
  unsigned char *corpo = malloc(total);
  if (!corpo) return -1;
  memcpy(corpo, cab1, (size_t)n1); memcpy(corpo + n1, dados, n); memcpy(corpo + n1 + n, fim, (size_t)n3);
  char tipo[96]; snprintf(tipo, sizeof tipo, "multipart/related; boundary=%s", lim);
  if (id && id[0]) snprintf(url, sizeof url, "https://www.googleapis.com/upload/drive/v3/files/%s?uploadType=multipart", id);
  else snprintf(url, sizeof url, "https://www.googleapis.com/upload/drive/v3/files?uploadType=multipart");
  RedeResposta r;
  int st = pedir(id && id[0] ? "PATCH" : "POST", url, tipo, corpo, total, &r, 120000);
  rede_resposta_limpar(&r);
  free(corpo);
  printf("[miniatura] google drive: %s the set (%d tiles, %zu KB) http=%d\n",
         id && id[0] ? "updated" : "saved", m->quadros, n / 1024, st); fflush(stdout);
  return st == 200 ? 0 : -1;
}

void minidrive_tocar(const char *id) {
  char url[300], corpo[64], d[9];
  RedeResposta r;
  if (!id || !id[0] || !minidrive_ligado()) return;
  hoje(d);
  snprintf(url, sizeof url, "https://www.googleapis.com/drive/v3/files/%s", id);
  snprintf(corpo, sizeof corpo, "{\"appProperties\":{\"lu\":\"%s\"}}", d);
  pedir("PATCH", url, "application/json; charset=UTF-8", corpo, strlen(corpo), &r, 15000);
  rede_resposta_limpar(&r);
}

void minidrive_limpeza(void) {
  char d[9], *ultima;
  if (!minidrive_ligado()) return;
  hoje(d);
  ultima = dados_ler(ARQ_LIMPEZA);
  int feita = ultima && !strncmp(ultima, d, 8);
  free(ultima);
  if (feita) return;
  time_t lim = time(NULL) - (time_t)DIAS_SEM_USO * 86400;
  struct tm g; gmtime_r(&lim, &g);
  char corte[9]; strftime(corte, sizeof corte, "%Y%m%d", &g);
  char pagina[600] = "", url[1000];
  int vistos = 0, apagados = 0, ok = 1;
  for (int voltas = 0; voltas < 50; voltas++) {
    RedeResposta r;
    snprintf(url, sizeof url, "https://www.googleapis.com/drive/v3/files?spaces=appDataFolder&pageSize=200"
             "&fields=nextPageToken,files(id,appProperties)%s%s", pagina[0] ? "&pageToken=" : "", pagina);
    int st = pedir("GET", url, NULL, NULL, 0, &r, 30000);
    if (st != 200 || !r.corpo) { ok = 0; rede_resposta_limpar(&r); break; }
    char prox[512] = "";
    js_texto_raiz(r.corpo, "nextPageToken", prox, sizeof prox);
    for (const char *f = js_array(r.corpo, NULL, "files"); f; f = js_prox(js_fim(f))) {
      const char *ff = js_fim(f);
      char id[128] = "", lu[16] = "";
      js_texto(f, ff, "id", id, sizeof id);
      js_texto(f, ff, "lu", lu, sizeof lu);
      vistos++;
      if (id[0] && lu[0] && strcmp(lu, corte) < 0) {
        RedeResposta x; char u[300];
        snprintf(u, sizeof u, "https://www.googleapis.com/drive/v3/files/%s", id);
        int s2 = pedir("DELETE", u, NULL, NULL, 0, &x, 15000);
        rede_resposta_limpar(&x);
        if (s2 == 204 || s2 == 200) apagados++;
      }
    }
    rede_resposta_limpar(&r);
    if (!prox[0]) break;
    codificar(prox, pagina, sizeof pagina);
  }
  if (ok) dados_gravar(ARQ_LIMPEZA, d);
  printf("[miniatura] google drive cleanup: %d set(s), %d unused for %d days deleted%s\n",
         vistos, apagados, DIAS_SEM_USO, ok ? "" : " (stopped: Drive unreachable)");
  fflush(stdout);
}

#else
typedef int minidrive_vazio;   // ISO C: a translation unit must not be empty
#endif
