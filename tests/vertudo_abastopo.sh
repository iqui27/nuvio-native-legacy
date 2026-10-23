#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
python3 - <<'PY'
from pathlib import Path
import re, subprocess, tempfile
s = Path('src/vertudo.c').read_text()
def function(name):
    m = re.search(r'static [^\n]+\b' + name + r'\([^\n]*\)\s*\{', s)
    assert m, 'missing function: ' + name
    end = m.end(); depth = 1
    while depth:
        depth += (s[end] == '{') - (s[end] == '}'); end += 1
    return s[m.start():end]
code = '''
#include <assert.h>
static int setting, tabs=2;
int ajustes_abas_estilo(void) { return setting; }
int nAbas(void) { return tabs; }
''' + function('estiloAbas') + '''
int main(void) {
  for (setting=0; setting<=4; setting++) assert(estiloAbas()==setting);
  setting=5; assert(estiloAbas()==0);
  setting=-1; assert(estiloAbas()==0);
  tabs=0; setting=4; assert(estiloAbas()==0);
}
'''
enum = re.search(r'enum \{ VT_AB_CHIPS[^}]+\};', s).group()
with tempfile.TemporaryDirectory() as d:
    p = Path(d)/'test.c'; p.write_text(enum+'\n'+code)
    subprocess.run(['cc', str(p), '-o', d+'/test'], check=True)
    subprocess.run([d+'/test'], check=True)
assert 'VT_AB_TOPO' in s
# Style toggle is a segment of the single layout switcher (no loose button).
assert 'botaoEstiloAbas' not in s
assert 'ponteiro_alvo(' in function('botaoExibicao')
assert 'aj_layout-list' in function('botaoExibicao')
assert 'gfx_vidro_pilula_cheia(' in function('botaoExibicao')  # focus = white glass pill (direction A)
assert 'alternarEstiloAbas();' in s
assert 'tabCursor<na+(nAbas()>0)' in s
assert 'if(est==VT_AB_TOPO)' in s
defines = {k: float(re.search(r'#define '+k+r'\s+(\d+\.\d+)f', s).group(1)) for k in ('VT_AB_Y','VT_AB_H')}
content = float(re.search(r'#define VT_TOPO\s+\(collection \? (\d+\.\d+)f', s).group(1))
assert defines['VT_AB_Y'] + defines['VT_AB_H'] < content
assert 'VT_AB_PAGINAS, VT_AB_TOPO' in s
assert '"Menu no topo"' in Path('src/ajustes.c').read_text()
print('vertudo_abastopo: PASS (values, icon target, D-pad, menu above content)')
PY
