// Regressoes do PR #489 no motor (regexjs.c). Roda em regexjs_v8.sh.
//  1. repeticao de grupo recursa em roda(): numa pilha de 64 KB tem de falhar
//     limpo (sem casar ou casando), nunca estourar a pilha.
//  2. `(?s)^.$` em "\n": o filtro de primeiro byte tem de aceitar \n com a flag s.
#include "regexjs.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

static void *pilha_pequena(void *r) {
  char p[501] = "^(?:ab)*", t[6001];
  int i;
  RegexJs *re;
  while (strlen(p) < 400) strcat(p, "(?:)");
  for (i = 0; i < 6000; i++) t[i] = i % 2 ? 'b' : 'a';
  t[6000] = 0;
  re = regexjs_compilar(p, 0, NULL, 0);
  assert(re);
  *(int *)r = regexjs_testar(re, t, strlen(t));
  regexjs_liberar(re);
  return NULL;
}

int main(void) {
  pthread_attr_t a;
  pthread_t th;
  int r = -1;
  RegexJs *re;
  pthread_attr_init(&a);
  pthread_attr_setstacksize(&a, 64 * 1024);
  assert(!pthread_create(&th, &a, pilha_pequena, &r));
  pthread_join(th, NULL);
  assert(r == 0 || r == 1);

  re = regexjs_compilar_web("(?s)^.$", NULL, 0);
  assert(re && regexjs_testar(re, "\n", 1) == 1);
  regexjs_liberar(re);
  re = regexjs_compilar_web("^.$", NULL, 0);
  assert(re && regexjs_testar(re, "\n", 1) == 0);
  regexjs_liberar(re);
  puts("regexjs_pr489: ok");
  return 0;
}
