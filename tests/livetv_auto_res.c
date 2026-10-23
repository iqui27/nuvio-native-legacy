#include "../src/proxyts.c"
#include <assert.h>

static int resolution;
int ajustes_livetv_resolucao(void) { return resolution; }

int main(void) {
  const char *master = "#EXTM3U\n"
    "#EXT-X-STREAM-INF:BANDWIDTH=3000000,RESOLUTION=1280x720\nhd.m3u8\n"
    "#EXT-X-STREAM-INF:BANDWIDTH=18000000,RESOLUTION=3840x2160\nuhd.m3u8\n"
    "#EXT-X-STREAM-INF:BANDWIDTH=7000000,RESOLUTION=1920x1080\nfhd.m3u8\n";
  const int options[] = {0, 2, 3, 0};
  const char *expected[] = {"uhd", "fhd", "hd", "uhd"};
  PxLista l;
  for (unsigned i = 0; i < sizeof options / sizeof options[0]; i++) {
    resolution = options[i];
    assert(pxLer(master, "http://fixture/master.m3u8", &l));
    char url[128]; snprintf(url, sizeof url, "http://fixture/%s.m3u8", expected[i]);
    if (strcmp(l.uri[0], url)) {
      fprintf(stderr, "FAIL option %d: expected %s, got %s\n", resolution, url, l.uri[0]);
      return 1;
    }
  }
  resolution = 0;
  assert(pxLer("#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=1\na.m3u8\n"
               "#EXT-X-STREAM-INF:BANDWIDTH=2\nb.m3u8\n", "http://fixture/m.m3u8", &l));
  assert(!strcmp(l.uri[0], "http://fixture/b.m3u8"));
  assert(pxLer("#EXTM3U\n#EXTINF:4,\na.ts\n", "http://fixture/m.m3u8", &l));
  assert(!l.mestre && !strcmp(l.uri[0], "http://fixture/a.ts"));
  puts("PASS: Automatic uses highest HLS variant; changes apply on next open");
  return 0;
}
