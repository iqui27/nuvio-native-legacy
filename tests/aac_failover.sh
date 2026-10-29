#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-aac-failover.XXXXXX")
trap 'rm -rf "$work"' EXIT
python3 - "$work" <<'PY'
from pathlib import Path
import sys
s = Path('src/app.c').read_text()
start = s.index('  if (aguardandoFonte == 2 && fonteEscolhida != -2) {')
end = s.index('\n  // Fonte de canal que nao abre', start)
Path(sys.argv[1], 'consumer.c').write_text('static void consume(void) {\n' + s[start:end] + '\n}\n')
PY
cc -std=c11 -Wall -Wextra -Wno-unused-parameter -I"$work" tests/aac_failover.c -o "$work/check"
"$work/check"
python3 tests/android_decoder409.py
