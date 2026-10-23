// Pilha de fibra alinhada em 16 (src/fio1.c, alvo VIDAA --um-fio).
//
// O DEFEITO QUE ISTO PEGA: a pilha C de cada fibra vinha de malloc(), que no
// Emscripten so garante 8 bytes. O compilador supoe o ponteiro de pilha
// alinhado em 16 e usa `(sp+16) | 8` no lugar de `(sp+16) + 8`: com a pilha
// 8 mod 16 o `| 8` nao soma nada. No __stdio_write da musl isso passa o
// iovec ERRADO (o vazio, iov_len 0) ao fd_write: volta 0 byte, sobra o mesmo
// resto, e o laco `for (;;)` gira para sempre sem ceder. No app a pagina
// inteira congelava ao gravar fileirasui-p1.txt (19767 B) numa fibra.
//
// Cada fibra confere o proprio ponteiro de pilha e faz um fwrite grande
// num FILE novo (buffer vazio, wpos == wbase — o caso que cai no laco).
// Com o defeito o processo nao termina: tests/fio1-pilha.sh poe prazo.
#include <emscripten.h>
#include <emscripten/stack.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fio1.h"

#define NFIBRAS 8
#define TAM 19767

static int falhas;
static char conteudo[TAM + 1];

static void *gravar(void *arg) {
  int i = (int)(intptr_t)arg;
  uintptr_t sp = emscripten_stack_get_current();
  char nome[64];
  FILE *f;
  size_t n;
  if (sp & 15) {
    printf("FAIL: fibra %d com pilha desalinhada (sp %% 16 = %u)\n", i, (unsigned)(sp & 15));
    falhas++;
  } else printf("PASS: fibra %d com pilha alinhada em 16\n", i);
  fflush(stdout);
  snprintf(nome, sizeof nome, "/tmp/fio1-pilha-%d.txt", i);
  f = fopen(nome, "w");
  if (!f) { printf("FAIL: fopen %s\n", nome); falhas++; return NULL; }
  n = fwrite(conteudo, 1, TAM, f);
  fclose(f);
  if (n != TAM) { printf("FAIL: fibra %d gravou %zu de %d\n", i, n, TAM); falhas++; }
  else printf("PASS: fibra %d gravou %d bytes\n", i, TAM);
  fflush(stdout);
  return NULL;
}

int main(void) {
  pthread_t t[NFIBRAS];
  void *lixo[NFIBRAS];
  int i;
  memset(conteudo, 'x', TAM);
  // Tamanhos variados antes de cada fibra: o endereco que o malloc devolve
  // muda de alinhamento entre 8 e 16, e o teste nao depende da sorte.
  for (i = 0; i < NFIBRAS; i++) {
    pthread_attr_t a;
    lixo[i] = malloc((size_t)(8 * i + 8));
    pthread_attr_init(&a);
    if (i & 1) pthread_attr_setstacksize(&a, 65536 + 8);
    pthread_create(&t[i], (i & 1) ? &a : NULL, gravar, (void *)(intptr_t)i);
    pthread_attr_destroy(&a);
  }
  for (i = 0; i < NFIBRAS; i++) pthread_join(t[i], NULL);
  for (i = 0; i < NFIBRAS; i++) free(lixo[i]);
  if (falhas) { printf("FAIL: %d verificacao(oes)\n", falhas); return 1; }
  printf("=== OK fio1_pilha\n");
  return 0;
}
