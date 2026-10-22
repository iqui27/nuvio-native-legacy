// #92: o pipeline recebe a URL inteira; a sonda e o ASS precisam da mesma URL.
#include "../src/video.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// Dobles inertes: video.c os referencia (velocidade e legenda DTS), este teste so olha a URL.
#include "../src/cacheboost.h"
#include "../src/dts/dts_overlay.h"
int cacheboost_ganho_estado(void) { return 0; }
int dts_overlay_draw(DtsPlayback *p, double seconds, float x, float y,
                     float w, float h, int video_w, int video_h, float alpha) {
  (void)p; (void)seconds; (void)x; (void)y; (void)w; (void)h; (void)video_w; (void)video_h; (void)alpha;
  return 0;
}

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
