#!/usr/bin/env python3
"""Local PR #487 review probes; never modifies production sources."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'src/streams.c').read_text()
# Compile the current formatting and declared-audio helpers under sanitizers.
helpers = source[source.index('static int faixaIdiomaInformado(const char *raw, size_t n) {'):source.index('static float desenharFaixas(')]
filter_helpers = source[source.index('static int audioDeclaradoEm('):source.index('// O IDIOMA nao tem logo')]
probe = r'''
#include "streams.h"
#include "linguas.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
static const char *i18n(const char *s) { return s; }
HELPERS
FILTER_HELPERS
static int failures;
static void check(const char *label, const char *json, const char *audio,
                  const char *subs, int portuguese) {
  Stream *v = NULL;
  int count = stream_extrair(json, "fixture", &v);
  assert(count == 1);
  char a[4096], s[4096];
  stream_faixas_texto(v, 0, a, sizeof a);
  stream_faixas_texto(v, 1, s, sizeof s);
  printf("%s: raw audio=[%s], raw subtitles=[%s], displayed audio=[%s], PT filter=%d\n",
         label, v->audioIdiomas, v->legendaIdiomas, a, audioDeclaradoEm(v, "pt"));
  if (strcmp(a, audio) || strcmp(s, subs) || (portuguese >= 0 && audioDeclaradoEm(v, "pt") != portuguese)) {
    printf("FAIL %s: expected audio=[%s], subtitles=[%s], PT filter=%d\n", label, audio, subs, portuguese);
    failures++;
  }
  free(v);
}
int main(void) {
  check("no metadata", "{\"streams\":[{\"url\":\"https://x.invalid/v\"}]}", "", "", 0);
  check("regional variants and dedup", "{\"streams\":[{\"url\":\"https://x.invalid/v\",\"audioLanguages\":[\"pt-BR\",\"pob\",\"pt-PT\",\"es-419\",\"zh-Hant\"]}]}",
        "Português (BR), Português (PT), Espanhol (419), Chinês (HANT)", "", -1);
  check("AIO structured", "{\"streams\":[{\"url\":\"https://x.invalid/v\",\"streamData\":{\"parsedFile\":{\"languages\":[\"English\",\"Portuguese (Brazil)\"],\"subtitles\":[\"French\"]}}}]}", "Inglês, Português (BR)", "Francês", -1);
  check("Malayalam name", "{\"streams\":[{\"url\":\"https://x.invalid/v\",\"audioLanguages\":[\"Malayalam\"]}]}", "Malayalam", "", -1);
  check("AIO flag marker", "{\"streams\":[{\"url\":\"https://x.invalid/v\",\"description\":\"⚑ English | Portuguese\"}]}", "Inglês, Português", "", -1);
  check("structured PT and existing filter", "{\"streams\":[{\"url\":\"https://x.invalid/v\",\"audioLanguages\":[\"pt-BR\"]}]}", "Português (BR)", "", 1);
  check("globes", "{\"streams\":[{\"url\":\"https://x.invalid/v\",\"description\":\"🌐 EN | PT\\n📝 French\"}]}", "Inglês, Português", "Francês", -1);
  check("labeled flags", "{\"streams\":[{\"url\":\"https://x.invalid/v\",\"description\":\"🗣 🇬🇧 | 🇧🇷\\n📝 🇫🇷\"}]}", "Inglês, Português (BR)", "Francês", -1);
  check("Torrentio Subs", "{\"streams\":[{\"url\":\"https://x.invalid/v\",\"description\":\"🇬🇧 Multi Subs / 🇫🇷\"}]}", "", "Francês", -1);
  check("NUL rejected", "{\"streams\":[{\"url\":\"https://x.invalid/v\",\"audioLanguages\":[\"en\\u0000pt\"]}]}", "", "", -1);
  check("header cannot supply languages", "{\"streams\":[{\"url\":\"https://x.invalid/v\",\"behaviorHints\":{\"proxyHeaders\":{\"request\":{\"audioLanguages\":[\"pt\"]}}}}]}", "", "", -1);
  // Every prefix exercises truncation boundaries, including surrogate pairs,
  // escaped strings, arrays, nested objects, and invalid Unicode bytes.
  const char *samples[] = {
    "{\"streams\":[{\"url\":\"https://x.invalid/v\",\"audioLanguages\":[\"en\",\"\\ud83c\\udde7\\ud83c\\uddf7\"],\"subtitleTracks\":[{\"language\":{\"code\":\"pt\"}}]}]}",
    "{\"streams\":[{\"url\":\"https://x.invalid/v\",\"description\":\"🗣 🇬🇧 | 🇧🇷\\n📝 French\"}]}",
    "{\"streams\":[{\"url\":\"https://x.invalid/v\",\"audioLanguages\":[\"\xf0\x9f\",\"\xed\xa0\x80\"]}]}"
  };
  for (unsigned i = 0; i < sizeof samples / sizeof *samples; i++) {
    size_t n = strlen(samples[i]);
    char *buf = malloc(n + 1);
    for (size_t k = 0; k <= n; k++) {
      memcpy(buf, samples[i], k); buf[k] = 0;
      Stream *v = NULL; int count = stream_extrair(buf, "fixture", &v);
      assert(count >= 0);
      for (int j = 0; j < count; j++) {
        char out[4096]; stream_faixas_texto(v + j, 0, out, sizeof out);
        assert(memchr(v[j].audioIdiomas, 0, sizeof v[j].audioIdiomas));
        assert(memchr(v[j].legendaIdiomas, 0, sizeof v[j].legendaIdiomas));
      }
      free(v);
    }
    free(buf);
  }
  // Large adversarial values and over-deep structures must stay bounded.
  for (int mode = 0; mode < 4; mode++) {
    size_t cap = 300000;
    char *buf = malloc(cap); size_t u = 0;
    u += snprintf(buf + u, cap - u, "{\"streams\":[{\"url\":\"https://x.invalid/v\",");
    if (mode == 0 || mode == 1) {
      u += snprintf(buf + u, cap - u, mode ? "\"description\":\"Audio: " : "\"audioLanguages\":\"");
      memset(buf + u, 'A', 200000); u += 200000;
      u += snprintf(buf + u, cap - u, "\"");
    } else {
      u += snprintf(buf + u, cap - u, "\"audioLanguages\":[");
      if (mode == 2) {
        for (int k = 0; k < 10000; k++) u += snprintf(buf + u, cap - u, "%s\"en\"", k ? "," : "");
      } else {
        for (int k = 0; k < 100; k++) buf[u++] = '[';
        u += snprintf(buf + u, cap - u, "\"en\"");
        for (int k = 0; k < 100; k++) buf[u++] = ']';
      }
      buf[u++] = ']';
    }
    snprintf(buf + u, cap - u, "}]}");
    Stream *v = NULL; assert(stream_extrair(buf, "fixture", &v) == 1);
    assert(memchr(v->audioIdiomas, 0, sizeof v->audioIdiomas));
    if (mode == 2) assert(!strcmp(v->audioIdiomas, "en"));
    else assert(!v->audioIdiomas[0]);
    free(v); free(buf);
  }
  puts("Truncation/Unicode/large-value/depth probes completed under ASan/UBSan.");
  return failures ? 1 : 0;
}
'''.replace('HELPERS', helpers, 1).replace('FILTER_HELPERS', filter_helpers, 1)
with tempfile.TemporaryDirectory(prefix='pr487-review-') as temp:
    cfile = Path(temp) / 'probe.c'
    binary = Path(temp) / 'probe'
    cfile.write_text(probe)
    subprocess.run(['cc', str(cfile), 'src/stream_parse.c', 'src/js.c', 'src/badges.c', 'src/linguas.c', 'src/fontefiltro.c',
                    '-Isrc', '-I/opt/homebrew/include', '-O1', '-g', '-ffunction-sections',
                    '-fdata-sections', '-Wl,-dead_strip', '-fsanitize=address,undefined,float-cast-overflow',
                    '-fno-omit-frame-pointer', '-fno-sanitize-recover=all', '-o', str(binary)], cwd=root, check=True)
    raise SystemExit(subprocess.run([str(binary)], cwd=root).returncode)
