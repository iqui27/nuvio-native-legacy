#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${EMSDK_DIR:=$HOME/emsdk}"
mkdir -p build/tizen-memory
for mib in 128 256; do
  "$EMSDK_DIR/upstream/emscripten/emcc" tests/tizen-memory.c -O2 -pthread \
    -sPTHREAD_POOL_SIZE=12 -sSTACK_SIZE=8388608 \
    -sDEFAULT_PTHREAD_STACK_SIZE=8388608 -sWASM_BIGINT=0 \
    -sINITIAL_MEMORY=$((mib * 1024 * 1024)) -sALLOW_MEMORY_GROWTH=0 \
    -sABORTING_MALLOC=1 -sASSERTIONS=1 -sENVIRONMENT=node \
    -o "build/tizen-memory/$mib.js"
done
python3 - <<'PY'
import subprocess
from pathlib import Path
for mib in (128, 256):
    result = subprocess.run(['node', f'build/tizen-memory/{mib}.js'],
        capture_output=True, text=True, timeout=30)
    log = result.stdout + result.stderr
    Path(f'build/tizen-memory/{mib}.log').write_text(log)
    if mib == 128:
        assert result.returncode != 0 and 'OOM' in log, log[-2000:]
        print('PASS: 128 MiB reproduz falta de memoria sem crescimento')
    else:
        assert result.returncode == 0 and 'PASS: 12 workers' in log, log[-2000:]
        print(log.strip())
PY
