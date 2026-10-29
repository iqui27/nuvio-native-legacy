#!/bin/bash
# New translations must be complete, sorted and add no validator errors.
set -euo pipefail
cd "$(dirname "$0")/.."
git diff --check
cc -fsyntax-only src/juntos.c src/juntosui.c src/salvospainel.c src/ilha.c src/app.c \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations
python3 - <<'PY'
import importlib.util
import re
import subprocess
import tempfile
from pathlib import Path
spec = importlib.util.spec_from_file_location('idiomas', 'tools/idiomas.py')
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
keys = {'Assistir Juntos em andamento', 'Escolha uma fonte para começar',
        'Esperando o anfitrião escolher uma fonte', 'Sessão em andamento', 'Voltar à sessão'}
for cod in m.IDIOMAS:
    found = {m.decodificar(k).decode(): v for _, k, v in m.ler_irma(cod)}
    assert all(found.get(k) for k in keys), cod
_, current = m.conferir()
with tempfile.TemporaryDirectory() as tmp:
    for name in ['idioma_tab.h'] + ['idioma_' + c + '.h' for c in m.IDIOMAS]:
        text = subprocess.check_output(['git', 'show', 'HEAD:src/' + name], text=True)
        Path(tmp, name).write_text(text)
    m.SRC = Path(tmp)
    _, baseline = m.conferir()
normalize = lambda rows: sorted(re.sub(r':\d+:', ':LINE:', s) for s in rows)
assert normalize(current) == normalize(baseline), '\n'.join(current)
print(f'New translations complete in {len(m.IDIOMAS)} languages; sorted; no new errors.')
print(f'Existing validator errors on HEAD: {len(baseline)} (Serbian script).')
PY
