#!/bin/bash
# #223: sessao pela metade nao deixa o arranque em branco; falha de login nao
# grava sessao e mostra o codigo da libcurl. Ver tests/sessao_parcial.c. Sem rede.
set -eu
cd "$(dirname "$0")/.."
flags=(-O1 -g -Isrc -pthread -Wall -Wno-deprecated-declarations -Wno-macro-redefined)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
dir="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-sessao-parcial.XXXXXX")"
trap 'rm -rf "$dir"' EXIT
cc "${flags[@]}" src/sessao.c src/js.c src/jsw.c tests/sessao_parcial.c tests/sessao_boundary_stub.c -o "$dir/t"
NV_T_DIR="$dir" "$dir/t" || { echo "sessao_parcial.sh: FALHOU"; exit 1; }
echo "sessao_parcial.sh: ok"
