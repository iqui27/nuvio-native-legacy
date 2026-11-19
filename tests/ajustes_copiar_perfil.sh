#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
export TMPDIR="${TMPDIR:-/tmp}"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
flags=(-std=gnu11 -O1 -g -DNV_TOUCH_UI -DAJUSTES_TESTE -Isrc)
if [ "$(uname -s)" = Darwin ]; then
  flags+=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib)
  libs=(-lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL)
else
  pkg-config --exists sdl2 SDL2_image SDL2_ttf
  flags+=(-DNV_LINUX_DESKTOP)
  read -r -a sdk_flags <<< "$(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)"
  read -r -a sdk_libs <<< "$(pkg-config --libs sdl2 SDL2_image SDL2_ttf)"
  flags+=("${sdk_flags[@]}")
  libs=("${sdk_libs[@]}" -lz -lGLESv2 -pthread -lm -ldl)
fi
cc "${sources[@]}" tests/ajustes_copiar_perfil.c "${flags[@]}" \
  -o "${TMPDIR%/}/nuvio-copiar-perfil" "${libs[@]}" -Wno-deprecated-declarations -Wno-macro-redefined
"${TMPDIR%/}/nuvio-copiar-perfil"
