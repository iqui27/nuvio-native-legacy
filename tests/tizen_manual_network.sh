#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp
work=$(mktemp -d "$TMPDIR/nuvio-tizen-manual.XXXXXX")
trap 'rm -rf "$work"' EXIT
python3 - "$work/manual.inc" <<'PY'
from pathlib import Path
import sys
s = Path('src/app.c').read_text()
def function(name):
    start = s.index('static void ' + name + '(')
    return s[start:s.index('\n}', start) + 2]
Path(sys.argv[1]).write_text(function('definirFonteVOD') + '\n' + function('abrirFonteManualDireta'))
PY
cc -std=gnu11 -Wall -Wextra -Werror -Isrc -I/opt/homebrew/include -I"$work" tests/tizen_manual_network.c src/redesaude.c -pthread -o "$work/manual"
# Prove the fixture catches the original Chromium 63 incompatibility without
# modifying the checkout (the default ref is the task's integration baseline).
git show "${TIZEN_NETWORK_BASELINE:-integracao/2.1}:src/rede.c" > "$work/baseline.c"
if TIZEN_NETWORK_SOURCE="$work/baseline.c" node tests/tizen_manual_network.cjs "$work/manual" > "$work/baseline.log" 2>&1; then
  echo "FAIL: integration baseline unexpectedly passed" >&2
  exit 1
fi
if ! rg -q 'ReferenceError: AbortController is not a constructor|TypeError: AbortController is not a constructor|ReferenceError: AbortController is not defined' "$work/baseline.log"; then
  echo "FAIL: baseline failed for an unexpected reason" >&2
  exit 1
fi
echo "PASS: integration baseline fails on Chromium 63 AbortController"
node tests/tizen_manual_network.cjs "$work/manual"
