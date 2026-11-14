#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
flags=(-O1 -g -std=gnu11 -DFIL_TESTE -Isrc -pthread -Wall -Wextra -Wno-misleading-indentation)
# ajustes.h (via fileiras.c) inclui <SDL2/SDL.h>: so o cabecalho, sem link.
read -r -a sdl <<< "$(pkg-config --cflags sdl2)"
flags+=("${sdl[@]}")
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
bin="$(mktemp "${TMPDIR:-/tmp}/nuvio-colfileiras-sync.XXXXXXXX")"
dir="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-colfileiras-sync-dir.XXXXXXXX")"
trap 'rm -rf "$dir" "$bin"' EXIT
cc "${flags[@]}" src/colfileiras.c src/colecoes.c src/fileiras.c src/catordem.c src/js.c src/redeurl.c tests/colfileiras_sync.c -o "$bin"
NV_T_DIR="$dir" "$bin"
