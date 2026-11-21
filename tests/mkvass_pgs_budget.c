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
static void start(void) {
  pthread_mutex_lock(&trava);
  mkvass_iniciar(esperado,6); mkvass_passo(600); mkvass_folga(3);
  pthread_mutex_unlock(&trava);
}
static void waiting(void) {
  double end=now()+2; MkvassDiagnostico d;
  do {
    mkvass_diagnostico(&d);
    if(d.scan_state==MKVASS_SCAN_BUDGET && d.reader_seconds==600) return;
    usleep(1000);
  } while(now()<end);
  assert(!"reader did not reach budget wait");
}
int main(int argc,char **argv) {
  assert(argc==2);arquivo=argv[1];dados_iniciar(".");
  mkvass_aceitar_imagem(1);legenda_desligar();
  double begin=now();start();waiting();
  ImgsubQuadro *q=NULL;
  while(now()-begin<8) { q=imgsub_pegar(600.5);if(q)break;usleep(1000); }
  assert(q && q->n==1);imgsub_soltar(q);
  double elapsed=now()-begin;
  assert(elapsed>=4.9 && elapsed<8);
  pthread_mutex_lock(&trava);
  assert(pedidos==4); // Header + Cues + two serial scan ranges.
  assert(request_lengths[2]<=65536 && request_lengths[3]<=65536);
  assert(request_times[3]-request_times[2]>=4.9);
  pthread_mutex_unlock(&trava);
  parar();
  start();waiting();
  begin=now();parar();assert(now()-begin<0.5);
  // Seek interrupts the budget wait. Raising headroom then releases reads
  // for the new window; no old-position cue may be queued from that wait.
  start();waiting();mkvass_passo(0);mkvass_folga(30);
  double end=now()+2;MkvassDiagnostico d;
  do { mkvass_diagnostico(&d);if(d.reader_seconds==120)break;usleep(1000); } while(now()<end);
  assert(d.reader_seconds==120 && d.packets==0);parar();
  printf("production PGS pacing: %.3f s; cancellation and seek ok\n",elapsed);
}
