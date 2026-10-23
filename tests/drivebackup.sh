#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/drivebackup.c) continue;; esac
  sources+=("$source")
done
task_dir="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-drive-modal.XXXXXX")"
trap 'rm -rf "$task_dir"' EXIT
export NUVIO_DADOS="$task_dir"
flags=(-DNV_BACKUP_MBEDTLS -DAJUSTES_TESTE -std=gnu11 -O0 -g -DNV_TOUCH_UI -Isrc)
if [[ "$(uname -s)" == Darwin ]]; then
  flags+=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib)
  libs=(-lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL)
else
  read -r -a sdk_flags <<< "$(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)"
  read -r -a sdk_libs <<< "$(pkg-config --libs sdl2 SDL2_image SDL2_ttf)"
  flags+=(-DNV_LINUX_DESKTOP "${sdk_flags[@]}")
  libs=("${sdk_libs[@]}" -lz -lGLESv2 -pthread -lm -ldl)
fi
if [ -n "${MBEDTLS_INCLUDE:-}" ]; then flags+=("-I$MBEDTLS_INCLUDE"); fi
if [[ "$(uname -s)" == Darwin ]]; then
  prefix="${MBEDTLS_PREFIX:-$(brew --prefix mbedtls@3)}";flags+=("-I$prefix/include" "-L$prefix/lib")
fi
if [ -n "${MBEDTLS_LIBRARY:-}" ]; then libs+=("$MBEDTLS_LIBRARY"); else libs+=(-lmbedcrypto);fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer);fi
cc "${sources[@]}" tests/drivebackup.c "${flags[@]}" -o "$task_dir/teste" "${libs[@]}" -Wno-deprecated-declarations
"$task_dir/teste"
