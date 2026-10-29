#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include "../src/seekprobe.h"
// argv[1] = server honoring Range, argv[2] = server ignoring it (200 + whole file)
int main(int argc, char **argv) {
  int i, r = -1;
  assert(argc == 3);
  assert(seekprobe_sem_range(argv[1], "") == 0);
  assert(seekprobe_sem_range(argv[2], "") == 1);
  assert(seekprobe_sem_range("http://127.0.0.1:1/x.m3u8", "") == 0);   // HLS: never warn
  assert(seekprobe_resultado(argv[2]) == -1);
  seekprobe_iniciar(argv[2], "");
  for (i = 0; i < 100 && (r = seekprobe_resultado(argv[2])) < 0; i++) usleep(50000);
  assert(r == 1);
  puts("seekprobe ok");
  return 0;
}
