// regexjs com as flags s e m (e i, sempre) contra o V8: tests/regexjs_flags.js.
//
//   bash tests/regexjs_flags.sh
#include "regexjs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
  FILE *f = argc > 1 ? fopen(argv[1], "r") : NULL;
  static char linha[4096];
  int total = 0, ruins = 0;
  if (!f) { fprintf(stderr, "uso: %s casos.tsv\n", argv[0]); return 2; }
  while (fgets(linha, sizeof linha, f)) {
    char *fl, *p, *t, *w, *r;
    int esperado, deu, flags = REGEXJS_I;
    RegexJs *re;
    linha[strcspn(linha, "\r\n")] = 0;
    if (!(fl = strchr(linha, '\t'))) continue;
    *fl++ = 0;
    if (!(p = strchr(fl, '\t'))) continue;
    *p++ = 0;
    if (!(t = strchr(p, '\t'))) continue;
    *t++ = 0;
    for (r = w = t; *r; r++) {
      if (r[0] == '\\' && r[1] == 'n') { *w++ = '\n'; r++; }
      else if (r[0] == '\\' && r[1] == 'r') { *w++ = '\r'; r++; }
      else *w++ = *r;
    }
    *w = 0;
    if (strchr(fl, 's')) flags |= REGEXJS_S;
    if (strchr(fl, 'm')) flags |= REGEXJS_M;
    esperado = atoi(linha);
    re = regexjs_compilar(p, flags, NULL, 0);
    deu = re ? regexjs_testar(re, t, strlen(t)) : -1;
    regexjs_liberar(re);
    total++;
    if (deu != esperado && ruins++ < 20) {
      char vis[64]; size_t i, k = 0;
      for (i = 0; t[i] && k + 3 < sizeof vis; i++) {
        if (t[i] == '\n') { vis[k++] = '\\'; vis[k++] = 'n'; }
        else if (t[i] == '\r') { vis[k++] = '\\'; vis[k++] = 'r'; }
        else vis[k++] = t[i];
      }
      vis[k] = 0;
      printf("DIVERGE (v8=%d, aqui=%d) flags=%s padrao=%s texto=\"%s\"\n", esperado, deu, fl, p, vis);
    }
  }
  fclose(f);
  printf("regexjs_flags: %d casos, %d divergencias\n", total, ruins);
  return ruins || total < 500 ? 1 : 0;
}
