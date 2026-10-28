#!/bin/bash
# Issue #647: LegaSync naming + existing account/profile settings sync.
set -euo pipefail
cd "$(dirname "$0")/.."

python3 - <<'PY'
from pathlib import Path
import re

root = Path('src')
langs = sorted(root.glob('idioma_*.h'))
assert langs, 'missing language tables'

for path in langs:
    text = path.read_text(encoding='utf-8')
    assert 'Sincronização (LegaSync)' in text, f'missing LegaSync label in {path.name}'
    if path.name == 'idioma_tab.h':
        assert re.search(r'\{ "Sincronização \(LegaSync\)", "Sync \(LegaSync\)" \}', text)

print('PASS: LegaSync label present in all language tables')
PY

bash tests/ajustes_sync.sh roundtrip defaults empty offline legacy webedit quota
bash tests/ajustes_escopo.sh
bash tests/posterprov.sh
bash tests/m3u.sh

echo 'PASS: issue647 legasync proof'
