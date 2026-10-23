// #203: o push de credencial sai do laco principal. sync_empurrar_credencial e
// trocado por um falso que dorme; credfio_iniciar tem de voltar na hora, nao
// deixar dois no ar para o mesmo provedor e entregar o resultado uma vez.
#include "credfio.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int chamadas, simultaneas, maxSimult, retorno = 0;
static char ultJson[64];
static pthread_mutex_t ultTrava = PTHREAD_MUTEX_INITIALIZER;
int sync_empurrar_credencial(const char *p, const char *j) {
  (void)p;
  __sync_fetch_and_add(&chamadas, 1);
  int agoraSim = __sync_add_and_fetch(&simultaneas, 1), m;
  // max atomico: o falso roda em dois fios ao mesmo tempo (TSAN acusava o
  // ternario antigo, que lia e escrevia maxSimult sem trava)
  while ((m = __atomic_load_n(&maxSimult, __ATOMIC_SEQ_CST)) < agoraSim &&
         !__sync_bool_compare_and_swap(&maxSimult, m, agoraSim)) {}
  pthread_mutex_lock(&ultTrava);
  snprintf(ultJson, sizeof ultJson, "%s", j);
  pthread_mutex_unlock(&ultTrava);
  usleep(300000);
  __sync_sub_and_fetch(&simultaneas, 1);
  return retorno;
}
static int falhas;
#define CHECA(c) do { if (!(c)) { printf("FALHOU: %s (linha %d)\n", #c, __LINE__); falhas++; } } while (0)
static double agora(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }

int main(void) {
  int res = 99; double t0 = agora();
  char j[] = "{\"a\":1}";
  retorno = 1;
  CHECA(credfio_iniciar("trakt", j));
  j[2] = 'z';                                    // copia propria: mexer no original nao muda o envio
  CHECA(agora() - t0 < 0.1);                     // voltou sem esperar a rede
  CHECA(!credfio_iniciar("trakt", "{}"));        // um por provedor
  CHECA(credfio_iniciar("simkl", "{\"b\":2}"));  // outro provedor roda junto
  CHECA(!credfio_resultado("trakt", &res));      // ainda no ar
  usleep(500000);
  CHECA(credfio_resultado("trakt", &res) && res == 1);
  CHECA(!credfio_resultado("trakt", &res));      // so uma vez
  CHECA(credfio_resultado("simkl", &res));
  CHECA(chamadas == 2 && maxSimult == 2);
  retorno = 0;
  CHECA(credfio_iniciar("trakt", "{\"a\":1}"));  // liberado: nova tentativa
  usleep(500000);
  CHECA(credfio_resultado("trakt", &res) && res == 0);
  CHECA(strcmp(ultJson, "{\"a\":1}") == 0);
  if (!falhas) printf("credfio: ok\n");
  return falhas != 0;
}
