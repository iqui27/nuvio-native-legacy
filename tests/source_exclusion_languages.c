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
  } else if (!strcmp(argv[1], "subtitles")) {
    // A bare "Subtitles:" hides only language values on the next lines.
    static const struct { const char *linha; int cat, opcao; } tec[] = {
      {"1080p", FONTEFILTRO_RESOLUCAO, 2}, {"HDR", FONTEFILTRO_VISUAL, 2},
      {"10bit", FONTEFILTRO_VISUAL, 8}, {"DTS", FONTEFILTRO_AUDIO, 7},
      {"5.1", FONTEFILTRO_CANAIS, 2}, {"HEVC", FONTEFILTRO_ENCODE, 1},
    };
    for (unsigned i = 0; i < sizeof tec / sizeof *tec; i++) {
      char json[256];
      snprintf(json, sizeof json, "{\"streams\":[{\"url\":\"https://example.invalid/a\",\"description\":\"Addon\\nTitle\\nSubtitles:\\n%s\"}]}", tec[i].linha);
      assert(stream_extrair(json, "fixture", &streams) == 1);
      uint64_t b = fontefiltro_bit(tec[i].cat, tec[i].opcao);
      assert(streams[0].filtro.valores[tec[i].cat] & b);
      memset(&cfg, 0, sizeof cfg);
      cfg.excluir[tec[i].cat] = b;
      assert(!fontefiltro_permite(&cfg, &streams[0].filtro));
      free(streams); streams = NULL;
    }
    // Same-line subtitle sections keep technical tags, never languages.
    static const struct { const char *descricao; int es, en, cat, opcao; } ling[] = {
      {"Addon\\nTitle\\nSubtitles:\\n1080p\xF0\x9F\x87\xAA\xF0\x9F\x87\xB8", 0, 0, FONTEFILTRO_RESOLUCAO, 2},
      {"Addon\\nTitle\\nSubtitles:\\n1080p\\n\xF0\x9F\x87\xAA\xF0\x9F\x87\xB8", 0, 0, FONTEFILTRO_RESOLUCAO, 2},
      {"Audio: Spanish | Subs: English", 1, 0, -1, 0},
      {"Subs: Spanish", 0, 0, -1, 0},
      {"Addon\\nTitle\\nMulti Subs 1080p", 0, 0, FONTEFILTRO_RESOLUCAO, 2},
      {"Subs and 1080p info", 0, 0, FONTEFILTRO_RESOLUCAO, 2},
      {"Subtitles: English \xE2\x80\xA2 1080p", 0, 0, FONTEFILTRO_RESOLUCAO, 2},
      {"Subtitles: Spanish \xE2\x80\xA2 DTS-ES 5.1", 0, 0, FONTEFILTRO_AUDIO, 6},
      {"Subs: English https://example.invalid/2160p", 0, 0, FONTEFILTRO_RESOLUCAO, -1},
    };
    for (unsigned i = 0; i < sizeof ling / sizeof *ling; i++) {
      char json[256];
      snprintf(json, sizeof json, "{\"streams\":[{\"url\":\"https://example.invalid/a\",\"description\":\"%s\"}]}", ling[i].descricao);
      assert(stream_extrair(json, "fixture", &streams) == 1);
      uint64_t v = streams[0].filtro.valores[FONTEFILTRO_LINGUAS];
      assert(!(v & fontefiltro_bit(FONTEFILTRO_LINGUAS, 3)) == !ling[i].es);
      assert(!(v & fontefiltro_bit(FONTEFILTRO_LINGUAS, 0)) == !ling[i].en);
      assert(!(v & fontefiltro_bit(FONTEFILTRO_LINGUAS, 13)));
      if (ling[i].cat >= 0) {
        uint64_t t = streams[0].filtro.valores[ling[i].cat];
        assert(ling[i].opcao < 0 ? !t : (t & fontefiltro_bit(ling[i].cat, ling[i].opcao)) != 0);
      }
      free(streams); streams = NULL;
    }
    // Declared audio languages still win over subtitle text.
    assert(stream_extrair("{\"streams\":[{\"url\":\"https://example.invalid/a\",\"languages\":[\"Spanish\"],\"description\":\"Multi Subs English 1080p\"}]}", "fixture", &streams) == 1);
    assert(streams[0].filtro.valores[FONTEFILTRO_LINGUAS] == fontefiltro_bit(FONTEFILTRO_LINGUAS, 3));
    assert(streams[0].filtro.valores[FONTEFILTRO_RESOLUCAO] & fontefiltro_bit(FONTEFILTRO_RESOLUCAO, 2));
    free(streams); streams = NULL;
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
