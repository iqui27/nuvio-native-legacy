// Confere regexjs (modo "m") e fonteregra_regex_testar (modo "f") contra as
// respostas do V8 geradas por tests/regexjs_v8.js, e mede o tempo de cada caso.
// Sai com 1 se algum divergir ou se algum caso passar do teto de tempo (o teto e
// folgado para a maquina de teste; na TV a conta e de 5 a 10 vezes isso).
//
//   bash tests/regexjs_v8.sh
#include "fonteregra.h"
#include "regexjs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TETO_MS 25.0

static double agora(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

int main(int argc, char **argv) {
  FILE *f = argc > 1 ? fopen(argv[1], "r") : NULL;
  static char linha[65536];
  int n[2] = { 0, 0 }, ruins = 0, lentos = 0;
  double pior[2] = { 0, 0 };
  if (!f) { fprintf(stderr, "uso: %s casos.tsv\n", argv[0]); return 2; }
  while (fgets(linha, sizeof linha, f)) {
    char *e, *p, *t, *w, *r;
    int esperado, deu, modo;
    double t0, ms;
    linha[strcspn(linha, "\r\n")] = 0;
    if (!linha[0] || !(e = strchr(linha, '\t'))) continue;
    *e++ = 0;
    if (!(p = strchr(e, '\t'))) continue;
    *p++ = 0;
    if (!(t = strchr(p, '\t'))) continue;
    *t++ = 0;
    for (r = w = t; *r; r++) {
      if (r[0] == '\\' && r[1] == 'n') { *w++ = '\n'; r++; }
      else *w++ = *r;
    }
    *w = 0;
    modo = linha[0] == 'f';
    esperado = atoi(e);
    t0 = agora();
    if (modo) deu = fonteregra_regex_testar(p, t);
    else {
      RegexJs *re = regexjs_compilar(p, REGEXJS_I, NULL, 0);
      deu = re ? regexjs_testar(re, t, strlen(t)) : -1;
      regexjs_liberar(re);
    }
    ms = (agora() - t0) * 1000.0;
    n[modo]++;
    if (ms > pior[modo]) pior[modo] = ms;
    if (deu != esperado && ruins++ < 20)
      printf("DIVERGE (%s: v8=%d, aqui=%d)\n  padrao: %.160s\n  texto : %.160s\n", modo ? "oficial" : "motor",
             esperado, deu, p, t);
    if (ms > TETO_MS && lentos++ < 20) printf("LENTO %.1f ms\n  padrao: %.160s\n", ms, p);
  }
  fclose(f);
  printf("regexjs_v8: motor %d casos (pior %.2f ms), regex longas %d casos (pior %.2f ms), %d divergencias, %d lentos\n",
         n[0], pior[0], n[1], pior[1], ruins, lentos);
  return ruins || lentos ? 1 : 0;
}
