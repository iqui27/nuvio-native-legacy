// Durable budget: concurrent reservations, new processes, UTC rollover,
// rollback, storage failure and corrupt data. No provider requests.
#include "dados.h"
#include "nvtmp.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static int falhaEscrita;
static int quotaGravar(const char *nome, const char *texto) {
  return falhaEscrita ? 0 : dados_gravar(nome, texto);
}
#define dados_gravar quotaGravar
#include "../src/seekrquota.c"
#undef dados_gravar

static const long long UTC = 2000000000LL;
static int resultados[100];
static void *reserva(void *p) {
  resultados[(size_t)p] = seekrquota_reservar(UTC);
  return NULL;
}
static void uso(int usadas, long long instante, int atrasado) {
  SeekrQuotaUso u;
  seekrquota_uso(instante, &u);
  assert(u.limite == 50 && u.usadas == usadas && u.restantes == 50 - usadas);
  assert(u.relogioAtrasado == atrasado);
}
int main(int argc, char **argv) {
  dados_iniciar(nvt(""));
  if (argc > 1) {
    uso(3, UTC + 86400, 0);
    uso(3, UTC, 1);
    assert(seekrquota_reservar(UTC) == SEEKR_QUOTA_OK);
    uso(4, UTC, 1);
    return 0;
  }
  assert(seekrquota_reservar(0) == SEEKR_QUOTA_RELOGIO);
  pthread_t fios[100];
  for (size_t i = 0; i < 100; i++) assert(!pthread_create(&fios[i], NULL, reserva, (void *)i));
  int aprovadas = 0;
  for (int i = 0; i < 100; i++) {
    pthread_join(fios[i], NULL);
    aprovadas += resultados[i] == SEEKR_QUOTA_OK;
    assert(resultados[i] == SEEKR_QUOTA_OK || resultados[i] == SEEKR_QUOTA_LIMITE);
  }
  assert(aprovadas == 50);
  uso(50, UTC, 0);
  assert(seekrquota_reservar(UTC - 86400) == SEEKR_QUOTA_LIMITE);
  uso(50, UTC - 86400, 1);
  long long proximo = (UTC / 86400 + 1) * 86400;
  SeekrQuotaUso u;
  seekrquota_uso(UTC, &u); assert(u.reinicioUtc == proximo);
  for (int i = 0; i < 3; i++) assert(seekrquota_reservar(UTC + 86400) == SEEKR_QUOTA_OK);
  uso(3, UTC + 86400, 0);
  pid_t p = fork(); assert(p >= 0);
  if (!p) { execl(argv[0], argv[0], "restart", NULL); _exit(99); }
  int st; assert(waitpid(p, &st, 0) == p && WIFEXITED(st) && WEXITSTATUS(st) == 0);
  assert(seekrquota_reservar(UTC) == SEEKR_QUOTA_OK); // Re-read the child's reservation.
  uso(5, UTC, 1);
  falhaEscrita = 1;
  assert(seekrquota_reservar(UTC + 86400) == SEEKR_QUOTA_ARMAZENAMENTO);
  seekrquota_uso(UTC + 86400, &u); assert(!u.persistente && !u.restantes);
  falhaEscrita = 0;
  assert(seekrquota_reservar(UTC + 86400) == SEEKR_QUOTA_OK);
  uso(6, UTC + 86400, 0);
  assert(dados_gravar(QUOTA_ARQUIVO, "partial ledger"));
  assert(seekrquota_reservar(UTC + 172800) == SEEKR_QUOTA_ARMAZENAMENTO);
  seekrquota_uso(UTC + 172800, &u); assert(!u.persistente && !u.restantes);
  puts("seekrquota: PASS concurrency, restart, UTC rollover, rollback, write failure, corruption");
  return 0;
}
