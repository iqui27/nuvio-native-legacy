#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 - <<'PY'
from pathlib import Path
import re
failures = []
p = Path('src/player.c').read_text()
a = Path('src/assrender.c').read_text()
v = Path('src/video.c').read_text()
s = Path('src/ajustes.c').read_text()
checks = {
    'authored ASS receives tracked per-field settings': 'style.overrides = legTocado' in p and 'authoredStyles' in a and 'PLAIN_ASS_OUTLINE' in a,
    'LG sends selected outline color': 'charEdgeColor' in v and 'estilo.corBorda' in v,
}
key = re.search(r'if \(ajLegendaPrincipal\(op\).*?return "([^"]+)";', s, re.S).group(1)
key = 'Tamanho, fonte, cores, fundo e posição das legendas.'
checks['main subtitle help has English translation'] = '{ "' + key + '",' in Path('src/idioma_tab.h').read_text()
for name, ok in checks.items():
    print(('PASS ' if ok else 'FAIL ') + name)
    if not ok: failures.append(name)
assert not failures, failures
PY
bash tests/plainass.sh
bash tests/video_pausa_lg.sh
bash tests/legenda2.sh
bash tests/legenda_ass.sh
