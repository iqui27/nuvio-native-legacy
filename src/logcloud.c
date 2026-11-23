// See logcloud.h. Adapted from Matis94 (src/logcloud.c on
// dv-dts-native-test-2.0.8.7): the multipart/catbox upload is replaced by
// POST /v1/loglink on OUR diagnostics worker (NV_REC_URL) with an
// upload-only credential (x-nuvio-auth: loglink + Bearer NV_LOGLINK_TOKEN,
// the worker's LOGLINK_TOKEN). It never carries DIAG_TOKEN: the worker only
// lets this token create a link, and refuses it on every other route.
// No token or no worker URL = the card says the upload is unavailable;
// nothing is sent anywhere else. The link itself is never printed.
#include "logcloud.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nvbackend.h"
#ifndef NV_REC_URL
#define NV_REC_URL ""
#endif
#ifndef NV_LOGLINK_TOKEN
#define NV_LOGLINK_TOKEN ""
#endif

/* The request body is the gzipped snapshot itself (binary, identity of the
 * bytes, boundary unused). Kept as a helper so tests exercise exactly what
 * goes on the wire. */
char *logcloud_corpo(const unsigned char *gz, unsigned long n, const char *fronteira, unsigned long *tam) {
  char *c;
  (void)fronteira;
  if (n && !gz) return NULL;
  c = malloc(n ? n : 1);
  if (!c) return NULL;
  if (n) memcpy(c, gz, n);
  if (tam) *tam = n;
  return c;
}
/* The worker answers {"ok":1,"url":"https://…/v1/loglink/<token>"}. */
int logcloud_resposta_url(const char *corpo, char *dst, int n) {
  const char *p, *s, *e;
  if (dst && n > 0) dst[0] = 0;
  if (!corpo || !dst || n <= 0) return 0;
  p = strstr(corpo, "\"url\"");
  if (!p) return 0;
  p = strchr(p + 5, '"');
  if (!p) return 0;
  s = p + 1;
  if (strncmp(s, "https://", 8) && strncmp(s, "http://", 7)) return 0;
  for (e = s; *e && *e != '"' && *e != '\\'; e++) {}
  if (e - s <= 8 || e - s >= n) return 0;
  memcpy(dst, s, (size_t)(e - s));
  dst[e - s] = 0;
  return 1;
}

#if defined(__EMSCRIPTEN__) || defined(NV_LOGCLOUD_SEM_REDE)
int logcloud_enviar(void) { return 0; }
int logcloud_estado(void) { return LOGCLOUD_PARADO; }
void logcloud_url(char *d, int n) { if (d && n > 0) d[0] = 0; }
void logcloud_motivo(char *d, int n) { if (d && n > 0) d[0] = 0; }
void logcloud_esquecer(void) {}
#else
#include "logexport.h"
#include "qr.h"
#include "registro.h"
#include "rede.h"
#include <pthread.h>
#include <unistd.h>
#include <zlib.h>

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static int estado, rodando;
static char url[256], motivo[128];

