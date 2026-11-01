#!/usr/bin/env python3
"""Run production selection blocks without SDL/GL; never fetch artwork.

Optional source root lets the same regression run against the integration base.
Only presentation code is sliced out; poster provider logic is linked unchanged.
"""
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else ROOT


def source(name):
    return (SOURCE / 'src' / name).read_text()


def function(file, name):
    text = source(file)
    start = text.index('static const char *' + name + '(')
    return text[start:text.index('\n}', start) + 2]


harness = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "catalogo.h"
#include "posterprov.h"
static int calls;
static const char *tracked(const char *origin, const char *imdb, long tmdb,
                           const char *type, const char *poster) {
  calls++;
  return posterprov_card_addon(origin, imdb, tmdb, type, poster);
}
#define posterprov_card_addon tracked
static int ajustes_hero_fonte(void) { return 0; }
static int ajustes_hero_arte_diferente(void) { return 0; }
static float tex_aspecto(const char *url) { (void)url; return 1.8f; }
static const char *artehero_url_card_fonte(const CatItem *ci, int f, int d) {
  (void)f; (void)d; return ci->backdrop;
}
static const char *artehero_url_metahub_fundo(const CatItem *ci) { return ci->backdrop; }
static float retPt(float x) { return x; }
typedef unsigned GLuint;
static struct { float h; } p = {300};
static CatItem fixture;
static int empty;
static int lst_item(int i, CatItem *dst) {
  if (!empty && i == 0) { *dst = fixture; return 1; }
  /* Failed slots must ignore output, even when it still contains old art. */
  strcpy(dst->backdrop, "stale-backdrop");
  return 0;
}
'''
harness += function('home.c', 'arte_por_formato') + '\n'
harness += function('spotlight.c', 'arteDe') + '\n'

# Both search renderers: For you/result cards and Popular rows.
search = source('busca.c')
blocks = list(re.finditer(
    r'const char \*arte = posterprov_card_addon\([^;]+;.*?'
    r'if \(!arte\[0\]\) arte = ci->backdrop\[0\] \? ci->backdrop : NULL;',
    search, re.S))
assert len(blocks) == 2, 'search artwork blocks changed; update host harness'
for name, block in zip(('search_for_you', 'search_popular'), blocks):
    harness += f'\nstatic const char *{name}(const CatItem *ci) {{\n'
    harness += block.group() + '\nreturn arte;\n}\n'

# Execute the actual list-cover loop selection, retaining local scope and guard.
context = source('ctxmenu.c')
start = context.index('  for (i = 0; i < CTXL_CAPAS; i++) {')
selection = context[context.index('    CatItem it', start):
                    context.index('    if (u) t = tex_obter_larg', start)]
harness += '\nstatic void covers(char out[2][PP_URL_MAX]) {\n'
harness += 'for (int i = 0; i < 2; i++) {\n' + selection
harness += 'snprintf(out[i], PP_URL_MAX, "%s", u ? u : "");\n}\n}\n'
harness += r'''
static const char *catalog(const CatItem *ci) { return arte_por_formato(ci, 0); }
static const char *spotlight(const CatItem *ci) { return arteDe(ci, 0); }
int main(void) {
  PosterProvCfg cfg = { .prov = PP_MODELO };
  strcpy(cfg.modelo, "https://posters.example/{imdb}.jpg");
  posterprov_configurar(&cfg);
  strcpy(fixture.origem, "https://addon.example");
  strcpy(fixture.imdb, "tt0111161");
  strcpy(fixture.tipo, "movie");
  strcpy(fixture.poster, "https://addon.example/poster.jpg");
  strcpy(fixture.backdrop, "https://addon.example/backdrop.jpg");
  const char *(*paths[])(const CatItem *) = {
    catalog, search_for_you, search_popular, spotlight
  };
  const char *names[] = {"catalog", "search For you", "search Popular", "Spotlight"};
  char out[2][PP_URL_MAX];
  for (int prefer = 0; prefer <= 1; prefer++) {
    posterprov_preferir_addon(prefer);
    const char *expected = prefer ? fixture.poster : "https://posters.example/tt0111161.jpg";
    for (int i = 0; i < 4; i++) {
      calls = 0;
      const char *actual = paths[i](&fixture);
      if (!actual || strcmp(actual, expected) || calls != 1) {
        fprintf(stderr, "FAIL: %s, addon preference %d\n", names[i], prefer);
        return 1;
      }
    }
    calls = 0;
    covers(out);
    assert(!strcmp(out[0], expected) && !out[1][0] && calls == 1);
  }
  posterprov_preferir_addon(0);
  calls = 0;
  assert(!strcmp(arte_por_formato(&fixture, 1), fixture.backdrop));
  assert(!strcmp(arteDe(&fixture, 1), fixture.backdrop));
  assert(calls == 0); /* Landscape never enters the portrait override. */
  empty = 1;
  covers(out);
  assert(!out[0][0] && !out[1][0] && calls == 0);
  empty = 0;
  cfg.prov = PP_DESLIGADO;
  posterprov_configurar(&cfg);
  fixture.poster[0] = 0;
  covers(out);
  assert(!strcmp(out[0], fixture.backdrop) && !out[1][0]);
  fixture.backdrop[0] = 0;
  covers(out);
  assert(!out[0][0] && !out[1][0]);
  puts("ok #708: catalog, search For you/Popular, list covers, Spotlight; addon priority, landscape, empty slots");
  return 0;
}
'''
# posterprov_redigir's unrelated log helper; no networking in this test.
harness += r'''
const char *rede_url_publica(const char *url, char *dst, unsigned n) {
  (void)url; if (n) dst[0] = 0; return dst;
}
'''
with tempfile.TemporaryDirectory(prefix='poster708-', dir=os.environ.get('TMPDIR')) as tmp:
    test = Path(tmp) / 'poster708.c'
    test.write_text(harness)
    binary = Path(tmp) / 'poster708'
    subprocess.run(['timeout', '60', os.environ.get('CC', 'cc'), '-std=c11', '-O1',
                    '-g', '-fsanitize=address,undefined', '-pthread', '-I' + str(SOURCE / 'src'),
                    str(test), str(SOURCE / 'src/posterprov.c'), '-o', str(binary)], check=True)
    subprocess.run(['timeout', '30', str(binary)], check=True)
