#!/bin/bash
# watch_state v2 push, passo 1 (manifesto + filtro). Ver tests/watchstate_manifesto.c.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ws-man.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc tests/watchstate_manifesto.c src/watchstate.c src/js.c src/jsw.c -Isrc -std=gnu11 -Wall -Wextra \
  -Wno-unused-function -Wno-macro-redefined -o "$work/t"
"$work/t"
