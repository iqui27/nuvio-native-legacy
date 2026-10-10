#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
flags=(-O1 -g -std=gnu11 -DFIL_TESTE -Isrc -pthread -Wall -Wextra -Wno-misleading-indentation)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
bin="$(mktemp "${TMPDIR:-/tmp}/nuvio-ordemperfil.XXXXXXXX")"
dir="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ordemperfil-dir.XXXXXXXX")"
trap 'rm -rf "$dir" "$bin"' EXIT
cc "${flags[@]}" src/fileiras.c tests/ordemperfil.c -o "$bin"
NV_T_DIR="$dir" "$bin"
