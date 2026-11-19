#!/bin/bash
# Verify only the three M2 review gaps; global scan has unrelated baseline failures.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 - <<'PY'
from pathlib import Path
import importlib.util, subprocess, tempfile
spec=importlib.util.spec_from_file_location('idiomas',Path('tools/idiomas.py'))
m=importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
for lang in m.IDIOMAS:
    entries={key:value for _,key,value in m.ler_irma(lang)}
    for key in ('Melhor para este iPhone','RESULTADOS','Títulos, pessoas'):
        assert entries[key],(lang,key)
print('PASS: three M2 keys translated in all 29 locale tables')
_,current=m.conferir()
with tempfile.TemporaryDirectory() as folder:
    target=Path(folder)
    for path in Path('src').glob('idioma*.h'):
        (target/path.name).write_bytes(subprocess.check_output(['git','show','00fd00ba5:src/'+path.name]))
    m.SRC=target
    _,base=m.conferir()
assert not set(current)-set(base)
print(f'PASS: global i18n baseline {len(base)}, current {len(current)}, no new issue')
PY
