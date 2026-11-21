// Corrida da flag de diagnostico na troca de faixa. O leitor publica o
// snapshot do fio e a API reescreve s->diagLog sob s->trava a cada selecao;
// ler a flag solta no fio corria com essa escrita (TSan rc 66 em 03d80de03).
// O gancho mkvassTesteDiag so existe neste build: o .sh injeta a chamada em
// uma COPIA de src/mkvass.c e o fio da geracao 1 estaciona na entrada do
// publicador enquanto a thread principal seleciona a segunda faixa — o
// interleaving da corrida. Modos:
//   corrida — interleaving forcado; o detector e o TSan (NUVIO_PGS_DIAGNOSTICS=0).
//   troca   — 60 selecoes 6/7 sem gancho: estresse do mesmo caminho.
//   diag    — NUVIO_PGS_DIAGNOSTICS=1: o snapshot ainda enche (coleta opt-in).
#include "mkvass.h"
#include "imgsub.h"
#include "dados.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "mkvass_pgs_transport.h"

static atomic_int portao;

// Chamado por publicarDiagnostico no build de prova. Ordem relaxada de
// proposito: o TSan ainda ve se os acessos de producao se sincronizam.
void mkvassTesteDiag(unsigned geracao) {
  if (geracao != 1 || atomic_load_explicit(&portao, memory_order_relaxed) != 1) return;
  atomic_store_explicit(&portao, 2, memory_order_relaxed);
  while (atomic_load_explicit(&portao, memory_order_relaxed) == 2) usleep(1000);
}

int main(int argc, char **argv) {
  assert(argc == 3);
  arquivo = argv[1];
  dados_iniciar(".");
  mkvass_aceitar_imagem(1);
  legenda_desligar();
  int corrida = !strcmp(argv[2], "corrida");
  int diag = !strcmp(argv[2], "diag");
  setenv("NUVIO_PGS_DIAGNOSTICS", diag ? "1" : "0", 1);
  if (!strcmp(argv[2], "troca")) {
    for (int i = 0; i < 60; ++i) {
      mkvass_iniciar(esperado, i % 2 ? 7 : 6);
      mkvass_passo(600.5);
      mkvass_folga(30);
      usleep(10000);
    }
    parar();
    return 0;
  }
  assert(corrida || diag);
  if (corrida) atomic_store_explicit(&portao, 1, memory_order_relaxed);
  pthread_mutex_lock(&trava);
  mkvass_iniciar(esperado, 6);
  mkvass_passo(600.5);
  mkvass_folga(30);
  pthread_mutex_unlock(&trava);
  double fim = now() + 3;
  if (corrida) {
    while (atomic_load_explicit(&portao, memory_order_relaxed) != 2 && now() < fim) usleep(1000);
    assert(atomic_load_explicit(&portao, memory_order_relaxed) == 2);
    // Selecao normal de faixa: mesmo MkvassPreparo, geracao nova sob s->trava.
    mkvass_iniciar(esperado, 7);
    atomic_store_explicit(&portao, 3, memory_order_relaxed);
    usleep(300000);
    parar();
    return 0;
  }
  // diag: com a flag ligada os contadores do leitor enchem o snapshot.
  ImgsubQuadro *q = NULL;
  while (!(q = imgsub_pegar(600.5)) && now() < fim) usleep(1000);
  assert(q && q->n);
  imgsub_soltar(q);
  mkvass_passo(601.0);
  usleep(50000);
  MkvassDiagnostico d;
  mkvass_diagnostico(&d);
  printf("snapshot reader=%.3f byte=%lld packets=%llu emitted=%llu requests=%ld bytes=%ld\n",
         d.reader_seconds, d.reader_byte, (unsigned long long)d.packets,
         (unsigned long long)d.emitted, d.requests, d.network_bytes);
  assert(d.packets > 0 && d.emitted > 0);
  parar();
  return 0;
}
