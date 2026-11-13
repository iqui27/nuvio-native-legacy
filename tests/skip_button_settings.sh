#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
task_dir=$(mktemp -d);trap 'rm -rf "$task_dir"' EXIT
flags=(-std=gnu11 -O1 -g -ffunction-sections -fdata-sections -Isrc)
if [[ $(uname -s) == Darwin ]]; then
  flags+=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wl,-dead_strip)
else
  read -r -a sdk_flags <<< "$(pkg-config --cflags sdl2)"
  flags+=(-DNV_LINUX_DESKTOP "${sdk_flags[@]}" -Wl,--gc-sections)
fi
if [[ ${SANITIZE:-0} == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" tests/skip_button_settings.c src/linguas.c -lm -pthread -o "$task_dir/test"
for mask in {0..7}; do
  "$task_dir/test" write "$task_dir" "$mask"
  "$task_dir/test" read "$task_dir" "$mask"
done
