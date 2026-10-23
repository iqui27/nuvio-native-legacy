#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-catconsistencia.XXXXXX")
trap 'rm -rf "$dir"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} -DNV_CAT_TEST_ANTES_TRAVA=nv_cat_teste_antes_trava \
  src/catalogo.c tests/catconsistencia.c tests/catalogo_boundary_stub.c -Isrc -o "$dir/teste" \
  -O1 -g -Wall -Wextra -lpthread
"$dir/teste"
