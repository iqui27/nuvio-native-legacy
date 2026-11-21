/* Duas sondas de cabecalho simultaneas (mkv_faixas_do_trecho) sobre os mesmos
 * bytes: um fio destacado da geracao anterior (video_tpk.c fioMkv) ainda pode
 * estar varrendo quando a sonda do titulo novo comeca. A classificacao DTS de
 * cada sonda fica no contexto dela e so o resultado pronto e publicado — sob
 * TSan nenhum campo DTS pode aparecer numa corrida. As corridas preexistentes
 * dos outros globais do parser nao sao objeto deste teste. */
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include "mkv.h"

/* A sonda do trecho nao toca a rede; os stubs so fecham o link do mkv.c. */
char *rede_baixar_trecho(const char *u, int s, long i, long f, long *t) {
  (void)u; (void)s; (void)i; (void)f; *t = 0; return NULL;
}
char *rede_baixar_trecho_st(const char *u, int s, long long i, long long f, long *t,
                            int *st, int *erro, char *fin, unsigned tamFin) {
  (void)u; (void)s; (void)i; (void)f; (void)fin; (void)tamFin;
  *t = 0; if (st) *st = 0; if (erro) *erro = 0; return NULL;
}

static pthread_barrier_t largada;
static unsigned char cabecalho[1 << 20];
static long tamanho;

static void *sonda(void *arg) {
  (void)arg;
  MkvFaixa fx[MKV_MAX_FAIXAS];
  pthread_barrier_wait(&largada);
  for (int vez = 0; vez < 24; vez++) {
    int n = mkv_faixas_do_trecho(cabecalho, tamanho, fx, MKV_MAX_FAIXAS, NULL, 0, NULL);
    assert(n >= 1);
  }
  return NULL;
}

int main(int argc, char **argv) {
  assert(argc == 2);
  FILE *f = fopen(argv[1], "rb");
  assert(f);
  tamanho = (long)fread(cabecalho, 1, sizeof cabecalho, f);
  fclose(f);
  assert(tamanho > 1024);
  { MkvFaixa fx[MKV_MAX_FAIXAS];
    int n = mkv_faixas_do_trecho(cabecalho, tamanho, fx, MKV_MAX_FAIXAS, NULL, 0, NULL), audio = 0;
    assert(n >= 1);
    for (int i = 0; i < n; i++) audio += fx[i].tipo == 2;
    assert(audio >= 1); }
  pthread_t fio;
  assert(pthread_barrier_init(&largada, NULL, 2) == 0);
  assert(pthread_create(&fio, NULL, sonda, NULL) == 0);
  sonda(NULL);
  assert(pthread_join(fio, NULL) == 0);
  pthread_barrier_destroy(&largada);
  puts("mkv_sonda_concorrente: sondas concluidas");
  return 0;
}
