#!/bin/bash
# Source guards for the episode-render review: highp faux bold, dithered
# merged veil, and holes only under fully opaque episode rows.
set -eu
cd "$(dirname "$0")/.."
python3 - <<'EOF'
import re, sys
g = open('src/gfx.c').read(); d = open('src/detail.c').read()
def block(s, start, end):
    i = s.index(start); return s[i:s.index(end, i)]
txt = block(g, '// GFX_TEXTO', '// GFX_FUNDO')
bold = block(txt, 'if (uPar.x < 0.0)', 'return;')
card = block(g, 'if (uVeu.x < 0.0) {', '// GFX_SOMBRA')
furos = block(d, 'static int epFuros(GfxRect *holes) {', '\n}\n')
fails = []
if not re.search(r'uniform NV_HP vec3 uPeso;', g): fails.append('uPeso not highp')
if re.search(r'(^|[^P] )vec2 (uv|p) =', bold) or 'uPar.y' in bold or 'uRaio' in bold:
    fails.append('faux-bold coords/offsets not highp')
if 'nv_dither(cor' not in card: fails.append('merged episode veil not dithered')
if 'gfx_opacidade_grupo' not in furos: fails.append('epFuros ignores row/group alpha')
rf = block(g, 'void gfx_rect_furos(', '\nvoid gfx_rect(')
if 'fillGpuArea(r,' in rf or 'GFX_TR_RECT' not in rf or 'escAtiva' not in rf:
    fails.append('rect_furos visibility check uses unscaled coordinates')
if 'if (epModerna() && ft > .005f)' not in d:
    fails.append('Modern C focus panel missing')
if 'NUVIO_EP_MATERIAL' in d: fails.append('temporary material switch remains')
if 'GfxRect th = {x, epRowY()-scrollY, epCardW(), epThumbH()};' not in furos:
    fails.append('translucent panel wrongly culls backdrop')
for f in fails: print('FAIL:', f)
print('OK' if not fails else ''); sys.exit(1 if fails else 0)
EOF
python3 - <<'EOF'
from pathlib import Path
p=Path('src/epcoment.c').read_text(); m=Path('src/episodios.c').read_text()
d=Path('src/descoberta.c').read_text()
assert 'int selected=k==filtro;' in p
assert 'int available[11] = {1}' in p and 'while (!available[filtro])' in p
assert '{{"→",buf},{"←",i18n("Menu")}}' in p and 'notasui_desenhar_linha(&plano' in p
assert 'VM_ANTERIOR' not in m and 'VM_PROXIMO' not in m
assert 'txt_bloco(TXT_ILHA_NOME,vmCab' in m and 'vmTituloEscala' in m
assert 'gfx_rect(th, t2, GFX_CARD, f,' in Path('src/detail.c').read_text()
assert 'rating=js_num(p,f,"rating",0.0)' in d
assert 'ajustes_tmdb_basico() || ajustes_nota_episodio(EX_TMDB)' in d
print('PASS: episode direction, filter, full title, focus shader and rating data guards')
EOF
python3 tests/detail_episode_tint.py
