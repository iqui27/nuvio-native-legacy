#include "streams.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
  Stream *streams = NULL;
  FonteFiltroConfig cfg = {0};
  assert(argc == 2);
  if (!strcmp(argv[1], "accents")) {
    assert(stream_extrair("{\"streams\":[{\"url\":\"https://example.invalid/a\",\"audioTracks\":[{\"language\":\"Português\"}]},{\"url\":\"https://example.invalid/b\",\"audioTracks\":[{\"language\":\"Inglês\"}]}]}", "fixture", &streams) == 2);
    for (int i = 0; i < 2; i++) {
      cfg.excluir[FONTEFILTRO_LINGUAS] = fontefiltro_bit(FONTEFILTRO_LINGUAS, i ? 0 : 6);
      assert(!fontefiltro_permite(&cfg, &streams[i].filtro));
      cfg.excluir[FONTEFILTRO_LINGUAS] = fontefiltro_desconhecido(FONTEFILTRO_LINGUAS);
      assert(fontefiltro_permite(&cfg, &streams[i].filtro));
    }
  } else {
    assert(!strcmp(argv[1], "mixed"));
    assert(stream_extrair("{\"streams\":[{\"url\":\"https://example.invalid/a\",\"languages\":[\"English\",\"Russian\"]},{\"url\":\"https://example.invalid/b\",\"audioTracks\":[{\"language\":\"English\"},{\"language\":\"Russian\"}]}]}", "fixture", &streams) == 2);
    for (int i = 0; i < 2; i++) {
      cfg.excluir[FONTEFILTRO_LINGUAS] = fontefiltro_bit(FONTEFILTRO_LINGUAS, 0);
      assert(fontefiltro_permite(&cfg, &streams[i].filtro));
      cfg.excluir[FONTEFILTRO_LINGUAS] |= fontefiltro_desconhecido(FONTEFILTRO_LINGUAS);
      assert(!fontefiltro_permite(&cfg, &streams[i].filtro));
    }
  }
  free(streams);
  return 0;
}
