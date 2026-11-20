#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ondever-lookup.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc tests/ondever_lookup.c src/js.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -std=gnu11 -pthread -Wall -Wextra -Wno-unused-function ${NV_CFLAGS:-} -o "$work/test"
"$work/test"
cc tests/ondever_lookup.c src/js.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -std=gnu11 -pthread -Wall -Wextra -Wno-unused-function -DNV_TPK ${NV_CFLAGS:-} -o "$work/tpk-test"
"$work/tpk-test"
