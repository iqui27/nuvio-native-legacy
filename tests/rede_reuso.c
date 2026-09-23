// Ver tests/rede_reuso.sh.
#include "rede.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int porta;
static int conexoes(void) {
  char u[80];
  char *r;
  int n;
  snprintf(u, sizeof u, "http://127.0.0.1:%d/conexoes", porta);
  r = rede_baixar(u, 5);
  n = r ? atoi(r) : -1;
  free(r);
  return n;
}
static void *outroFio(void *arg) {
  char u[80];
  int i;
  for (i = 0; i < 3; i++) {
    char *r;
    snprintf(u, sizeof u, "http://127.0.0.1:%d/f2/%d", porta, i);
    r = rede_baixar(u, 5);
    if (!r) *(int *)arg = 1;
    free(r);
  }
  return NULL;
}
int main(int argc, char **argv) {
  char u[80];
  int i, antes, depois, erro = 0;
  pthread_t f;
  if (argc < 2) return 2;
  porta = atoi(argv[1]);
  antes = conexoes();
  for (i = 0; i < 6; i++) {
    char *r;
    snprintf(u, sizeof u, "http://127.0.0.1:%d/img/%d.jpg", porta, i);
    r = rede_baixar(u, 5);
    if (!r || strlen(r) != 2048) { printf("FALHOU: pedido %d\n", i); return 1; }
    free(r);
  }
  depois = conexoes();
  if (antes != 1 || depois != 1) {
    printf("FALHOU: sete pedidos no mesmo fio abriram %d conexao(oes) (esperado 1)\n", depois);
    return 1;
  }
  printf("ok  sete pedidos no mesmo fio, uma conexao\n");
  pthread_create(&f, NULL, outroFio, &erro);
  pthread_join(f, NULL);
  depois = conexoes();
  if (erro || depois != 2) {
    printf("FALHOU: o segundo fio devia abrir a sua conexao (total %d, esperado 2)\n", depois);
    return 1;
  }
  printf("ok  outro fio, outra conexao (handle nao e compartilhado)\n");
  printf("rede_reuso: tudo ok\n");
  return 0;
}
