#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-fontesmortas.XXXXXX")
trap 'rm -rf "$dir"' EXIT
flags=(-O1 -g -Isrc -pthread -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
       -Wall -Wextra -ffunction-sections -fdata-sections -Wl,-dead_strip)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" tests/fontesmortas.c src/fontesmortas.c src/fontefalha.c src/fontefiltro.c src/linguas.c -o "$dir/teste"
if [ "$#" -gt 0 ] && [ "$1" != retomar ]; then "$dir/teste" "$@"; else
  if [ "$#" -eq 0 ]; then
  "$dir/teste"
  for caso in concorrencia ordem identidade; do "$dir/teste" "$caso"; done
  fi
  cc "${flags[@]}" -Wno-deprecated-declarations tests/fontesmortas_torrent.c \
    src/fontesmortas.c src/fontevolta.c src/fontefalha.c src/fontefiltro.c src/linguas.c \
    src/fontesonda.c src/perfis.c src/sessao.c src/debrid.c -o "$dir/torrent"
  if [ "$#" -gt 0 ]; then "$dir/torrent" "$@"; else
    "$dir/torrent"
    "$dir/torrent" retomar
  fi
fi
