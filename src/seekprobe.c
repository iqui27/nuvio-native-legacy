#include "seekprobe.h"
#include "rede.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int seekprobe_sem_range(const char *url, const char *headers) {
  long n = 0; int status = 0; char *b;
  if (!url || strstr(url, ".m3u8")) return 0;
  b = rede_baixar_trecho64_cab(url, headers, 0, 1, &n, NULL, &status, NULL);
  free(b);
  return status == 200;
}

static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static char urlVista[2048], cabVista[2048];
static int resultado = -1;

static void *fio(void *a) {
  char u[sizeof urlVista], c[sizeof cabVista]; int r;
  (void)a;
  pthread_mutex_lock(&mu); memcpy(u, urlVista, sizeof u); memcpy(c, cabVista, sizeof c); pthread_mutex_unlock(&mu);
  r = seekprobe_sem_range(u, c);
  pthread_mutex_lock(&mu);
  if (!strcmp(u, urlVista)) resultado = r;   // source changed meanwhile: drop it
  pthread_mutex_unlock(&mu);
  return NULL;
}

void seekprobe_iniciar(const char *url, const char *headers) {
  pthread_t t;
  if (!url || !url[0] || strlen(url) >= sizeof urlVista) return;
  pthread_mutex_lock(&mu);
  if (!strcmp(url, urlVista)) { pthread_mutex_unlock(&mu); return; }
  snprintf(urlVista, sizeof urlVista, "%s", url);
  snprintf(cabVista, sizeof cabVista, "%s", headers ? headers : "");
  resultado = -1;
  pthread_mutex_unlock(&mu);
  if (pthread_create(&t, NULL, fio, NULL) == 0) pthread_detach(t);
}

int seekprobe_resultado(const char *url) {
  int r;
  pthread_mutex_lock(&mu);
  r = url && !strcmp(url, urlVista) ? resultado : -1;
  pthread_mutex_unlock(&mu);
  return r;
}
