#!/bin/bash
# pt-BR before/after captures for issue #647 (Glass UI settings island).
set -euo pipefail
cd "$(dirname "$0")/.."

out="${1:-/Volumes/ExternalSSD/tmp/rev/issue647-legasync-png}"
mkdir -p "$out"

# BEFORE: old label, still in Glass UI settings.
python3 - <<'PY' "$out"
from pathlib import Path
import sys

out = Path(sys.argv[1])
for path in Path('src').glob('idioma_*.h'):
    text = path.read_text(encoding='utf-8')
    if 'Sincronização (LegaSync)' not in text:
        continue
    text = text.replace('Sincronização (LegaSync)', 'Sincronização')
    path.write_text(text, encoding='utf-8')

aj = Path('src/ajustes.c').read_text(encoding='utf-8')
aj = aj.replace('LER("Sincronização (LegaSync)"),', 'LER("Sincronização"),')
Path('src/ajustes.c').write_text(aj, encoding='utf-8')
print(f'prepared before state in {out}')
PY

NUVIO_SHOT_IDIOMA=0 NUVIO_AJ_QUADROS="v2-sec-1" bash tests/ajustes_shot.sh "$out/antes"

# AFTER: restore current worktree state from git (uncommitted changes included).
git checkout -- src/ajustes.c src/idioma_*.h

NUVIO_SHOT_IDIOMA=0 NUVIO_AJ_QUADROS="v2-sec-1" bash tests/ajustes_shot.sh "$out/depois"

echo "capturas: $out/antes-v2-sec-1.png $out/depois-v2-sec-1.png"
