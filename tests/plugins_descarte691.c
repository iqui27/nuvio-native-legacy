// #691: estado do job de rede antes de soltar (sem use-after-free).
#include "rede.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

const char *rede_url_publica(const char *url, char *dst, unsigned n) {
  if (!dst || !n) return url;
  snprintf(dst, n, "%s", url ? url : "");
  return dst;
}
int rede_pino_valido(const char *pino) { (void)pino; return 1; }

int main(void) {
  RedeGrupo *g = rede_grupo_criar();
  RedeJob *j = rede_job_criar(g);
  RedeErro e;
  assert(j && g);
  e = rede_job_estado(j);
  rede_job_soltar(j);
  assert(e == REDE_OK);
  rede_grupo_soltar(g);
  puts("plugins_descarte691 ok");
  return 0;
}
