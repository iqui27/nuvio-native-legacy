#include "farolsonda.h"
#include "farolanel.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int aberto, mini;
int player_aberto(void) { return aberto; }
int player_mini_ativo(void) { return mini; }
static long agora(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000L + t.tv_nsec / 1000000;
}
static FarolSonda esperar(void) {
  FarolSonda s;
  long inicio = agora();
  while (!farolsonda_estado(&s)) {
    assert(agora() - inicio < 5000);
    usleep(1000);
  }
  FarolEvento eventos[FAROL_ANEL_N];
  assert(farolanel_ler(eventos, FAROL_ANEL_N, 0) == 0);
  return s;
}
int main(int argc, char **argv) {
  assert(argc == 2 || argc == 3);
  if (argc == 3) {
    FarolSonda lenta;
    long inicio = agora();
    assert(farolsonda_iniciar(argv[1], NULL));
    lenta = esperar();
    assert(agora() - inicio >= 3800 && agora() - inicio < 4500);
    assert(lenta.internet_http == 0 && lenta.http == 0 && lenta.range == -1);
    assert(farolsonda_iniciar(argv[1], NULL));
    usleep(100000);
    inicio = agora();
    farolsonda_cancelar();
    lenta = esperar();
    assert(lenta.cancelada && agora() - inicio < 300);
    return 0;
  }
  char url[256];
  const char *paths[] = {"/range", "/ignored", "/forbidden", "/slow",
                         "/redirect-empty", "/redirect-body"};
  const int statuses[] = {206, 200, 403, 0, 206, 206};
  const int ranges[] = {1, 0, 0, -1, 1, 1};
  FarolSonda s;
  assert(farolsonda_estado(&s));
  assert(s.range == -1 && s.dns_ms == -1 && s.tls_ms == -1);
  assert(!farolsonda_iniciar(NULL, NULL));
  aberto = 1;
  assert(!farolsonda_iniciar(argv[1], NULL));
  aberto = 0;
  mini = 1;
  assert(!farolsonda_iniciar(argv[1], NULL));
  mini = 0;
  for (int i = 0; i < 6; i++) {
    snprintf(url, sizeof url, "%s%s", argv[1], paths[i]);
    char header[] = "X-Probe: synthetic";
    const char *headers[] = {header, NULL};
    long inicio = agora();
    assert(farolsonda_iniciar(url, headers));
    memset(header, 'x', strlen(header));
    memset(url, 0, sizeof url);
    assert(!farolsonda_iniciar(argv[1], NULL));
    s = esperar();
    assert(s.http == statuses[i]);
    assert(s.internet_http == 200);
    assert(s.range == ranges[i]);
    assert(!s.cancelada);
    assert(agora() - inicio < (i == 3 ? 2500 : 2000));
    if (i == 3) assert(agora() - inicio >= 1800);
  }
  snprintf(url, sizeof url, "%s/slow", argv[1]);
  for (int i = 0; i < 3; i++) {
    assert(farolsonda_iniciar(url, NULL));
    usleep(100000);
    long inicio = agora();
    if (i == 2) mini = 1;
    else if (i) aberto = 1;
    else farolsonda_cancelar();
    s = esperar();
    assert(agora() - inicio < 300);
    assert(s.cancelada);
    aberto = 0;
    mini = 0;
  }
  farolsonda_cancelar();
  return 0;
}
