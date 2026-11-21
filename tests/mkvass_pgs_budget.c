// Production pacing constants, without shortened test deadlines in the reader.
#include "mkvass.h"
#include "imgsub.h"
#include "dados.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "mkvass_pgs_transport.h"
// Offset do Cluster cujo Timestamp vale `ms`: o elemento E7 sai com
// tamanho fixo de 4 B na fixture; os 8 B anteriores sao id+tamanho dele.
static long long achar_cluster(const char *path, unsigned long ms) {
  FILE *fp = fopen(path, "rb"); assert(fp);
  fseek(fp, 0, SEEK_END); long n = ftell(fp); rewind(fp);
  unsigned char *buf = malloc(n); assert(buf);
  assert(fread(buf, 1, n, fp) == (size_t)n); fclose(fp);
  unsigned char agulha[9] = {0xe7, 0x10, 0x00, 0x00, 0x04,
                             (unsigned char)(ms >> 24), (unsigned char)(ms >> 16),
                             (unsigned char)(ms >> 8), (unsigned char)ms};
  long long r = -1;
  for (long i = 8; i + 9 <= n; i++) {
    if (memcmp(buf + i, agulha, 9)) continue;
    assert(buf[i-8] == 0x1f && buf[i-7] == 0x43 && buf[i-6] == 0xb6 && buf[i-5] == 0x75);
    r = i - 8; break;
  }
  free(buf);
  return r;
}
static void start(void) {
  pthread_mutex_lock(&trava);
  mkvass_iniciar(esperado,6); mkvass_passo(600); mkvass_folga(3);
  pthread_mutex_unlock(&trava);
}
static void waiting(void) {
  double end=now()+6; MkvassDiagnostico d;
  do {
    mkvass_diagnostico(&d);
    if(d.scan_state==MKVASS_SCAN_BUDGET && d.reader_seconds==660) return;
    usleep(1000);
  } while(now()<end);
  assert(!"reader did not reach budget wait");
}
int main(int argc,char **argv) {
  assert(argc==2);arquivo=argv[1];dados_iniciar(".");
  mkvass_aceitar_imagem(1);legenda_desligar();
  double begin=now();start();waiting();
  ImgsubQuadro *q=NULL;
  while(now()-begin<8) { q=imgsub_pegar(660.5);if(q)break;usleep(1000); }
  assert(q && q->n==1);imgsub_soltar(q);
  double elapsed=now()-begin;
  assert(elapsed>=4.9 && elapsed<8);
  pthread_mutex_lock(&trava);
  // Header + Cues + varredura de @600 (livre: o playhead ja entrou nele).
  // TODO pedido que comeca num Cluster futuro (a partir do de 660 s) fica no
  // orcamento: <= 64 KiB e uma espera de ritmo cheia depois do ultimo pedido
  // cobrado — o primeiro da varredura ou o pedido futuro anterior.
  long long futuro = achar_cluster(arquivo, 660000);
  assert(futuro >= 0);
  int cobrado = 2, vistos = 0;
  for (int i = 3; i < pedidos; ++i) {
    if (request_offsets[i] < futuro) continue;
    assert(request_lengths[i] <= 65536);
    assert(request_times[i] - request_times[cobrado] >= 4.9);
    cobrado = i; vistos++;
  }
  assert(vistos >= 1);
  pthread_mutex_unlock(&trava);
  parar();
  start();waiting();
  begin=now();parar();assert(now()-begin<0.5);
  // Seek interrupts the budget wait. Raising headroom then releases reads
  // for the new window; no old-position cue may be queued from that wait —
  // a contagem de pacotes nao pode crescer depois do salto.
  start();waiting();
  MkvassDiagnostico d; mkvass_diagnostico(&d);
  unsigned long long pacotes = d.packets;
  mkvass_passo(0);mkvass_folga(30);
  double end=now()+2;
  do { mkvass_diagnostico(&d);if(d.reader_seconds==120)break;usleep(1000); } while(now()<end);
  assert(d.reader_seconds==120 && d.packets==pacotes);parar();
  printf("production PGS pacing: %.3f s; cancellation and seek ok\n",elapsed);
}
