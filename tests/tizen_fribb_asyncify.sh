#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 - <<'PY'
from pathlib import Path

source = Path('src/intro.c').read_text()
start = source.index('static int pedirAniskip(')
end = source.index('\n}', start)
body = source[start:end]
guard = body.index('#ifdef __EMSCRIPTEN__')
mapping = body.index('malMapeado(')
assert guard < mapping
assert 'Fribb mapping disabled on Tizen WGT' in body
assert '-sASYNCIFY_ONLY=[main,dados_iniciar]' in Path('tools/tizen.sh').read_text()
print('ok: Tizen WGT skips the worker Asyncify path with a clear log')
PY
