// O RESUMO DO SYNC (linha da tela de ajustes) E LIDO NO QUADRO E ESCRITO NO FIO.
//
// resumoTipo/resumoHttp/resumoTrakt/resumoPend/resumoN[5]/resumoData/resumo eram
// globais escritos pelo fio do ciclo (rodar) e lidos SEM trava por
// sync_resumo() no quadro: leitura rasgada (tipo novo com numeros velhos, ou o
// texto fixo no meio de um snprintf). Aqui o fio de verdade de sync.c roda
// ciclos seguidos (os dubles de tests/syncordem.c, main dele renomeado)
// enquanto o "quadro" le sync_resumo() sem parar. Com TSan: nenhuma corrida
// pode passar por sync_resumo. tests/sync_resumo_corrida.sh confere o relatorio.
#define main syncordem_main
#include "syncordem.c"
#undef main

static volatile int pararLeitor;
static int lidas;
static char copia[300];
// O quadro da tela de ajustes: le a linha o tempo todo, sem nenhuma outra
// sincronizacao com o fio do ciclo.
static void *leitor(void *u) {
  (void)u;
  while (!__atomic_load_n(&pararLeitor, __ATOMIC_ACQUIRE)) {
    snprintf(copia, sizeof copia, "%s", sync_resumo());
    lidas++;
    usleep(50);
  }
  return NULL;
}

int main(void) {
  int c;
  pthread_t th;
  setvbuf(stdout, NULL, _IOLBF, 0);
  escolher(2);
  pthread_create(&th, NULL, leitor, NULL);
  for (c = 0; c < 6; c++) { sync_iniciar(); ateTerminar(); }
  __atomic_store_n(&pararLeitor, 1, __ATOMIC_RELEASE);
  pthread_join(th, NULL);
  printf("sync_resumo_corrida: %d leituras; ultima: %s\n", lidas, copia);
  return 0;
}
