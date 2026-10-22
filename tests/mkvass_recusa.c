// #385: o no do CDN do TorBox RECUSOU toda conexao nossa (curl 7, "Connection
// refused") logo depois de o video abrir no .tpk. No registro, a pre-busca da
// legenda pediu o Range 0+65536 CINCO vezes, cada uma em DUAS conexoes (a
// segunda "de novo em conexao nova", rede.c), 13-15 s batendo num host que
// recusava — e a sonda do MKV pela rede ao mesmo tempo. Recusa de conexao e o
// CDN pedindo calma tanto quanto o curl 28 / 429 / 5xx do #308: a leitura
// LATERAL ao video (mkvass, sonda do MKV, capitulos) tem de parar ou pausar
// sem conexao nova, e voltar a usar o host so depois da pausa.
//
// O host falso (tests/mkvass_recusa.sh) e um redirecionador, como o
// StremThru do registro: cada GET do video volta 302 para uma porta que
// RECUSA conexao (socket preso sem listen). Cada tentativa nossa passa pelo
// redirecionador, entao a contagem dele e o numero de conexoes que a leitura
// lateral tentou. Depois, em modo "aceita", ele serve 206 — o host voltou.
//
//   tests/mkvass_recusa <base-url>
//
// Fases (todas com o video "aberto", mkvass_video_aberto(1)):
//   1. pre-busca contra o host que recusa: no maximo UMA tentativa (antes: 5
//      Ranges x 2 conexoes = 10);
//   2. dentro da pausa, a colheita de verdade (faixas.c) E a sonda do MKV
//      (fio proprio, como fioMkv do video_tpk.c) ao mesmo tempo: ZERO conexoes;
//   3. passada a pausa, com o host aceitando: o host e usado de novo.
#include "../src/mkvass.h"
#include "../src/rede.h"
#include "../src/mkv.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// O fio da sonda se declara LATERAL como o fioMkv do .tpk. Fraca: na arvore
// sem o #385 nao existe em rede.c e o teste ainda compila (e falha nas
// contagens, que e o que se quer provar).
__attribute__((weak)) void rede_lateral(int sim) { (void)sim; }

static int falhas;
static void ok(int cond, const char *o) {
  printf("  %-66s %s\n", o, cond ? "ok" : "FALHOU");
  if (!cond) falhas++;
}
static long agoraMs(void) {
  struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long)ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}
static void dormirMs(long ms) { usleep((useconds_t)ms * 1000); }

static char base[512];
static long controle(const char *cmd) {
  char url[600]; char *r; long v;
  snprintf(url, sizeof url, "%s/%s", base, cmd);
  r = rede_baixar(url, 5);
  v = r ? atol(r) : -1;
  free(r);
  return v;
}

static int escolher(const char *const *idiomas, int n) { (void)idiomas; return n > 0 ? 0 : -1; }

// Espera o fio do mkvass terminar ou parar num estado (no-go, ocioso, completo).
static void esperarQuieto(long prazoMs) {
  long t0 = agoraMs();
  while (agoraMs() - t0 < prazoMs) {
    int e = mkvass_estado();
    mkvass_passo(0.0);
    if (!mkvass_ocupado() && mkvass_prebusca_fase() != 1) return;
    if (e >= MKVASS_NOGO && !mkvass_ocupado()) return;
    dormirMs(20);
  }
}

typedef struct { const char *url; int n; } Sonda;
static void *fioSonda(void *arg) {
  Sonda *s = arg;
  MkvFaixa fx[MKV_MAX_FAIXAS];
  rede_lateral(1);
  s->n = mkv_faixas(s->url, fx, MKV_MAX_FAIXAS);
  return NULL;
}

int main(int argc, char **argv) {
  char url[700];
  long n1, n2, n3, t0;
  if (argc < 2) { fprintf(stderr, "uso: %s <base-url>\n", argv[0]); return 2; }
  snprintf(base, sizeof base, "%s", argv[1]);
  snprintf(url, sizeof url, "%s/stream/v.mkv", base);
  setvbuf(stdout, NULL, _IOLBF, 0);

  printf("fase 1: pre-busca com o video aberto, host recusa conexao\n");
  controle("modo/recusa");
  controle("zerar");
  t0 = agoraMs();
  ok(mkvass_prebuscar(url, escolher, 0.0) == 1, "pre-busca comecou");
  mkvass_video_aberto(1);
  esperarQuieto(60000);
  n1 = controle("contagem");
  printf("[teste] fase 1: %ld tentativa(s) de conexao ao host do video em %ld ms\n", n1, agoraMs() - t0);
  ok(n1 >= 1, "a pre-busca tentou o host (o redirecionador viu o pedido)");
  ok(n1 <= 1, "no maximo UMA tentativa: sem 5 Ranges nem segunda conexao");

  printf("fase 2: dentro da pausa, colheita (faixas.c) e sonda do MKV ao mesmo tempo\n");
  controle("zerar");
  t0 = agoraMs();
  {
    pthread_t t; Sonda s = { url, -1 };
    pthread_create(&t, NULL, fioSonda, &s);
    mkvass_iniciar_ordinal(url, 0);
    esperarQuieto(60000);
    pthread_join(t, NULL);
    n2 = controle("contagem");
    printf("[teste] fase 2: %ld tentativa(s) de conexao (colheita + sonda) em %ld ms, sonda=%d faixa(s), estado=%d\n",
           n2, agoraMs() - t0, s.n, mkvass_estado());
  }
  ok(n2 == 0, "dentro da pausa: nenhuma conexao nova ao host que recusou");

  printf("fase 3: passada a pausa, o host aceita de novo\n");
  dormirMs(1500);
  controle("modo/aceita");
  controle("zerar");
  mkvass_retomar();
  esperarQuieto(60000);
  n3 = controle("contagem");
  printf("[teste] fase 3: %ld pedido(s) ao host depois da pausa, estado=%d\n", n3, mkvass_estado());
  ok(n3 >= 1, "depois da pausa o host volta a ser usado");
  mkvass_parar();

  printf("contagens: fase1=%ld fase2=%ld fase3=%ld\n", n1, n2, n3);
  if (falhas) { printf("mkvass_recusa: %d falha(s)\n", falhas); return 1; }
  printf("mkvass_recusa: ok\n");
  return 0;
}
