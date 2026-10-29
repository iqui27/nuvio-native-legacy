// Private per-provider GET gate. Call only from existing HTTP workers.
#ifndef NV_PROVEDORHTTP_H
#define NV_PROVEDORHTTP_H
#include "rede.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

static pthread_mutex_t provedorTrava = PTHREAD_MUTEX_INITIALIZER;
static time_t provedorAte;
static int provedorRecuo;

// Follow only the page number; never send credentials to a Link URL's host.
// -1 means no pagination metadata; 0 means the final page.
static int provedorProxima(const char *headers, int page) {
  int count = -1, next = 0, link = 0;
  for (const char *s = headers; s && *s; ) {
    const char *end = strchr(s, '\n'); if (!end) end = s + strlen(s);
    if (!strncasecmp(s, "X-Pagination-Page-Count:", 24)) count = atoi(s + 24);
    if (!strncasecmp(s, "Link:", 5)) {
      link = 1;
      for (const char *p = s + 5; p < end; ) {
        const char *close = memchr(p, '>', (size_t)(end - p));
        if (!close) break;
        const char *comma = memchr(close, ',', (size_t)(end - close));
        const char *stop = comma ? comma : end;
        const char *rel = strstr(close, "rel=\"next\"");
        if (rel && rel < stop) {
          const char *q = strstr(p, "?page=");
          if (!q || q > close) q = strstr(p, "&page=");
          if (q && q < close) next = atoi(q + 6);
        }
        p = stop + (stop < end);
      }
    }
    s = *end ? end + 1 : NULL;
  }
  if (next > page) return next;
  if (count >= 0) return page < count ? page + 1 : 0;
  return link ? 0 : -1;
}

static char *provedorGet(const char *url, int seconds, const char *const *headers,
                         int *status, int *next, int page) {
  int st = 0, retry = 0;
  char *body = NULL;
  if (next) *next = -1;
  if (time(NULL) < __atomic_load_n(&provedorAte, __ATOMIC_ACQUIRE)) { st = 429; goto done; }
#ifndef __EMSCRIPTEN__
  if (next) {
    RedePedido p = { .url = url, .cabecalhos = headers,
                      .prazo_ms = (unsigned)seconds * 1000, .seguir = 1 };
    RedeResposta r = {0};
    if (rede_pedir(&p, &r)) {
      st = r.status; retry = r.retry_after_s;
      *next = provedorProxima(r.cabecalhos, page);
      body = r.corpo; r.corpo = NULL;
    } else if (r.erro == REDE_INDISPONIVEL) {
      body = rede_baixar_st_retry(url, seconds, headers, &st, &retry);
    }
    rede_resposta_limpar(&r);
  } else
#endif
  body = rede_baixar_st_retry(url, seconds, headers, &st, &retry);
  pthread_mutex_lock(&provedorTrava);
  if (st == 429) {
    provedorRecuo = provedorRecuo ? (provedorRecuo < 60 ? provedorRecuo * 2 : 60) : 5;
    if (provedorRecuo > 60) provedorRecuo = 60;
    time_t until = time(NULL) + (retry > provedorRecuo ? retry : provedorRecuo);
    if (until > __atomic_load_n(&provedorAte, __ATOMIC_ACQUIRE))
      __atomic_store_n(&provedorAte, until, __ATOMIC_RELEASE);
  } else if (st >= 200 && st < 300 && time(NULL) >= __atomic_load_n(&provedorAte, __ATOMIC_ACQUIRE)) provedorRecuo = 0;
  pthread_mutex_unlock(&provedorTrava);
done:
  if (status) *status = st;
  if (st < 200 || st >= 300) { free(body); return NULL; }
  return body;
}
static int provedorEspera(void) {
  return time(NULL) < __atomic_load_n(&provedorAte, __ATOMIC_ACQUIRE);
}
static char *provedorBaixar(const char *u, int s, const char *const *h) {
  return provedorGet(u, s, h, NULL, NULL, 1);
}
static char *provedorBaixarSt(const char *u, int s, const char *const *h, int *st) {
  return provedorGet(u, s, h, st, NULL, 1);
}
#endif
