#!/bin/bash
# LEGACY_NODE must be a Node WITHOUT globalThis (Node 10.x, V8 6.8 ~ Chrome 68/69),
# e.g. a wrapper around: docker run --rm -v "$PWD":"$PWD" -w "$PWD" node:10.24.1-alpine node "$@"
# An old engine is intentional: Tizen 5.5 is Chromium M69 and globalThis is M71.
# Input: the glue built by tools/tizen.sh (default build/tizen/index.js).
set -euo pipefail
cd "$(dirname "$0")/.."
: "${LEGACY_NODE:?Set LEGACY_NODE to a Node 10 executable}"
GLUE="${1:-build/tizen/index.js}"
OUT=build/tizen-globalthis-test
RUNNER=tests/tizen-globalthis-runner.cjs
mkdir -p "$OUT"

# sem = glue sem o polyfill (tira a linha 1 se for ele); com = polyfill + sem.
if head -n 1 "$GLUE" | grep -q 'nuvio:globalThis'; then
  tail -n +2 "$GLUE" > "$OUT/sem.js"
else
  cp "$GLUE" "$OUT/sem.js"
fi
cat tools/tizen-globalthis.js "$OUT/sem.js" > "$OUT/com.js"

for modo in pagina worker; do
  if "$LEGACY_NODE" "$RUNNER" "$OUT/sem.js" "$modo" > "$OUT/sem-$modo.log" 2>&1; then
    echo "FAIL: sem polyfill passou como $modo; o motor tem globalThis?" >&2
    cat "$OUT/sem-$modo.log" >&2
    exit 1
  fi
  grep -q 'ReferenceError: globalThis is not defined' "$OUT/sem-$modo.log"
  echo "sem polyfill, $modo: $(cat "$OUT/sem-$modo.log")"

  # Controle: o mesmo arquivo num motor com globalThis nao da esse erro.
  node "$RUNNER" "$OUT/sem.js" "$modo" --moderno > "$OUT/moderno-$modo.log" 2>&1
  echo "sem polyfill, $modo, node $(node -v): $(cat "$OUT/moderno-$modo.log")"

  "$LEGACY_NODE" "$RUNNER" "$OUT/com.js" "$modo" > "$OUT/com-$modo.log" 2>&1 || {
    cat "$OUT/com-$modo.log" >&2; exit 1; }
  echo "com polyfill, $modo: $(cat "$OUT/com-$modo.log")"
done

# O artefato de verdade tem de trazer o polyfill (tools/tizen.sh).
if ! head -n 1 "$GLUE" | grep -q 'nuvio:globalThis'; then
  echo "FAIL: $GLUE nao comeca pelo polyfill; rode tools/tizen.sh" >&2
  exit 1
fi
echo 'PASS: sem polyfill morre na linha 6 (pagina e worker); com polyfill avalia'
