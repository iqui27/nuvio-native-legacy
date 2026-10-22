// CORRIDA NO MAPA DE EPISODIOS VISTOS (vistoep.c). O fio de extras escreve no
// mapa (vistoep_definir, crescendo o vetor com realloc) enquanto o desenho do
// detalhe le (vistoep_estado). Sem trava, o ASAN acusou heap-use-after-free em
// achar() no app inteiro no Mac. Aqui: um fio escreve ate o teto, dois leem e
// um terceiro tambem escreve (o fio principal marca episodios). Rode com
// SANITIZE=1 (ASAN) ou SANITIZE=thread (TSAN) para a prova ter dentes.
#include "vistoep.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>

static atomic_int fim;

static void *escrever(void *u) {
  const char *id = u;
  int t, e;
  for (t = 1; t <= 40; t++)
    for (e = 1; e <= 100; e++) vistoep_definir(id, t, e, (t + e) & 1);
  return NULL;
}

static void *ler(void *u) {
  int i = 0;
  (void)u;
  while (!fim) {
    vistoep_estado("tt0000001", 1 + i % 40, 1 + i % 100);
    vistoep_contar("tt0000002");
    vistoep_conhecido("tt0000003");
    i++;
  }
  return NULL;
}

int main(void) {
  pthread_t w1, w2, r1, r2;
  int rodada;
  for (rodada = 0; rodada < 20; rodada++) {
    fim = 0;
    pthread_create(&r1, NULL, ler, NULL);
    pthread_create(&r2, NULL, ler, NULL);
    pthread_create(&w1, NULL, escrever, "tt0000001");
    pthread_create(&w2, NULL, escrever, "tt0000002");
    pthread_join(w1, NULL);
    pthread_join(w2, NULL);
    fim = 1;
    pthread_join(r1, NULL);
    pthread_join(r2, NULL);
    assert(vistoep_n() == 8000);          // o teto, sem perder nem duplicar
    assert(vistoep_estado("tt0000001", 1, 2) == 1);
    vistoep_esquecer();
  }
  printf("vistoep_corrida: ok\n");
  return 0;
}
