/* Regressao F92 (PR 749): a classificacao DTS publicada (mkv_dts_kind) nao pode
 * ser sobrescrita por trabalho de uma geracao/sonda anterior.
 *
 * Compila o CORPO REAL de fioMkv (src/video_tpk.c), extraido pelo .sh para
 * worker.c, com src/mkv.c real e so as bordas de plataforma stubadas.
 *
 * Cenarios (argv[1]):
 *   before-parser  o worker TPK e escalonado so depois da sonda do titulo novo:
 *                  sem a geracao na publicacao, ele reserva sequencia maior e
 *                  rebaixa DTS-HD MA (3) para DTS Core (1) — rc 1 no pai.
 *   inside-parser  o worker ja reservou a sequencia e ficou parado na rede: a
 *                  conferencia de sequencia ja bastava — rc 0 nas duas pontas.
 *   long-read      a leitura complementar (mkv_dts_alem) aplica bytes do
 *                  cabecalho ANTIGO sobre a sonda nova publicada durante o
 *                  I/O — rc 1 no pai.
 *
 * As barreiras ordenam os cruzamentos e criam happens-before para o TSan:
 * mkvGeracao so muda entre worker_entered e release_worker.
 */
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mkv.h"
#include "mkv_dts.h"

#define URL_MAX 128
typedef struct { char url[URL_MAX]; unsigned geracao; } PedidoMkv;
static _Atomic unsigned mkvGeracao = 1;
static int mkvNaoMkv, mkvEstado, stored_tracks;
static pthread_barrier_t worker_entered, release_worker;
static int delay_at_network;
static unsigned char old_header[1 << 20], active_header[1 << 20];
static long old_size, active_size;

static void wait_at(pthread_barrier_t *barrier) {
  int rc = pthread_barrier_wait(barrier);
  assert(rc == 0 || rc == PTHREAD_BARRIER_SERIAL_THREAD);
}
static void delay_worker(void) {
  wait_at(&worker_entered);
  wait_at(&release_worker);
}
static void rede_lateral(int value) { assert(value == 1); if (!delay_at_network) delay_worker(); }
static void guardarMkv(const MkvFaixa *tracks, int count) { assert(tracks && count > 0); stored_tracks += count; }

char *rede_baixar_trecho(const char *url, int timeout, long first, long last, long *size) {
  (void)timeout; (void)first; (void)last;
  assert(!strcmp(url, "fixture://old"));
  if (delay_at_network) delay_worker();
  char *result = malloc(old_size);
  assert(result);
  memcpy(result, old_header, old_size);
  *size = old_size;
  return result;
}
char *rede_baixar_trecho_st(const char *url, int timeout, long long first, long long last, long *size,
                            int *status, int *error, char *final, unsigned final_size) {
  (void)url; (void)timeout; (void)first; (void)last; (void)status; (void)error; (void)final; (void)final_size;
  *size = 0;
  return NULL;
}
#include "worker.c"

static long read_file(const char *path, unsigned char *bytes) {
  FILE *file = fopen(path, "rb");
  assert(file);
  long size = fread(bytes, 1, 1 << 20, file);
  fclose(file);
  assert(size > 1024);
  return size;
}

static void *longer_probe(void *unused) {
  (void)unused;
  MkvFaixa tracks[MKV_MAX_FAIXAS];
  int count = mkv_faixas_do_trecho(old_header, 1024, tracks, MKV_MAX_FAIXAS, NULL, 0, NULL);
  assert(count > 0);
  assert(mkv_dts_alem("fixture://old", tracks, count) == 1);
  return NULL;
}

int main(int argc, char **argv) {
  assert(argc == 4);
  int longer = !strcmp(argv[1], "long-read");
  delay_at_network = longer || !strcmp(argv[1], "inside-parser");
  old_size = read_file(argv[2], old_header);
  active_size = read_file(argv[3], active_header);
  // The active prefetch contains Tracks, without a complete audio packet.
  assert(active_size > 1024);
  active_size = 1024;
  assert(!pthread_barrier_init(&worker_entered, NULL, 2));
  assert(!pthread_barrier_init(&release_worker, NULL, 2));
  PedidoMkv *request = calloc(1, sizeof *request);
  assert(request);
  strcpy(request->url, "fixture://old");
  request->geracao = mkvGeracao;
  pthread_t thread;
  if (longer) {
    free(request);
    assert(!pthread_create(&thread, NULL, longer_probe, NULL));
  } else {
    assert(!pthread_create(&thread, NULL, fioMkv, request));
  }
  wait_at(&worker_entered);
  mkv_geracao_avancar(&mkvGeracao);
  MkvFaixa tracks[MKV_MAX_FAIXAS];
  int count = mkv_faixas_do_trecho(active_header, active_size, tracks, MKV_MAX_FAIXAS, NULL, 0, NULL);
  int track = -1;
  for (int i = 0; i < count; i++) if (tracks[i].tipo == 2) track = tracks[i].numero;
  assert(track > 0);
  int before = mkv_dts_kind(track);
  assert(before == NV_DTS_KIND_MA);
  wait_at(&release_worker);
  assert(!pthread_join(thread, NULL));
  int after = mkv_dts_kind(track);
  assert(!stored_tracks);
  printf("%s: active track=%d before=%d after=%d tracks_published_by_obsolete_worker=%d\n",
         argv[1], track, before, after, stored_tracks);
  pthread_barrier_destroy(&worker_entered);
  pthread_barrier_destroy(&release_worker);
  return before == after ? 0 : 1;
}
