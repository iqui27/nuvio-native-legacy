#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
bin="$(mktemp "${TMPDIR:-/tmp}/nuvio-homefil.XXXXXXXX")"
trap 'rm -f "$bin"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
# Perfil real: o teste precisa provar a recarga pela API usada pelo app.
cc ${flags[@]+"${flags[@]}"} -O1 -g -ftrivial-auto-var-init=pattern -Isrc -Wno-deprecated-declarations \
  src/homefil.c src/perfis.c src/js.c src/jsw.c tests/homefil.c -o "$bin"
"$bin" empty
"$bin" profile
"$bin"
# Same-profile account change through the real centralized logout, without network.
cc ${flags[@]+"${flags[@]}"} -O1 -g -Isrc -pthread \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations \
  src/sync.c tests/stub_contapend.c src/catordem.c src/catordemcache.c src/contacache.c \
  src/homefil.c src/js.c src/jsw.c tests/homefil_logout.c -o "$bin"
dir="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-homefil-logout.XXXXXXXX")"
trap 'rm -rf "$dir" "$bin"' EXIT
NV_T_DIR="$dir" "$bin"
