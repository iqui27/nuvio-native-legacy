// Confere fonteregra_regex_testar contra respostas do Nuvio oficial (geradas por
// tests/fonteregra_oficial.js). Cada linha: esperado<TAB>padrao<TAB>texto (\n no
// texto = quebra de linha). Sai com 1 se alguma divergir.
//
//   bash tests/fonteregra_oficial.sh
#include "fonteregra.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
  FILE *f = argc > 1 ? fopen(argv[1], "r") : NULL;
  char linha[4096];
  int n = 0, ruins = 0;
  if (!f) { fprintf(stderr, "uso: %s casos.tsv\n", argv[0]); return 2; }
  while (fgets(linha, sizeof linha, f)) {
    char *p, *t, *w, *r;
    int esperado, deu;
    linha[strcspn(linha, "\r\n")] = 0;
    if (!linha[0]) continue;
    p = strchr(linha, '\t');
    if (!p) continue;
    *p++ = 0;
    t = strchr(p, '\t');
    if (!t) continue;
    *t++ = 0;
    for (r = w = t; *r; r++) {                     // \n -> quebra de linha
      if (r[0] == '\\' && r[1] == 'n') { *w++ = '\n'; r++; }
      else *w++ = *r;
    }
    *w = 0;
    esperado = atoi(linha);
    deu = fonteregra_regex_testar(p, t);
    n++;
    if (deu != esperado) {
      ruins++;
      printf("DIVERGE (oficial=%d, aqui=%d)\n  padrao: %s\n  texto : %s\n", esperado, deu, p, t);
    }
  }
  fclose(f);
  printf("fonteregra_oficial: %d casos, %d divergencias\n", n, ruins);
  return ruins ? 1 : 0;
}
