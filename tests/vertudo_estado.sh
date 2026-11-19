#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
python3 - <<'PY'
from pathlib import Path
import subprocess, tempfile
s = Path('src/vertudo.c').read_text()
def function(name):
    start = s.index('static ', s.index(name) - 20)
    brace = s.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (s[end] == '{') - (s[end] == '}')
        end += 1
    return s[start:end]
code = '''
#include <assert.h>
typedef struct { int progresso, watched; } CatItem;
typedef struct { float x,y,w,h; } GfxRect;
static int enabled=1, checks, bars;
int cat_visto(const CatItem *it) { return it->watched; }
int desc_filme_visto(const CatItem *it) { return it->watched; }
int ajustes_selo_visto(void) { return enabled; }
void badges_check_cartao(GfxRect r,int round,float a) { checks++; }
void badges_progresso(GfxRect r,float p,float a) { bars++; }
'''+function('vistoItem')+'\n'+function('marcaEstado')+'''
int main(void) {
  GfxRect r={0}; CatItem it={95,0};
  assert(!vistoItem(&it));
  marcaEstado(r,&it,1); assert(checks==0 && bars==1);
  it.watched=1; marcaEstado(r,&it,1); assert(checks==1 && bars==1);
  enabled=0; marcaEstado(r,&it,1); assert(checks==1 && bars==1);
  it.watched=0; marcaEstado(r,&it,1); assert(checks==1 && bars==2);
  return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
    source=Path(d)/'estado.c'; binary=Path(d)/'estado'
    source.write_text(code)
    subprocess.run(['cc',str(source),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
bg = s[s.index('static void themeBackground('):s.index('// PONTEIRO (#99)')]
for banned in ('Directors', 'retratoLocal', 'diretor_foto', 'GFX_RETRATO', 'tex_publicar_paleta'):
    assert banned not in bg, 'director portrait still drawn: ' + banned
print('vertudo_estado: PASS')
PY
