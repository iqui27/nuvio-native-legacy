#include "stub_perfisessao.h"
// tests/xtepg.sh: a fila da grade curta do Xtream contra um painel FALSO que
// devolve 429 acima de PAINEL_MAX pedidos por segundo (o que a TCL do dono
// mediu: "50 pedido(s) ... 6 falha(s) (ultimo HTTP 429)").
#include "stub_perfisessao.h"
#include "../src/xtepg.c"
#include <assert.h>
#include <sys/time.h>
#define PAINEL_MAX 2           // pedidos por janela de 1 s
static pthread_mutex_t pt = PTHREAD_MUTEX_INITIALIZER;
static double hist[4096]; static int nHist, n429, nOk, emVoo, maxVoo, comRetry;
static double agoraS(void) { struct timeval t; gettimeofday(&t, NULL); return t.tv_sec + t.tv_usec / 1e6; }
int xtream_e_id(const char *id) { return id && !strncmp(id, "xtream:", 7); }
static int ra;
int xtream_ultimo_retry_after(void) { return ra; }
int xtream_epg_curto(const char *id, XtreamProg *out, int cap, int *st) {
  double t = agoraS();
  int i, janela = 0;
  pthread_mutex_lock(&pt);
  if (++emVoo > maxVoo) maxVoo = emVoo;
  for (i = 0; i < nHist; i++) if (t - hist[i] < 1.0) janela++;
  hist[nHist++ % 4096] = t;
  pthread_mutex_unlock(&pt);
  usleep(30000);
  pthread_mutex_lock(&pt); emVoo--; pthread_mutex_unlock(&pt);
  if (janela >= PAINEL_MAX) {
    n429++;
    ra = (n429 % 2) ? 1 : 0;     // metade com Retry-After: 1, metade sem
    if (ra) comRetry++;
    *st = 429; return -1;
  }
  ra = 0;
  nOk++;
  *st = 200;
  snprintf(out[0].titulo, sizeof out[0].titulo, "Programa de %s", id);
  out[0].ini = time(NULL) - 60; out[0].fim = time(NULL) + 3600;
  (void)cap;
  return 1;
}
int main(void) {
  char id[24];
  int i, k, prontos = 0;
  double t0 = agoraS();
  // 20 canais de uma vez, como o guia rolando.
  for (i = 0; i < 20; i++) { snprintf(id, sizeof id, "xtream:%d", 100 + i); xtepg_querer(id); }
  for (k = 0; k < 600 && prontos < 20; k++) {
    usleep(100000);
    xtepg_passo();
    for (prontos = 0, i = 0; i < 20; i++) { snprintf(id, sizeof id, "xtream:%d", 100 + i); prontos += xtepg_tem(id); }
  }
  printf("prontos %d/20 em %.1f s | 200=%d 429=%d (com Retry-After %d) | max em voo %d | falhas %d\n",
         prontos, agoraS() - t0, nOk, n429, comRetry, maxVoo, logFalhas);
  assert(prontos == 20);              // todos chegam, mesmo com o painel recusando
  assert(logFalhas == 0);             // 429 nao vira falha: volta para a fila
  assert(maxVoo == 1);                // um pedido por vez
  assert(n429 <= 3);                  // o ritmo se ajusta: poucos 429, nao uma rajada
  // CACHE: pedir de novo nao vai ao painel.
  { int antes = nOk + n429;
    for (i = 0; i < 20; i++) { snprintf(id, sizeof id, "xtream:%d", 100 + i); xtepg_querer(id); }
    usleep(800000); xtepg_passo();
    assert(nOk + n429 == antes); }
  // PRIORIDADE: o ultimo pedido (o canal que acabou de aparecer) sai primeiro.
  { xtepg_limpar();
    for (i = 0; i < 5; i++) { snprintf(id, sizeof id, "xtream:%d", 300 + i); xtepg_querer(id); }
    for (k = 0; k < 40 && !xtepg_tem("xtream:304"); k++) { usleep(100000); xtepg_passo(); }
    assert(xtepg_tem("xtream:304") && !xtepg_tem("xtream:300")); }
  puts("xtepg: tudo ok");
  return 0;
}
