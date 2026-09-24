// Ver tests/rede_parada.sh.
#include "rede.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int porta;
static unsigned long agoraMs(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (unsigned long)ts.tv_sec * 1000UL + (unsigned long)ts.tv_nsec / 1000000UL;
}
static char *pedir(const char *caminho, int segundos, long *n, unsigned long *ms) {
  char u[160];
  unsigned long t0 = agoraMs();
  char *r;
  snprintf(u, sizeof u, "http://127.0.0.1:%d%s", porta, caminho);
  r = rede_baixar_bin(u, segundos, n);
  *ms = agoraMs() - t0;
  return r;
}
static int conexoes(void) {
  long n = 0;
  unsigned long ms;
  char *r = pedir("/conexoes", 5, &n, &ms);
  int v = r ? atoi(r) : -1;
  free(r);
  return v;
}
static int falhas;
static void conferir(int ok, const char *o_que, unsigned long ms) {
  printf("%s  %s (%lu ms)\n", ok ? "ok  " : "FALHOU", o_que, ms);
  if (!ok) falhas++;
}

int main(int argc, char **argv) {
  long n = 0;
  unsigned long ms;
  char *r;
  int i, antes, depois;
  if (argc < 2) return 2;
  porta = atoi(argv[1]);

  // D primeiro: rajada no mesmo fio abre UMA conexao (o /conexoes tambem vai
  // por ela). Sem isto a correcao podia "resolver" desligando o reuso.
  antes = conexoes();
  for (i = 0; i < 5; i++) {
    char c[40];
    snprintf(c, sizeof c, "/img/%d.jpg", i);
    r = pedir(c, 5, &n, &ms);
    if (!r || n != 65536) { conferir(0, "rajada: pedido falhou", ms); return 1; }
    free(r);
  }
  depois = conexoes();
  conferir(depois == antes, "D. rajada de 5 pedidos na mesma conexao", 0);

  // A. Conexao reusada abandonada em silencio depois de ociosa. O servidor
  // responde este pedido e congela a conexao; passado o limite de ociosidade
  // (1 s no teste), o proximo pedido tem de ir por conexao NOVA. Antes da
  // correcao ele ia pela congelada e esperava o prazo inteiro (curl 28).
  r = pedir("/img/a.jpg?congelar=1", 5, &n, &ms);
  free(r);
  usleep(1500 * 1000);
  r = pedir("/img/a2.jpg", 4, &n, &ms);
  conferir(r && n == 65536 && ms < 2000,
           "A. reusada e abandonada apos ociosa: conexao nova, sem esperar o prazo", ms);
  free(r);

  // B. Corpo parado no meio com a conexao aberta. Prazo de 8 s: o vigia de
  // "sem progresso" corta em ~2,7 s e a segunda tentativa, em conexao nova,
  // recebe o corpo inteiro. Antes: 8 s e NULL.
  r = pedir("/corte/b.jpg", 8, &n, &ms);
  conferir(r && n == 65536 && ms < 5000,
           "B. corpo parado no meio: corta cedo e repete em conexao nova", ms);
  free(r);

  // C. Servidor fecha a ociosa COM FIN antes do limite: a libcurl percebe e
  // abre outra sozinha, sem esperar nada.
  r = pedir("/img/c.jpg?fechar=1", 5, &n, &ms);
  free(r);
  usleep(500 * 1000);
  r = pedir("/img/c2.jpg", 4, &n, &ms);
  conferir(r && n == 65536 && ms < 1000, "C. ociosa fechada com FIN: conexao nova na hora", ms);
  free(r);

  // Corte pelo teto (curl 23 de proposito) nao e repetido: o que veio basta.
  rede_teto = 1000;
  r = pedir("/img/teto.jpg", 5, &n, &ms);
  rede_teto = 0;
  conferir(r && n == 1000, "teto de bytes: corte de proposito sem segunda tentativa", ms);
  free(r);

  if (falhas) { printf("rede_parada: %d falha(s)\n", falhas); return 1; }
  printf("rede_parada: tudo ok\n");
  return 0;
}
