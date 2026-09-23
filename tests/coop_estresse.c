// Estresse das fibras do Tizen 4 no formato do app: o principal roda um laco
// de "quadros" que cede por EM_ASYNC_JS (como nv_ceder_quadro em main.c) e
// gira coop_rodar; as fibras gravam arquivo com stdio, travam mutex e dormem.
// Reproduz o travamento visto no Chrome: fwrite preso em writev de 0 bytes.
#include <emscripten.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

EM_ASYNC_JS(void, quadro, (), {
  await new Promise(function (r) { setTimeout(r, 4); });
});

static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static int gravacoes, falhas;
static char grande[20000];

static void *fio(void *a) {
  int k = (int)(intptr_t)a, i;
  char nome[64];
  for (i = 0; i < 40; i++) {
    FILE *f;
    snprintf(nome, sizeof nome, "/tmp/f%d.txt", k);
    pthread_mutex_lock(&m);
    f = fopen(nome, "w");
    if (!f || fwrite(grande, 1, sizeof grande - 1 - k, f) != sizeof grande - 1 - k) falhas++;
    if (f) fclose(f);
    gravacoes++;
    pthread_mutex_unlock(&m);
    usleep(1000 * (1 + (k + i) % 5));
  }
  return NULL;
}

int main(void) {
  pthread_t t[12];
  int i, q;
  memset(grande, 'x', sizeof grande);
  for (i = 0; i < 12; i++) pthread_create(&t[i], NULL, fio, (void *)(intptr_t)i);
  for (q = 0; q < 3000 && gravacoes < 12 * 40; q++) {
    coop_rodar(8);
    quadro();
  }
  printf("estresse: quadros=%d gravacoes=%d falhas=%d\n", q, gravacoes, falhas);
  printf(gravacoes == 480 && !falhas ? "estresse: tudo certo\n" : "estresse: FALHOU\n");
  fflush(stdout);
  emscripten_force_exit(0);
  return 0;
}
