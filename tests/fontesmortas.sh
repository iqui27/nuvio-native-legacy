#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
if [ "${1:-}" = p2p ]; then exec bash tests/fontesmortas_p2p_integracao.sh; fi
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-fontesmortas.XXXXXX")
trap 'rm -rf "$dir"' EXIT
flags=(-O1 -g -Isrc -pthread -Wall -Wextra -ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then
  flags+=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2)
  linker=(-Wl,-dead_strip)
else
  flags+=(-DNV_LINUX_DESKTOP)
  linker=(-Wl,--gc-sections -lm)
  if pkg-config --exists sdl2; then
    read -r -a sdk <<< "$(pkg-config --cflags sdl2)"
    flags+=("${sdk[@]}")
  fi
fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
if [ "${1:-}" != escolhida ]; then
  cc "${flags[@]}" tests/fontesmortas.c src/fontesmortas.c src/fontefalha.c src/fontefiltro.c src/linguas.c \
    src/stream_parse.c src/js.c src/badges.c src/streamfit.c src/naovideo.c \
    "${linker[@]}" -o "$dir/teste"
fi
if [ "$#" -gt 0 ] && [ "$1" != retomar ] && [ "$1" != escolhida ]; then "$dir/teste" "$@"; else
  if [ "$#" -eq 0 ]; then
  "$dir/teste"
  for caso in concorrencia ordem identidade; do "$dir/teste" "$caso"; done
  fi
  cc "${flags[@]}" -Wno-deprecated-declarations tests/fontesmortas_torrent.c \
    src/fontesmortas.c src/fontevolta.c src/fontefalha.c src/fontefiltro.c src/linguas.c \
    src/fontesonda.c src/farolanel.c src/debrid.c src/audioescolha.c src/partescartao.c \
    "${linker[@]}" -o "$dir/torrent"
  if [ "$#" -gt 0 ]; then "$dir/torrent" "$@"; else
    "$dir/torrent"
    "$dir/torrent" retomar
    "$dir/torrent" escolhida
    "$dir/torrent" escolhida prazo
    bash tests/fontesmortas_p2p_integracao.sh
  fi
fi
