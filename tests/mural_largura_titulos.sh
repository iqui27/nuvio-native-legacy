#!/bin/bash
# #521: o Mural nao pode passar por baixo do painel da direita (com a rail
# fixa so cabem 4 colunas) e o nome no ladrilho e um ajuste local, ligado de fabrica.
set -eu
cd "$(dirname "$0")/.."
ROOT="${MURAL_ROOT:-.}" python3 -I - <<'PY'
import os, re
from pathlib import Path
r = Path(os.environ['ROOT'])
v = (r / 'src/vertudo.c').read_text()
a = (r / 'src/ajustes.c').read_text()
ok = True
def falha(m):
    global ok
    print('FALHA', m); ok = False
if not re.search(r'#define VT_CW\s+vtLarguraUtil\(\)', v) or 'vtFim() - vtInicio()' not in v:
    falha('VT_CW nao e limitado ao espaco entre a rail e o painel')
m = re.search(r'case VT_LAY_MURAL: \{.*?sh = (.*?),\s*sw', v, re.S)
if not m or 'VT_CW' not in m.group(1):
    falha('o ladrilho pequeno do Mural nao encolhe com VT_CW')
t = re.search(r'static void muralTile\(.*?\n\}\n', v, re.S).group(0)
if 'ajustes_mural_titulos()' not in t:
    falha('muralTile nao le o ajuste de titulos')
if 'AJ_MURAL_TITULOS' not in a or '"muralTitulosLocal"' not in a:
    falha('ajuste AJ_MURAL_TITULOS nao registrado')
# geometria: TV 1920, margem 64, painel 336, vao 24, g 28; rail fixa => x0 218
for x0 in (218.0, 64.0, 300.0):
    W = min(5 * 248 + 4 * 28, 1920 - 64 - 336 - 24 - x0)
    g = 28.0
    sh = min(174.0, (W - 2 * g - 16 / 9 * g) / (64 / 9))
    tot = (2 * sh + g) * 16 / 9 + 2 * sh * 16 / 9 + 2 * g
    if tot > W + 0.5:
        falha(f'Mural de {tot:.0f}px passa de {W:.0f}px com x0={x0}')
raise SystemExit(0 if ok else 1)
PY
