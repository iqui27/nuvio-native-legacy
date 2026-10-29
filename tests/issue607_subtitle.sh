#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/issue607-subtitle.XXXXXX")
trap 'rm -rf "$work"' EXIT
# TPK40's manual loader cannot initialize compiler TLS. Inspect compiler output
# so this regression runs on the host without Docker or a packaged TV build.
cc tests/issue607_subtitle.c -DNV_TPK40 -S -o "$work/subtitle.s" \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -std=gnu11 -pthread
python3 - "$work/subtitle.s" <<'PY'
import re
import sys
from pathlib import Path
assembly = Path(sys.argv[1]).read_text()
assert not re.search(r'__thread_vars|__thread_bss|__thread_data|\.tdata|\.tbss|\.tls\$', assembly), \
    'TPK40 subtitle module emits compiler TLS'
print('PASS: TPK40 subtitle module emits no compiler TLS')
PY
cc tests/issue607_subtitle.c tests/addons_legendas_prazo.c src/linguas.c src/js.c src/legextras.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -std=gnu11 -pthread -ffunction-sections -fdata-sections -Wl,-dead_strip \
  -Wno-macro-redefined -Wno-return-type -o "$work/test"
"$work/test"
