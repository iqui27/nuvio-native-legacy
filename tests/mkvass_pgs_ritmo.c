// Ritmo serial do leitor PGS (folga de video baixa). Regressao da revisao:
// se a folga cai no meio do Cluster ja iniciado, o proximo Range nao pode
// esperar o ritmo — a legenda vigente expiraria antes de aparecer. E depois
// de um seek o ritmo recomeca, sem divida de scanNext da posicao velha.
// A excecao de ritmo livre vale so para bytes do Cluster do playhead:
// "ahead" (playhead 600 s, Cluster de 32 KiB, proximo em 660 s) e "behind"
// (playhead 599 s, proximo em 600 s) exigem o primeiro Range do Cluster
// futuro com <= 64 KiB e so depois da espera de ritmo cheia — antes do
// Timestamp dele ser lido.
// Modos: "drop" (folga 30 -> 19.75 na primeira leitura do Cluster) e
// "seek" (600 -> 300 s com folga 3 constante). Transporte Range local.
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

static int baixar_folga;

char *rede_baixar_trecho_st(const char *url, int timeout, long long ini,
                           long long fim, long *tam, int *status, int *erro,
                           char *final, unsigned tamFinal) {
  pthread_mutex_lock(&trava);
  int primeira_varredura = pedidos == 2;
  pthread_mutex_unlock(&trava);
  char *corpo = transporte_fixture(url, timeout, ini, fim, tam, status, erro, final, tamFinal);
  if (baixar_folga && primeira_varredura) mkvass_folga(19.75);
  return corpo;
}

static int ler_quadro(double pos) {
  unsigned dono = imgsub_dono_atual();
  ImgsubQuadro *quadro = legenda_ligada_em(dono) ? imgsub_pegar(pos) : NULL;
  int visivel = quadro && quadro->n > 0;
  imgsub_soltar(quadro);
  return visivel;
}

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

int main(int argc, char **argv) {
  assert(argc == 4);
  arquivo = argv[1];
  double folga = atof(argv[2]);
  int buscar = !strcmp(argv[3], "seek");
  int atras = !strcmp(argv[3], "behind");
  int adiante = !strcmp(argv[3], "ahead") || atras;
  baixar_folga = !strcmp(argv[3], "drop");
  assert(buscar || baixar_folga || adiante);
  double pos = buscar ? 300.0 : atras ? 599.0 : 600.0;
  int visivel = 0, pedidos_pre_seek = 0;
  double atraso = -1, saltou = -1, range_pos_seek = -1;
  dados_iniciar(".");
  mkvass_aceitar_imagem(1);
  legenda_desligar();
  double inicio = now();
  pthread_mutex_lock(&trava);
  mkvass_iniciar(esperado, 6);
  mkvass_passo(600);
  mkvass_folga(folga);
  pthread_mutex_unlock(&trava);
  if (buscar) {
    for (;;) {
      pthread_mutex_lock(&trava);
      int leituras = pedidos;
      pthread_mutex_unlock(&trava);
      if (leituras >= 3 || now() - inicio > 1.0) break;
      usleep(1000);
    }
    saltou = now();
    pthread_mutex_lock(&trava);
    pedidos_pre_seek = pedidos;
    pthread_mutex_unlock(&trava);
    mkvass_passo(pos);
  }

  double relogio = buscar ? saltou : inicio;
  double tempo = 0;
  while (tempo < (adiante ? 7.5 : 6.5)) {
    tempo = now() - relogio;
    double playhead = pos + (adiante ? 0 : tempo);
    mkvass_passo(playhead);
    if (!visivel && ler_quadro(playhead)) {
      visivel = 1;
      atraso = tempo;
    }
    if (buscar && range_pos_seek < 0) {
      pthread_mutex_lock(&trava);
      if (pedidos > pedidos_pre_seek)
        range_pos_seek = request_times[pedidos - 1] - saltou;
      pthread_mutex_unlock(&trava);
    }
    usleep(10000);
  }
  double pararInicio = now();
  parar();
  printf("%s folga=%.1f visivel=%d atraso=%.3fs range_pos_seek=%.3fs parada=%.3fs\n",
         argv[3], folga, visivel, atraso, range_pos_seek, now() - pararInicio);
  pthread_mutex_lock(&trava);
  for (int indice = 0; indice < pedidos; ++indice)
    printf("range %d em %.3fs offset=%lld bytes=%lld\n", indice + 1,
           request_times[indice] - inicio, request_offsets[indice], request_lengths[indice]);
  pthread_mutex_unlock(&trava);
  if (adiante) {
    // Primeiro Range no Cluster futuro: <= 64 KiB e nao antes de ~5 s do
    // pedido que carregou a divida do ritmo (o primeiro da varredura).
    long long futuro = achar_cluster(arquivo, atras ? 600000 : 660000);
    assert(futuro >= 0);
    for (int i = 2; i < pedidos; ++i) {
      if (request_offsets[i] != futuro) continue;
      double gap = request_times[i] - request_times[2];
      printf("%s primeiro range futuro: offset=%lld bytes=%lld t=%.3fs gap=%.3fs\n",
             argv[3], futuro, request_lengths[i], request_times[i] - inicio, gap);
      return request_lengths[i] <= 65536 && gap >= 4.9 ? 0 : 1;
    }
    printf("%s nenhum range no Cluster futuro (offset %lld)\n", argv[3], futuro);
    return 1;
  }
  // A legenda dura 2 s (600-602 / 300-302): visivel so conta se chegou a
  // tempo de ser exibida; no seek o primeiro Range novo nao pode herdar a
  // espera do ritmo anterior.
  assert(visivel && atraso < 2.0);
  if (buscar) assert(range_pos_seek >= 0 && range_pos_seek < 1.5);
  return 0;
}
