// perdifio: o POST ao Gemini e cancelado de verdade (rede_parar_quando) e
// perdifio_encerrar junta os fios. Rede falsa: segura ate o parar() mandar.
#include "perdifio.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static _Thread_local int (*pararFn)(void *);
static _Thread_local void *pararU;
static volatile int entrou, abortados, respondidos, segurar = 1;
void rede_parar_quando(int (*p)(void *), void *u) { pararFn = p; pararU = u; }
char *rede_postar_seguro_st(const char *url, int segundos, const char *const *cab,
                            const char *corpo, int *status) {
  (void)url; (void)segundos; (void)corpo;
  __sync_fetch_and_add(&entrou, 1);
  assert(cab && cab[0] && !strncmp(cab[0], "x-goog-api-key: ", 16));
  for (int i = 0; i < 500; i++) {                 // ate 5 s, como uma rede parada
    if (pararFn && pararFn(pararU)) { __sync_fetch_and_add(&abortados, 1); return NULL; }
    if (!segurar) break;
    usleep(10000);
  }
  __sync_fetch_and_add(&respondidos, 1);
  *status = 200;
  return strdup("{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Ana procura o Rui.\"}]}}]}");
}

static double agora(void) {
  struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
  return (double)t.tv_sec + t.tv_nsec / 1e9;
}

int main(void) {
  PerdiResultado r; char txt[PERDI_RESUMO_MAX];
  // Sem chave ou corpo: nao dispara (e libera o corpo).
  assert(perdifio_pedir("", strdup("{}")) == 0);
  assert(perdifio_pedir("k", NULL) == 0);

  // Cancelar aborta o POST em voo (nao espera os 45 s/5 s da rede).
  unsigned a = perdifio_pedir("AIzaTeste", strdup("{}"));
  assert(a && perdifio_estado(a, NULL, NULL, 0) == PERDIFIO_CARREGANDO);
  double t0 = agora();
  while (entrou < 1 && agora() - t0 < 2.0) usleep(1000);   // o POST esta em voo
  t0 = agora();
  perdifio_cancelar();
  assert(perdifio_estado(a, NULL, NULL, 0) == PERDIFIO_NADA);
  while (!abortados && agora() - t0 < 2.0) usleep(5000);
  assert(abortados == 1 && agora() - t0 < 1.0);

  // Pedido novo descarta o anterior; o novo responde.
  unsigned b = perdifio_pedir("AIzaTeste", strdup("{}"));
  while (entrou < 2 && agora() - t0 < 2.0) usleep(1000);
  unsigned c = perdifio_pedir("AIzaTeste", strdup("{}"));
  assert(b && c && c != b);
  while (abortados < 2 && agora() - t0 < 3.0) usleep(5000);
  assert(abortados == 2);
  segurar = 0;
  while (perdifio_estado(c, &r, txt, sizeof txt) != PERDIFIO_PRONTO && agora() - t0 < 3.0) usleep(5000);
  assert(perdifio_estado(c, &r, txt, sizeof txt) == PERDIFIO_PRONTO && r == PERDI_OK);
  assert(!strcmp(txt, "Ana procura o Rui."));
  assert(perdifio_estado(b, NULL, NULL, 0) == PERDIFIO_NADA);

  // Encerrar: cancela o que esta em voo e JUNTA (volta rapido, nada vivo).
  segurar = 1;
  int antes = entrou;
  unsigned d = perdifio_pedir("AIzaTeste", strdup("{}"));
  assert(d);
  t0 = agora();
  while (entrou == antes && agora() - t0 < 2.0) usleep(1000);
  t0 = agora();
  perdifio_encerrar();
  assert(agora() - t0 < 1.0 && abortados == 3);
  // Depois de encerrar, nada dispara.
  assert(perdifio_pedir("AIzaTeste", strdup("{}")) == 0);
  puts("perdifio ok");
  return 0;
}
