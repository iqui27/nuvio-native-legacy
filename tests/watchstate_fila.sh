#!/bin/bash
# watch_state v2 push, passo 2 (corpo, id, URL, fila e retry com HTTP falso).
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ws-fila.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc tests/watchstate_fila.c src/watchstate.c src/js.c src/jsw.c -Isrc -std=gnu11 -Wall -Wextra \
  -Wno-unused-function -Wno-macro-redefined -o "$work/t"
"$work/t"
