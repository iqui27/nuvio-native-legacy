#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
flags=(-O1 -g -Isrc -pthread -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wall -Wno-deprecated-declarations -Wno-macro-redefined -Wno-misleading-indentation)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
bin="$(mktemp "${TMPDIR:-/tmp}/nuvio-synccolecoes.XXXXXXXX")"
dir="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-synccolecoes-dir.XXXXXXXX")"
trap 'rm -rf "$dir" "$bin"' EXIT
cc "${flags[@]}" src/sync.c tests/stub_contapend.c src/colecoes.c src/colfileiras.c src/fileiras.c src/redeurl.c src/catordem.c src/catordemcache.c src/contacache.c src/js.c src/jsw.c tests/synccolecoes.c -o "$bin"
for case in network copy fallback isolated no-copy; do
  export NV_T_DIR="$dir/$case"; mkdir "$NV_T_DIR"
  "$bin" "$case"
done
