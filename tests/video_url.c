// #92: o pipeline recebe a URL inteira; a sonda e o ASS precisam da mesma URL.
// video.c inteiro nao linka mais no teste (audio externo/DTS puxam dezenas de
// modulos); video_url.inc extrai as cotas de simulacao — urlAtual,
// video_url_atual e o video_tocar coto — do src/video.c de verdade.
#ifndef NV_LINUX_DESKTOP
#define NV_LINUX_DESKTOP   // VideoSimulacao existe so na cota desktop do video.h
#endif
#include "../src/mediaurl.h"
#include "../src/video.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "video_url.inc"

int main(void) {
  char url[4096];
  const size_t tamanhos[] = { 900, 1200, 2000, sizeof url - 1 };
  for (unsigned i = 0; i < sizeof tamanhos / sizeof *tamanhos; i++) {
    memset(url, 'a', tamanhos[i]);
    memcpy(url, "https://example.test/", 21);
    memcpy(url + tamanhos[i] - 4, ".mkv", 4);
    url[tamanhos[i]] = 0;
    video_tocar(url);
    assert(strcmp(video_url_atual(), url) == 0);
  }
  puts("video_url: ok");
  return 0;
}
