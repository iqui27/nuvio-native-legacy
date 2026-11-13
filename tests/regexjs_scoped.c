#include "regexjs.h"
#include <stdio.h>
#include <string.h>

static int check(const char *pattern, const char *text, int expected) {
  char error[128] = {0};
  RegexJs *re = regexjs_compilar(pattern, 0, error, sizeof error);
  if (!re) { fprintf(stderr, "compile failed: %s: %s\n", pattern, error); return 1; }
  int result = regexjs_testar(re, text, strlen(text));
  regexjs_liberar(re);
  if (result != expected) { fprintf(stderr, "wrong match: %s / %s\n", pattern, text); return 1; }
  return 0;
}
int main(void) {
  return check("^(?i:abc)D$", "ABCD", 1) |
         check("^(?i:abc)D$", "ABCd", 0) |
         check("^(?i:a(?-i:b)c)$", "AbC", 1) |
         check("^(?i:a(?-i:b)c)$", "ABC", 0) |
         check("^(?s:.)$", "\n", 1);
}
