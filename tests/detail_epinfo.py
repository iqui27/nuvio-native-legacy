#!/usr/bin/env python3
"""Check approved fixed episode metrics and content paths without GL."""
from pathlib import Path
import subprocess
import tempfile

source = Path('src/detail.c').read_text()
card = source.split('static void desenhaEpisodioEm(', 1)[1].split('// Abas de informacao:', 1)[0]
assert 'cat_episodio(idx, epAbsoluto(c))' in card
assert 'gfx_rect(th, t2, GFX_CARD, f,' in card
assert 'th = thLayout;' in card
assert 'vistoep_estado(serie->imdb, ep->temporada, ep->episodio) == 1' in card
assert 'epOculto(ep)' in card and 'travaui_segura' in card
assert 'if (c!=col) return;' in card
assert 'epMeta(meta,sizeof meta,epDur,epData,NULL)' in card
metrics = source.split('static int epModerno(void) {', 1)[1].split('// Bloco das Notas', 1)[0]
program = '''
#include <assert.h>
#include <stdio.h>
#include "layout.h"
#include "detail.h"
#include "ajustes.h"
static int layout = HOME_LAYOUT_MODERNA;
int ajustes_home_layout(void) { return layout; }
static int epModerno(void) {''' + metrics + '''
int main(void) {
  assert(epCardW()==420 && epThumbH()==236 && epCardH()==530);
  assert(epCardPasso()==448 && epCardPasso()-epCardW()==28);
  assert(epCardH()-epThumbH()>=268+3*36-236);
  layout=HOME_LAYOUT_DINAMICA;
  assert(epCardW()==420 && epThumbH()==236 && epCardH()==500 && epCardPasso()==452);
  layout=HOME_LAYOUT_PADRAO;
  assert(epCardW()==640 && epThumbH()==414 && epCardH()==414 && epCardPasso()==672);
  puts("PASS: layout-specific episode geometry and content guards");
}
'''
with tempfile.TemporaryDirectory(prefix='epinfo-host-') as tmp:
    cfile = Path(tmp) / 'check.c'
    binary = Path(tmp) / 'check'
    cfile.write_text(program)
    subprocess.run(['cc', '-std=c99', '-Isrc', '-I/opt/homebrew/include/SDL2',
                    '-I/opt/homebrew/include', str(cfile), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
