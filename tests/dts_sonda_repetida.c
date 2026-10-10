/* webOS 3 (65SJ800V / OLED55B7P, 2.0.2 e 2.0.3): o app caia ao abrir um video
 * na 2a/3a/4a sonda do adaptador DTS da sessao, nunca na 1a. Cada sonda fazia
 * dlopen + dlclose da libplayerAPIs; o dlclose descarregava junto as libs que
 * ja tinham fios rodando. Aqui: sondar varias vezes e seguir vivo. */
#define _DEFAULT_SOURCE
#include "dts/dts_pipeline.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
int main(int argc, char **argv) {
  if (argc < 2) return 2;
  setenv("NUVIO_DTS_ADAPTER_DIR", argv[1], 1);
  for (int i = 0; i < 4; i++) {
    if (!dts_pipeline_available(3)) { fprintf(stderr, "sonda %d recusada\n", i + 1); return 1; }
    usleep(50 * 1000);  /* o fio da lib falsa roda entre uma sonda e outra */
    dts_pipeline_available_esquecer();  /* sem a resposta guardada: carrega de novo */
  }
  usleep(200 * 1000);
  printf("OK: 4 sondas, processo vivo\n");
  return 0;
}
