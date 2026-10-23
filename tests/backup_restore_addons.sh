#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
task_dir="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-restore-addons.XXXXXX")"
trap 'rm -rf "$task_dir"' EXIT
export NUVIO_DADOS="$task_dir/data"
mkdir -p "$NUVIO_DADOS"
flags=(-std=gnu11 -O0 -g -DNV_TOUCH_UI -Isrc -Wno-deprecated-declarations)
if [[ "$(uname -s)" == Darwin ]]; then
  flags+=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib)
  libs=(-lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL)
else
  read -r -a sdk_flags <<< "$(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)"
  read -r -a sdk_libs <<< "$(pkg-config --libs sdl2 SDL2_image SDL2_ttf)"
  flags+=(-DNV_LINUX_DESKTOP "${sdk_flags[@]}")
  libs=("${sdk_libs[@]}" -lz -lGLESv2 -pthread -lm -ldl)
fi
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
if [ -n "${NV_BACKUP_APP_BASELINE:-}" ]; then
  flags+=("-DBACKUP_RESTORE_APP=\"$NV_BACKUP_APP_BASELINE\"")
fi
# Keep discovery's lifecycle real but prevent its asynchronous network start.
cc "${flags[@]}" -Ddesc_sessao_preparada=fixture_desc_sessao_preparada -c src/descoberta.c -o "$task_dir/descoberta.o"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/app.c|src/descoberta.c) continue;; esac
  sources+=("$source")
done
cc "${flags[@]}" "${sources[@]}" "$task_dir/descoberta.o" tests/backup_restore_addons.c -o "$task_dir/test" "${libs[@]}"
"$task_dir/test"
