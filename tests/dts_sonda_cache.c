/* A sonda do adaptador DTS guarda a resposta pelo processo: no webOS 3 o
 * dts-starfish-webos4.so nao carrega (GLIBCXX_3.4.21) e cada video repetia o
 * dlopen que falha, mais o do webos3 e o da libplayerAPIs.
 *   sim pasta   5 sondas com adaptador: todas dizem sim
 *   nao pasta   5 sondas sem adaptador: todas dizem nao
 * Quantas vezes carregou de verdade quem conta e o .sh, pelas linhas [dts]. */
#define _DEFAULT_SOURCE
#include "dts/dts_pipeline.h"
#include "webosver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
  int quer;
  if (argc < 4) return 2;
  nv_webos_testar(argv[3], "/nonexistent/starfish-release");  /* o parser de verdade */
  quer = !strcmp(argv[1], "sim");
  setenv("NUVIO_DTS_ADAPTER_DIR", argv[2], 1);
  for (int i = 0; i < 5; i++)
    if (!!dts_pipeline_available(0) != quer) { fprintf(stderr, "sonda %d: resposta errada\n", i + 1); return 1; }
  return 0;
}
