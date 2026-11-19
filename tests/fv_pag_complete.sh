#!/bin/bash
# Issue #789 Phase-2: paginated addon catalogs consumed whole (14k / 7.6k),
# linear dedup, bounded memory.  bash tests/fv_pag_complete.sh
set -euo pipefail
cd "$(dirname "$0")/.."
out="${TMPDIR:-/tmp}"
flags=(-O1 -g -ffunction-sections -fdata-sections -Wl,-dead_strip -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-macro-redefined -Wno-deprecated-declarations)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/cwordem.c tests/fv_pag_complete.c src/js.c src/metaprov.c -o "$out/fv-pag-complete-tests"
"$out/fv-pag-complete-tests"
