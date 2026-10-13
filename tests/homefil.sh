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
