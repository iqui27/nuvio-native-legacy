// dotAll first-byte filter: "." under (?s) must match \n/\r. The search
// filter used to drop those bytes even with REGEXJS_S, so a pattern whose
// first consuming atom is "." silently missed sources the official accepts.
//
//   bash tests/regexjs_dotall.sh
#include "regexjs.h"
#include <stdio.h>
#include <string.h>

static int casa(const char *padrao, const char *texto) {
  RegexJs *re = regexjs_compilar_web(padrao, NULL, 0);
  int deu = re ? regexjs_testar(re, texto, strlen(texto)) : -1;
  regexjs_liberar(re);
  return deu;
}

int main(void) {
  int ruins = 0;
  struct { const char *p, *t; int e; } c[] = {
    { "(?s)^.$", "\n", 1 },
    { "(?s).a", "\nab", 1 },
    { "(?s).{2}a", "\n\na", 1 },
    { "^.$", "\n", 0 },
    { "(?s)^.$", "a", 1 },
  };
  size_t i;
  for (i = 0; i < sizeof c / sizeof *c; i++) {
    int deu = casa(c[i].p, c[i].t);
    if (deu != c[i].e) {
      printf("DIVERGE %s on %s: want %d got %d\n", c[i].p,
             c[i].t[0] == '\n' ? "\\n..." : c[i].t, c[i].e, deu);
      ruins++;
    }
  }
  printf("regexjs_dotall: %d divergencias\n", ruins);
  return ruins ? 1 : 0;
}
