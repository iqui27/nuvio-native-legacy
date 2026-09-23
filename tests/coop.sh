#!/bin/bash
# Fibras cooperativas do alvo Tizen 4 (src/coop.c), em wasm2js como na TV.
# Opcional: LEGACY_NODE=<node 7.x> roda tambem num V8 da idade do Chromium M56.
set -euo pipefail
cd "$(dirname "$0")/.."
: "${EMSDK_DIR:=$HOME/emsdk}"
EMCC="$EMSDK_DIR/upstream/emscripten/emcc"
OUT=build/coop-test
mkdir -p "$OUT"
"$EMCC" -O2 -DNV_COOP=1 -include src/coop_fio.h -Isrc \
  src/coop.c tests/coop.c -sUSE_SDL=2 \
  -sWASM=0 -sASYNCIFY -sASYNCIFY_STACK_SIZE=65536 -sSTACK_SIZE=1048576 \
  -sINITIAL_MEMORY=134217728 -sENVIRONMENT=node,shell -sEXIT_RUNTIME=1 \
  -o "$OUT/coop.js"
node "$OUT/coop.js" | tee "$OUT/coop.log"
grep -q 'coop: tudo certo' "$OUT/coop.log"
if [ -n "${LEGACY_NODE:-}" ]; then
  npx --yes esbuild@0.25.0 "$OUT/coop.js" --target=chrome56 \
    --outfile="$OUT/coop.56.js" --log-level=error
  "$LEGACY_NODE" "$OUT/coop.56.js" | tee "$OUT/coop.56.log"
  grep -q 'coop: tudo certo' "$OUT/coop.56.log"
fi
echo 'PASS: coop'
