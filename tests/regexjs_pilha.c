// regexjs numa thread de 64 KB de pilha: repeticao de grupo sobre texto longo
// (tests/regexjs_pilha.js, resposta do V8). O motor antigo recursava uma vez por
// volta do laco e derrubava o processo; o retrocesso agora usa uma pilha na heap.
//
//   bash tests/regexjs_pilha.sh
#include "regexjs.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PILHA_FIO (64 * 1024)
static char *linha;
static FILE *arq;
static int total, ruins;

static void *roda(void *a) {
  (void)a;
  while (fgets(linha, 1 << 20, arq)) {
    char *e, *p, *t, *w, *r;
    int esperado, deu;
    RegexJs *re;
    linha[strcspn(linha, "\r\n")] = 0;
    if (!(p = strchr(linha, '\t'))) continue;
    *p++ = 0;
    if (!(t = strchr(p, '\t'))) continue;
    *t++ = 0;
    for (r = w = t; *r; r++) {
      if (r[0] == '\\' && r[1] == 'n') { *w++ = '\n'; r++; }
      else *w++ = *r;
    }
    *w = 0;
    e = linha;
    esperado = atoi(e);
    re = regexjs_compilar_web(p, NULL, 0);
    deu = re ? regexjs_testar(re, t, strlen(t)) : -1;
    regexjs_liberar(re);
    total++;
    if (deu != esperado && ruins++ < 20)
      printf("DIVERGE (v8=%d, aqui=%d)\n  padrao: %.120s\n  texto : %.60s... (%zu bytes)\n", esperado, deu, p, t, strlen(t));
  }
  return NULL;
}

int main(int argc, char **argv) {
  pthread_attr_t at;
  pthread_t th;
  if (argc < 2 || !(arq = fopen(argv[1], "r"))) { fprintf(stderr, "uso: %s casos.tsv\n", argv[0]); return 2; }
  linha = malloc(1 << 20);
  pthread_attr_init(&at);
  pthread_attr_setstacksize(&at, PILHA_FIO);
  if (pthread_create(&th, &at, roda, NULL)) { fprintf(stderr, "sem thread\n"); return 2; }
  pthread_join(th, NULL);
  fclose(arq);
  printf("regexjs_pilha: %d casos numa pilha de %d KB, %d divergencias\n", total, PILHA_FIO / 1024, ruins);
  return ruins || total < 20 ? 1 : 0;
}
