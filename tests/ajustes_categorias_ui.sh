#!/bin/bash
# Real Settings renderer and pointer dispatch in a hidden GL window; no network requests.
set -euo pipefail
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
task_dir="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ajustes-iptv.XXXXXX")"
trap 'rm -rf "$task_dir"' EXIT
export NUVIO_DADOS="$task_dir"
flags=(-std=gnu11 -O0 -g -DNV_TOUCH_UI -DAJUSTES_TESTE -Isrc)
if [[ "$(uname -s)" == Darwin ]]; then
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
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${sources[@]}" tests/ajustes_categorias_ui.c "${flags[@]}" -o "$task_dir/teste" \
  "${libs[@]}" -Wno-deprecated-declarations
if [ -n "${NUVIO_CATEGORY_BINARY:-}" ]; then cp "$task_dir/teste" "$NUVIO_CATEGORY_BINARY"; fi
"$task_dir/teste" "${NUVIO_CATEGORY_IMAGES:-$task_dir}"
