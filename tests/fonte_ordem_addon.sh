#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ordem400.XXXXXX")
trap 'rm -rf "$dir"' EXIT
flags=(-O1 -g -Isrc -pthread -ffunction-sections -fdata-sections)
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
# O teste inclui src/streams.c para observar a ordem realmente montada.
cc "${flags[@]}" tests/fonte_ordem_addon.c src/fontesmortas.c src/stream_parse.c src/fontefiltro.c src/fontefalha.c src/fontenota.c src/linguas.c src/js.c src/badges.c src/streamfit.c src/naovideo.c "${linker[@]}" -o "$dir/test"
"$dir/test"
