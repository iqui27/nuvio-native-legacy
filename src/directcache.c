// #764: cache USB por escrita antecipada para links HTTP diretos/debrid.
#include "directcache.h"
#include "ajustes.h"
#include "p2pmotor.h"
#include "rede.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

#define DC_MIN_TOCAR (4 << 20)   /* 4 MiB antes de file:// */
#define DC_BLOCO (256 << 10)

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static char jobUrl[2048], jobArq[760], jobCab[4096];
static pthread_t jobFio;
static int jobAtivo, jobRodando;
static unsigned long long jobEscrito;

static int httpUrl(const char *u) {
  return u && (!strncmp(u, "http://", 7) || !strncmp(u, "https://", 8));
}

int directcache_ativo(void) {
  return ajustes_direct_cache_usb() && p2pmotor_usb_presente();
}

int directcache_pasta(char *out, size_t n) {
  P2pmUsbInfo usb;
  if (!directcache_ativo() || !out || n < 8) return 0;
  p2pmotor_usb_info(&usb);
  if (!usb.estado || !usb.gravavel || !usb.caminho[0]) return 0;
  if (snprintf(out, n, "%s/wa", usb.caminho) >= (int)n) return 0;
  { char tmp[820];
    snprintf(tmp, sizeof tmp, "%s", out);
    if (mkdir(tmp, 0755) && errno != EEXIST) return 0; }
  return access(out, W_OK) == 0;
}

static unsigned fnv(const char *s) {
  unsigned h = 2166136261u;
  for (; s && *s; s++) h = (h ^ (unsigned char)*s) * 16777619u;
  return h;
}

static void cabSplit(const char *cab, const char *out[], unsigned max) {
  char buf[4096];
  unsigned n = 0;
  if (!cab || !cab[0]) { out[0] = NULL; return; }
  snprintf(buf, sizeof buf, "%s", cab);
  for (char *p = buf, *t; p && n + 1 < max; p = t) {
    t = strchr(p, '\n');
    if (t) *t++ = 0;
    while (*p == '\r' || *p == ' ') p++;
    if (*p) out[n++] = p;
    if (!t) break;
  }
  out[n] = NULL;
}

static int gravaCb(const char *dados, size_t tam, void *u) {
  FILE *f = u;
  if (!f || !dados || !tam) return 0;
  if (fwrite(dados, 1, tam, f) != tam) return 0;
  jobEscrito += tam;
  return 1;
}

static void *fioGrava(void *u) {
  (void)u;
  FILE *f = fopen(jobArq, "wb");
  const char *cab[48];
  int ok = 0;
  if (!f) goto fim;
  cabSplit(jobCab, cab, 48);
  ok = rede_baixar_fluxo(jobUrl, 0, cab, gravaCb, f, 0, NULL, NULL);
  fflush(f);
  fsync(fileno(f));
  fclose(f);
  if (!ok) unlink(jobArq);
fim:
  pthread_mutex_lock(&trava);
  jobRodando = 0;
  pthread_mutex_unlock(&trava);
  return NULL;
}

static void jobIniciar(const char *url, const char *cab, const char *arq) {
  pthread_mutex_lock(&trava);
  if (jobAtivo && !strcmp(jobUrl, url)) { pthread_mutex_unlock(&trava); return; }
  if (jobRodando) { pthread_mutex_unlock(&trava); return; }
  snprintf(jobUrl, sizeof jobUrl, "%s", url);
  snprintf(jobArq, sizeof jobArq, "%s", arq);
  snprintf(jobCab, sizeof jobCab, "%s", cab ? cab : "");
  jobEscrito = 0;
  jobAtivo = 1;
  jobRodando = 1;
  if (pthread_create(&jobFio, NULL, fioGrava, NULL)) {
    jobRodando = 0;
    jobAtivo = 0;
  }
  pthread_mutex_unlock(&trava);
}

const char *directcache_preparar(const char *url, const char *cabecalhos,
                                 char *buf, size_t n) {
  char dir[760], arq[900], chave[96];
  struct stat st;
  if (!url || !buf || n < 16) return url;
  snprintf(buf, n, "%s", url);
  if (!httpUrl(url) || !directcache_ativo() || !directcache_pasta(dir, sizeof dir))
    return buf;
  snprintf(chave, sizeof chave, "%u", fnv(url));
  if (snprintf(arq, sizeof arq, "%s/%s.part", dir, chave) >= (int)sizeof arq)
    return buf;
  jobIniciar(url, cabecalhos, arq);
  if (stat(arq, &st) == 0 && (unsigned long long)st.st_size >= DC_MIN_TOCAR) {
    if (snprintf(buf, n, "file://%s", arq) < (int)n) return buf;
  }
  return buf;
}

void directcache_encerrar(void) {
  pthread_mutex_lock(&trava);
  if (jobRodando) pthread_join(jobFio, NULL);
  jobAtivo = jobRodando = 0;
  jobUrl[0] = jobArq[0] = 0;
  pthread_mutex_unlock(&trava);
}
