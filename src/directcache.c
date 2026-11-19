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
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

#ifdef __EMSCRIPTEN__
int directcache_ativo(void) { return 0; }
int directcache_pasta(char *out, size_t n) { (void)out; (void)n; return 0; }
const char *directcache_preparar(const char *url, const char *cabecalhos, char *buf, size_t n) {
  if (url && buf && n) snprintf(buf, n, "%s", url);
  return buf ? buf : url;
}
void directcache_encerrar(void) {}
#else

#define DC_MAX_BYTES_PADRAO (8ull << 30)

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static char jobUrl[2048], jobPart[760], jobFinal[760], jobCab[4096];
static pthread_t jobFio;
static int jobAtivo, jobRodando, jobCancelar;
static unsigned jobSeq;
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

static void cabSplit(const char *cab, char buf[4096], const char *out[], unsigned max) {
  unsigned n = 0;
  char *p, *t;
  if (!cab || !cab[0]) { out[0] = NULL; return; }
  snprintf(buf, 4096, "%s", cab);
  for (p = buf; p && n + 1 < max; p = t) {
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
  pthread_mutex_lock(&trava);
  jobEscrito += tam;
  pthread_mutex_unlock(&trava);
  return 1;
}

static void *fioGrava(void *u) {
  (void)u;
  FILE *f;
  const char *cab[48];
  char cabBuf[4096];
  char part[760], final[760], url[2048], cabec[4096];
  unsigned long long maxBytes = (unsigned long long)ajustes_p2p_limite_mb() * 1024ull * 1024ull;
  int ok = 0, cancel;
  if (!maxBytes) maxBytes = DC_MAX_BYTES_PADRAO;
  pthread_mutex_lock(&trava);
  snprintf(part, sizeof part, "%s", jobPart);
  snprintf(final, sizeof final, "%s", jobFinal);
  snprintf(url, sizeof url, "%s", jobUrl);
  snprintf(cabec, sizeof cabec, "%s", jobCab);
  pthread_mutex_unlock(&trava);
  f = fopen(part, "wb");
  if (!f) goto fim;
  cabSplit(cabec, cabBuf, cab, 48);
  ok = rede_baixar_fluxo(url, 0, cab, gravaCb, f, (long long)maxBytes, NULL, NULL);
  pthread_mutex_lock(&trava);
  cancel = jobCancelar;
  pthread_mutex_unlock(&trava);
  if (cancel) ok = 0;
  fflush(f); fsync(fileno(f)); fclose(f);
  if (ok) {
    unlink(final);
    if (rename(part, final) != 0) { ok = 0; unlink(part); }
  } else unlink(part);
fim:
  pthread_mutex_lock(&trava);
  jobRodando = 0;
  pthread_mutex_unlock(&trava);
  return NULL;
}

static void jobIniciar(const char *url, const char *cab, const char *dir) {
  unsigned seq;
  pthread_t velho;
  int tinha;
  pthread_mutex_lock(&trava);
  if (jobRodando && jobAtivo && !strcmp(jobUrl, url)) { pthread_mutex_unlock(&trava); return; }
  if (jobRodando) {
    tinha = 1; velho = jobFio; jobCancelar = 1;
    pthread_mutex_unlock(&trava);
    pthread_join(velho, NULL);
    pthread_mutex_lock(&trava);
    jobCancelar = 0;
  } else tinha = 0;
  (void)tinha;
  snprintf(jobUrl, sizeof jobUrl, "%s", url);
  snprintf(jobCab, sizeof jobCab, "%s", cab ? cab : "");
  jobEscrito = 0; jobRodando = 1; jobAtivo = 1;
  seq = ++jobSeq;
  snprintf(jobPart, sizeof jobPart, "%s/%u-%u.part", dir, fnv(url), seq);
  snprintf(jobFinal, sizeof jobFinal, "%s/%u", dir, fnv(url));
  if (pthread_create(&jobFio, NULL, fioGrava, NULL)) {
    jobRodando = 0; jobAtivo = 0;
  }
  pthread_mutex_unlock(&trava);
}

static void limparPartAntigos(const char *dir) {
  DIR *d = opendir(dir);
  struct dirent *de;
  char path[900];
  if (!d) return;
  while ((de = readdir(d))) {
    const char *n = de->d_name;
    size_t nl = strlen(n);
    if (nl > 5 && !strcmp(n + nl - 5, ".part")) {
      snprintf(path, sizeof path, "%s/%s", dir, n);
      unlink(path);
    }
  }
  closedir(d);
}

const char *directcache_preparar(const char *url, const char *cabecalhos,
                                 char *buf, size_t n) {
  char dir[760], final[900];
  struct stat st;
  if (!url || !buf || n < 16) return url;
  snprintf(buf, n, "%s", url);
  if (!httpUrl(url) || !directcache_ativo() || !directcache_pasta(dir, sizeof dir))
    return buf;
  limparPartAntigos(dir);
  snprintf(final, sizeof final, "%s/%u", dir, fnv(url));
  if (stat(final, &st) == 0 && st.st_size > 0 &&
      snprintf(buf, n, "file://%s", final) < (int)n)
    return buf;
  jobIniciar(url, cabecalhos, dir);
  snprintf(buf, n, "%s", url);
  return buf;
}

void directcache_encerrar(void) {
  pthread_t fio;
  int rodando;
  pthread_mutex_lock(&trava);
  rodando = jobRodando;
  fio = jobFio;
  jobCancelar = 1;
  pthread_mutex_unlock(&trava);
  if (rodando) pthread_join(fio, NULL);
  pthread_mutex_lock(&trava);
  jobAtivo = jobRodando = 0; jobCancelar = 0;
  jobUrl[0] = jobPart[0] = jobFinal[0] = 0;
  pthread_mutex_unlock(&trava);
}

#endif
