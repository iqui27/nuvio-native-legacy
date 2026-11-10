// O codigo do registro tem de ser o MESMO que o servidor calcula
// (servidor/recomendacoes/src/codigo.js): os pares abaixo sairam de
// `node codigo-registro.mjs --id <id>`.
#include "../src/regcodigo.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  char c[8];
  assert(regcodigo_de_id("1", c) && !strcmp(c, "105ZR4"));
  assert(regcodigo_de_id("9866", c) && !strcmp(c, "W6N859"));
  assert(regcodigo_de_id("123456", c) && !strcmp(c, "E79387"));
  assert(!regcodigo_de_id("0", c) && !c[0]);
  assert(!regcodigo_de_id("null", c) && !c[0]);
  assert(!regcodigo_de_id("", c));
  assert(!regcodigo_de_id("1073741824", c));
  puts("regcodigo: ok");
  return 0;
}
