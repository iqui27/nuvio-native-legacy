// Exercise the actual webOS reply decoder and Tizen-host request publication.
#include "painel.h"
#include <assert.h>
#ifdef NV_WEBOS
static const char *payload(void *message) { return message; }
#endif
int main(void) {
#ifdef NV_WEBOS
  painelPayload = payload;
  const char *replies[] = {
    "{\"panelResolution\":\"UHD\"}", "{\"panelResolution\":\"3840x2160\"}",
    "{\"UHD\":true}", "{\"UHD\":\"true\"}", "{\"UHD\":\"1\"}",
    "{\"panelResolution\":\"FHD\"}", "{\"panelResolution\":\"HD\"}",
    "{\"returnValue\":false}", "{\"panelResolution\":\"unknown\"}",
    "{\"UHD\":false}", "{\"panelResolution\":\"8K\"}"
  };
  const int widths[] = {3840,3840,3840,3840,3840,1920,1280,0,0,0,7680};
  const int heights[] = {2160,2160,2160,2160,2160,1080,720,0,0,0,4320};
  for (unsigned i = 0; i < sizeof replies / sizeof *replies; i++) {
    int size[3] = {0};
    painelResposta(NULL, (void *)replies[i], size);
    assert(size[0] == widths[i] && size[1] == heights[i] && size[2]);
  }
  puts("PASS webOS panelResolution/UHD replies and unknown/denied fallback");
#endif
  return 0;
}
