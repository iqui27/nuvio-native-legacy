#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-p2p-784.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
       -Wno-deprecated-declarations -Wno-macro-redefined)
cc "${flags[@]}" -Dp2p_resolver=fixture_p2p_resolver -Dp2p_ativo=fixture_p2p_ativo \
  -Ddebrid_ativo=fixture_debrid_ativo -c src/streams.c -o "$work/streams.o"
cc "${flags[@]}" -Dvideo_tocar=fixture_video_tocar -c src/player.c -o "$work/player.o"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/app.c|src/streams.c|src/player.c) continue;; esac
  sources+=("$source")
done
cc "${flags[@]}" "${sources[@]}" "$work/streams.o" "$work/player.o" tests/fontesmortas_p2p_integracao.c \
  -o "$work/test" -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
mkdir "$work/data"
if [ "$(uname -s)" = Darwin ]; then
  NUVIO_DADOS="$work/data" sandbox-exec -p '(version 1)(allow default)(deny network*)' "$work/test"
else
  # Sem sandbox-exec fora do macOS: o harness ja isola a rede (loopback).
  NUVIO_DADOS="$work/data" "$work/test"
fi
