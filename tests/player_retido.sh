#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-player-retido.XXXXXX")
trap 'rm -rf "$dir"' EXIT
export NUVIO_DADOS="$dir/dados"
mkdir -p "$NUVIO_DADOS"
flags=(-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
  -Wno-deprecated-declarations -Wno-macro-redefined)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
renomes=()
for nome in tocar parar pausar buscar pausa_confirmada tocando pronto ativo falhou conflito_recurso pos duracao url_atual; do
  renomes+=("-Dvideo_$nome=nv_base_video_$nome")
done
cc "${flags[@]}" "${renomes[@]}" -c src/video.c -o "$dir/video.o"
cc "${flags[@]}" -Dsessao_usuario=nv_test_usuario -c src/player.c -o "$dir/player.o"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/player.c|src/video.c) continue;; esac
  sources+=("$source")
done
cc "${flags[@]}" "${sources[@]}" tests/player_retido.c "$dir/player.o" "$dir/video.o" \
  -o "$dir/test" -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
if [ "${NV_LLDB:-0}" = 1 ]; then
  lldb --batch -o run -k 'bt all' -- "$dir/test"
else
  "$dir/test"
fi
