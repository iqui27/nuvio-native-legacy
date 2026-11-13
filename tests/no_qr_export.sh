#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 - <<'PY'
from pathlib import Path
for name in ['logexport.c','logexport.h']:
    assert not (Path('src')/name).exists()
for name in ['app.c','celbotao.c','celbotao.h','celular.c','celular.h','registro.c','registro_ilha.inc']:
    s=(Path('src')/name).read_text()
    for marker in ['CELB_LOG','logexport','celular_abrir_relatorio','Export diagnostics','paginaRelatorio']:
        assert marker not in s,(name,marker)
assert 'celular_abrir(titulo)' in Path('src/celbotao.c').read_text()
assert 'int celular_abrir(const char *titulo)' in Path('src/celular.c').read_text()
print('PASS diagnostic QR export removed; ordinary phone text input and log panel retained')
PY
