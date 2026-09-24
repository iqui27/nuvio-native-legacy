// #92: o pipeline recebe a URL inteira; a sonda e o ASS precisam da mesma URL.
#include "../src/video.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

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
