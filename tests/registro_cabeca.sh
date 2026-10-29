#!/bin/bash
# Exercise the native upload reader without SDL or network access.
set -eu
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-log-header-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
python3 - "$tmp/upload.inc" <<'PY'
from pathlib import Path
import sys
source = Path('src/avisos.c').read_text()
start = source.index('  { const char *arq = manual ? registro_arquivo()')
end = source.index('\n#endif', start)
Path(sys.argv[1]).write_text(source[start:end])
PY
cc -O1 -g -Wall -Wextra -I"$tmp" tests/registro_cabeca.c -o "$tmp/test"
"$tmp/test" "$tmp/nuvio.log"
