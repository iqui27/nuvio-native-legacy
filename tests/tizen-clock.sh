#!/bin/bash
# LEGACY_NODE must be a Node with Wasm/BigInt integration disabled, e.g. 12.22.12.
# An old engine is intentional: a modern browser hides this regression.
set -euo pipefail
cd "$(dirname "$0")/.."
: "${EMSDK_DIR:=$HOME/emsdk}"
: "${LEGACY_NODE:?Set LEGACY_NODE to a Node 12 executable}"
EMCC="$EMSDK_DIR/upstream/emscripten/emcc"
OUT=build/tizen-clock-test
mkdir -p "$OUT"

for variant in before after; do
  flags=()
  if [ "$variant" = after ]; then flags=(-sWASM_BIGINT=0); fi
  "$EMCC" tests/tizen-clock.c src/marco.c -Isrc -O2 \
    -sENVIRONMENT=shell -sSINGLE_FILE=1 -sASSERTIONS=0 \
    ${flags[@]+"${flags[@]}"} -o "$OUT/$variant.js"
  npx --yes esbuild@0.25.0 "$OUT/$variant.js" --target=chrome76 \
    --outfile="$OUT/$variant.76.js" --log-level=error
done

if "$LEGACY_NODE" --unhandled-rejections=strict tests/tizen-clock-runner.cjs \
    "$OUT/before.76.js" > "$OUT/before.log" 2>&1; then
  echo 'FAIL: baseline passed; use an engine without Wasm/BigInt integration' >&2
  exit 1
fi
grep -q 'antes de marco_iniciar' "$OUT/before.log"
grep -q 'wasm function signature contains illegal type' "$OUT/before.log"
"$LEGACY_NODE" --unhandled-rejections=strict tests/tizen-clock-runner.cjs \
  "$OUT/after.76.js" > "$OUT/after.log" 2>&1
grep -q 'depois de marco_iniciar' "$OUT/after.log"
grep -q 'relogio e arquivo ok' "$OUT/after.log"
cat "$OUT/after.log"
echo 'PASS: original fails at marco_iniciar; legalized ABI completes it'
