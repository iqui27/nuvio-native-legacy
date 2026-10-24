#define main order_main
#include "fonte_ordem_addon.c"
#undef main

int main(void) {
  static Stream sources[2];
  for (int i = 0; i < 2; i++) {
    strcpy(sources[i].provedor, "AIOStreams");
    strcpy(sources[i].rotulo, "1080p");
    strcpy(sources[i].url, "https://video.test/same.mp4");
    snprintf(sources[i].addonChave, sizeof sources[i].addonChave, "instance%d", i);
    sources[i].addonOrdem = i;
  }
  assert(!mesmaFonte(&sources[0], &sources[1]));
  stream_definir_lista(sources, 2);
  filtro = 0; atualizarProvedores();
  assert(nProvedores == 3);
  filtro = 1;
  assert(passaFiltro(0) && !passaFiltro(1));
  assert(!irma(0, 1));
  puts("AIOStreams instances: independent tabs, focus and retry siblings PASS");
  return 0;
}
