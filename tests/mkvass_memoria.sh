#!/bin/bash
# No network or SDL: compile the real memory callback with the subtitle reader.
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-mkvass-memory.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
python3 - "$DIR/memory_callback.h" <<'PY'
import pathlib
import re
import sys
source = pathlib.Path('src/app.c').read_text()
callback = re.search(r'static void aliviarMemoria\(void\) \{.*?^\}', source, re.M | re.S)
assert callback, 'memory callback missing'
pathlib.Path(sys.argv[1]).write_text(callback[0] + '\n')
PY
cc tests/mkvass_memoria.c src/legenda.c tests/mkvass_i18n.c src/imgsub.c src/pgs.c src/assrender.c src/dados.c \
  -Isrc -I"$DIR" -O1 -g -Wno-deprecated-declarations -pthread -lm -o "$DIR/test"
"$DIR/test"
