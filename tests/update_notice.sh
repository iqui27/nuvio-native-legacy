#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
task_dir=$(mktemp -d);trap 'rm -rf "$task_dir"' EXIT
flags=(-std=gnu11 -O1 -g -Isrc -ffunction-sections -fdata-sections)
if [[ $(uname -s) == Darwin ]]; then
  flags+=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wl,-dead_strip)
else
  read -r -a sdk_flags <<< "$(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)"
  flags+=(-DNV_LINUX_DESKTOP "${sdk_flags[@]}" -Wl,--gc-sections)
fi
if [[ ${SANITIZE:-0} == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc tests/update_notice.c "${flags[@]}" -o "$task_dir/test" -lm -pthread
"$task_dir/test"
