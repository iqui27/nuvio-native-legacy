#include <assert.h>
#include <stdio.h>
#include "../src/video_dvretry.h"
int main(void) {
  NvDvSonda s = {0, 0};
  int n = 0;
  assert(nv_dvsonda_falhou(&s, &n) == 3000 && n == 1);
  assert(nv_dvsonda_falhou(&s, &n) == 8000 && n == 2);
  assert(nv_dvsonda_falhou(&s, &n) == 20000 && n == 3);
  assert(!s.desistiu);
  assert(nv_dvsonda_falhou(&s, &n) == 0 && s.desistiu && n == 3);   // esgotou
  nv_dvsonda_zerar(&s);
  assert(!s.desistiu && s.falhas == 0 && nv_dvsonda_falhou(&s, NULL) == 3000);
  assert(nv_dvsonda_pronta(1, 0) && !nv_dvsonda_pronta(1, 1) && !nv_dvsonda_pronta(0, 0));
  assert(nv_url_e_mp4("https://imdb-video.media-imdb.com/vi1/x.mp4?Expires=1&Sig=a") &&
         nv_url_e_mp4("http://h/a.MP4") && !nv_url_e_mp4("http://h/a.mkv") &&
         !nv_url_e_mp4("http://h/a.mkv?f=x.mp4") && !nv_url_e_mp4("mp4") && !nv_url_e_mp4(NULL));
  puts("dvretry ok");
  return 0;
}
