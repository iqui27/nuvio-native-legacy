// Retomada no instante exato do Cluster (pausa com varreTs == pos). A folga
// cai de 30 para 19,75 s enquanto o leitor varre o Cluster onde o playhead
// esta parado: o ritmo NAO pode frear essa leitura, ou a PGS de 400 ms
// (600,0-600,4 s) expira antes de aparecer — a reproducao volta a 1x depois
// de 1,3 s, entao o quadro tem ate 1,7 s de relogio para chegar.
// Em 03d80de03 o 4o Range era cortado para 64 KiB e o quadro so saia a
// 1,857 s (rc 1). Transporte Range local, sem rede.
#include "mkvass.h"
#include "imgsub.h"
#include "legenda.h"
#include "dados.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#define rede_baixar_trecho_st transporte_fixture
#include "mkvass_pgs_transport.h"
#undef rede_baixar_trecho_st

char *rede_baixar_trecho_st(const char *url, int timeout, long long ini,
                           long long fim, long *tam, int *status, int *erro,
                           char *final, unsigned tamFinal) {
  pthread_mutex_lock(&trava);
  int primeira_varredura = pedidos == 2;
  pthread_mutex_unlock(&trava);
  char *corpo = transporte_fixture(url, timeout, ini, fim, tam, status, erro, final, tamFinal);
  if (primeira_varredura) mkvass_folga(19.75);
  return corpo;
}

static int ler_quadro(double pos) {
  unsigned dono = imgsub_dono_atual();
  ImgsubQuadro *quadro = legenda_ligada_em(dono) ? imgsub_pegar(pos) : NULL;
  int visivel = quadro && quadro->n > 0;
  imgsub_soltar(quadro);
  return visivel;
}

int main(int argc, char **argv) {
  assert(argc == 3);
  arquivo = argv[1];
  double folga = atof(argv[2]);
  int visivel = 0, retrospectivo = 0;
  double atraso = -1;
  dados_iniciar(".");
  mkvass_aceitar_imagem(1);
  legenda_desligar();
  double inicio = now();
  pthread_mutex_lock(&trava);
  mkvass_iniciar(esperado, 6);
  mkvass_passo(600);
  mkvass_folga(folga);
  pthread_mutex_unlock(&trava);

  double tempo = 0;
  while (tempo < 6.5) {
    tempo = now() - inicio;
    // Pausa de 1,3 s exatamente em 600,0 s (inicio do Cluster) e retomada a
    // 1x. mkvass_passo so publica de 0,5 em 0,5 s: o leitor ve pos==600 por
    // toda a janela da legenda.
    double playhead = 600.0 + (tempo > 1.3 ? tempo - 1.3 : 0);
    mkvass_passo(playhead);
    if (!visivel && ler_quadro(playhead)) {
      visivel = 1;
      atraso = tempo;
    }
    if (!retrospectivo && ler_quadro(600.1)) {
      retrospectivo = 1;
      printf("cue consultada em 600.1s chegou apos %.3fs\n", tempo);
    }
    usleep(10000);
  }
  double pararInicio = now();
  parar();
  printf("retomada folga=%.1f visivel=%d atraso=%.3fs retrospectivo=%d parada=%.3fs\n",
         folga, visivel, atraso, retrospectivo, now() - pararInicio);
  pthread_mutex_lock(&trava);
  for (int indice = 0; indice < pedidos; ++indice)
    printf("range %d em %.3fs offset=%lld bytes=%lld\n", indice + 1,
           request_times[indice] - inicio, request_offsets[indice], request_lengths[indice]);
  pthread_mutex_unlock(&trava);
  // A legenda dura 400 ms a partir de 600,0 s; a retomada aos 1,3 s fecha a
  // janela real em 1,7 s. Visivel so conta se o quadro chegou dentro dela.
  return visivel && atraso < 1.7 ? 0 : 1;
}
