#include <assert.h>
#include <stdio.h>
#include "../src/video_seekretry.h"
int main(void) {
  NvSeekRetry s = {0, 0};
  assert(nv_seek_e_recusa(700, 0) && nv_seek_e_recusa(-1, 1) && !nv_seek_e_recusa(100, 0));
  assert(nv_seek_recusado(&s) == 500 && nv_seek_recusado(&s) == 1500 && nv_seek_recusado(&s) == 3000);
  assert(!s.desistiu);
  assert(nv_seek_recusado(&s) == 0 && s.desistiu);   // desiste, nunca falha de fonte
  nv_seek_zerar(&s);
  assert(!s.desistiu && nv_seek_recusado(&s) == 500);
  nv_seek_ok(&s); assert(s.falhas == 0);
  puts("seekretry ok");
  return 0;
}