static void terminar(int e, const char *u, const char *m) {
  pthread_mutex_lock(&trava);
  estado = e;
  snprintf(url, sizeof url, "%s", u ? u : "");
  snprintf(motivo, sizeof motivo, "%s", m ? m : "");
  rodando = 0;
  pthread_mutex_unlock(&trava);
}
/* The whole snapshot gzipped in memory (16 MB of log is ~1-2 MB). */
static unsigned char *gzipArquivo(const char *path, unsigned long *n) {
  FILE *f = fopen(path, "rb");
  unsigned char in[65536], *out = NULL;
  unsigned long cap = 0;
  z_stream z;
  int r = Z_OK, fim = 0;
  if (!f) return NULL;
  memset(&z, 0, sizeof z);
  if (deflateInit2(&z, 6, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) { fclose(f); return NULL; }
  cap = 1 << 20; out = malloc(cap);
  if (!out) goto erro;
  z.next_out = out; z.avail_out = (uInt)cap;
  while (r != Z_STREAM_END) {
    if (!z.avail_in && !fim) {
      size_t k = fread(in, 1, sizeof in, f);
      if (ferror(f)) goto erro;
      z.next_in = in; z.avail_in = (uInt)k;
      fim = feof(f) ? 1 : 0;
    }
    if (!z.avail_out) {
      unsigned long usado = cap;
      unsigned char *m = realloc(out, cap * 2);
      if (!m) goto erro;
      out = m; cap *= 2;
      z.next_out = out + usado; z.avail_out = (uInt)(cap - usado);
    }
    r = deflate(&z, fim ? Z_FINISH : Z_NO_FLUSH);
    if (r != Z_OK && r != Z_STREAM_END && r != Z_BUF_ERROR) goto erro;
  }
  *n = z.total_out;
  deflateEnd(&z); fclose(f);
  return out;
erro:
  deflateEnd(&z); fclose(f); free(out);
  return NULL;
}
static void *fio(void *u) {
  char snap[128], m[128], achou[QR_MAX_BYTES + 1], auth[160], u2[256]; /* the link must fit the QR */
  unsigned long ngz = 0, ncorpo = 0;
  unsigned char *gz = NULL;
  char *corpo = NULL;
  const char *cab[4];
  RedePedido q;
  RedeResposta r;
  (void)u;
  memset(&q, 0, sizeof q); memset(&r, 0, sizeof r);
  if (!NV_REC_URL[0]) { terminar(LOGCLOUD_FALHOU, NULL, "no diagnostics server in this build"); return NULL; }
  if (!NV_LOGLINK_TOKEN[0]) { terminar(LOGCLOUD_FALHOU, NULL, "log upload unavailable in this build"); return NULL; }
  if (!logexport_create(registro_arquivo(), snap, sizeof snap)) { terminar(LOGCLOUD_FALHOU, NULL, "log snapshot failed"); return NULL; }
  gz = gzipArquivo(snap, &ngz);
  unlink(snap);
  if (!gz) { terminar(LOGCLOUD_FALHOU, NULL, "could not pack the log"); return NULL; }
  corpo = logcloud_corpo(gz, ngz, NULL, &ncorpo);
  free(gz);
  if (!corpo) { terminar(LOGCLOUD_FALHOU, NULL, "out of memory"); return NULL; }
  snprintf(auth, sizeof auth, "Authorization: Bearer %s", NV_LOGLINK_TOKEN);
  {
    snprintf(u2, sizeof u2, "%s/v1/loglink", NV_REC_URL);
    cab[0] = "x-nuvio-auth: loglink";
    cab[1] = auth;
    cab[2] = "Content-Type: application/gzip";
    cab[3] = NULL;
    q.metodo = "POST"; q.url = u2; q.cabecalhos = cab;
    q.corpo = corpo; q.n_corpo = ncorpo; q.prazo_ms = 180000; q.max_bytes = 4096; q.seguir = 0;
  }
  printf("[log-cloud] sending %lu KiB\n", ngz / 1024); fflush(stdout);
  {
    int ok = rede_pedir(&q, &r);
    free(corpo);
    if (ok && r.status == 200 && r.corpo && logcloud_resposta_url(r.corpo, achou, sizeof achou)) {
      printf("[log-cloud] sent: link ready (expires in 72 h)\n"); fflush(stdout);
      terminar(LOGCLOUD_PRONTO, achou, NULL);
    } else {
      if (!ok || !r.status) snprintf(m, sizeof m, "no connection (curl %d)", r.curl_erro);
      /* Response bodies may contain capabilities, even on an error. */
      else if (r.status == 200) snprintf(m, sizeof m, "HTTP 200: invalid answer");
      else snprintf(m, sizeof m, "HTTP %d", r.status);
      for (char *c = m; *c; c++) if (*c == '\r' || *c == '\n') *c = ' ';
      printf("[log-cloud] failed: %s\n", m); fflush(stdout);
      terminar(LOGCLOUD_FALHOU, NULL, m);
    }
    rede_resposta_limpar(&r);
  }
  return NULL;
}
int logcloud_enviar(void) {
  pthread_t t;
  pthread_mutex_lock(&trava);
  if (rodando) { pthread_mutex_unlock(&trava); return 0; }
  rodando = 1; estado = LOGCLOUD_ENVIANDO; url[0] = motivo[0] = 0;
  pthread_mutex_unlock(&trava);
  if (pthread_create(&t, NULL, fio, NULL)) { terminar(LOGCLOUD_FALHOU, NULL, "no thread"); return 0; }
  pthread_detach(t);
  return 1;
}
int logcloud_estado(void) {
  int e;
  pthread_mutex_lock(&trava); e = estado; pthread_mutex_unlock(&trava);
  return e;
}
void logcloud_url(char *d, int n) {
  pthread_mutex_lock(&trava); if (d && n > 0) snprintf(d, (size_t)n, "%s", url); pthread_mutex_unlock(&trava);
}
void logcloud_motivo(char *d, int n) {
  pthread_mutex_lock(&trava); if (d && n > 0) snprintf(d, (size_t)n, "%s", motivo); pthread_mutex_unlock(&trava);
}
void logcloud_esquecer(void) {
  pthread_mutex_lock(&trava);
  if (!rodando) { estado = LOGCLOUD_PARADO; url[0] = motivo[0] = 0; }
  pthread_mutex_unlock(&trava);
}
#endif
