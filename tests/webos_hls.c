#include "dts/dts_hls_tpk.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  char url[128];
  if (argc != 3 || !dts_hls_webos_open(argv[1], NULL, 0, atoi(argv[2]), 0, 0, url, sizeof url))
    return 1;
  printf("playlist %s\n", url);
  fflush(stdout);
  getchar();
  dts_tpk_fechar();
  return 0;
}
