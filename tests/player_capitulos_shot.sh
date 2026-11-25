#!/bin/bash
# The chapter row and the seek strip, drawn by the real player (GL window).
# Not in the suite (*_shot.sh).   bash tests/player_capitulos_shot.sh <dir>
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-player-caps-shot.XXXXXX")
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/player.c) continue;; esac
  sources+=("$source")
done
if [ "$(uname -s)" = Darwin ]; then
  flags=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2); libs=(-L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL); run=()
else
  read -r -a flags <<< "-DNV_LINUX_DESKTOP $(pkg-config --cflags sdl2 SDL2_image SDL2_ttf glesv2 egl zlib)"
  read -r -a libs <<< "$(pkg-config --libs sdl2 SDL2_image SDL2_ttf glesv2 egl zlib) -ldl -pthread -lm"
  run=(); [ -z "${DISPLAY:-}" ] && command -v xvfb-run >/dev/null && run=(xvfb-run -a -s "-screen 0 1920x1080x24")
fi
cc -DNV_SHOT_HOOKS "${flags[@]}" "${sources[@]}" tests/player_capitulos_shot.c -Isrc -o "$work/shot" -O1 \
  "${libs[@]}" -Wno-deprecated-declarations -Wno-macro-redefined
mkdir -p "$1" "$work/dados"
NUVIO_SHOT_VIDRO=0 NUVIO_DADOS="$work/dados" NUVIO_TESTE_DIR="$work/dados" ${run[@]+"${run[@]}"} "$work/shot" "$1"
