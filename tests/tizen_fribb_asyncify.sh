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
build = Path('tools/tizen.sh').read_text()
assert '-sASYNCIFY -sASYNCIFY_STACK_SIZE=32768' in build
assert '-sASYNCIFY_ONLY=' not in build, 'network waits must suspend through every caller'
network = Path('src/rede.c').read_text()
start = network.index('static void nv_http_esperar(')
end = network.index('\n}', start)
wait = network[start:end]
assert 'fio1_ceder();' in wait and 'emscripten_sleep(1);' in wait
print('ok: Tizen WGT retains the Fribb guard and instruments all network wait stacks')
PY
